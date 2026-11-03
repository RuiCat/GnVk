// tests/cornellBox/cornellPathTracer.cpp — CPU 路径追踪参考实现
//
// 职责：解析求交（四边形 + 球）、面光 NEE、余弦漫反射、镜面金属、俄罗斯轮盘、多线程、
//       ACES 色调映射与 PNG 写出。纯 CPU，不依赖 Vulkan；用于给引擎 GI 提供 ground truth。
#include "cornellPathTracer.h"

#include <png.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <csetjmp>
#include <thread>
#include <vector>

namespace Cornell {

namespace {

const float PI = 3.14159265359f;
const float EPS = 1.0e-4f;
const int MAX_BOUNCES = 4;
const float SUN_INTENSITY = 3.0f;

struct sRay {
    sVec3 aOrigin;
    sVec3 aDir;
};

struct sHit {
    float aT;
    sVec3 aNormal;
    int aMaterial;
    bool aValid;
};

struct sRng {
    unsigned int aState;
    explicit sRng(unsigned int seed) : aState(seed ? seed : 1u) {}
    float next() {
        aState ^= aState << 13;
        aState ^= aState >> 17;
        aState ^= aState << 5;
        return static_cast<float>(aState & 0x00FFFFFFu) / 16777216.0f;
    }
};

sVec3 makeVec3(float x, float y, float z) {
    sVec3 v;
    v.x = x;
    v.y = y;
    v.z = z;
    return v;
}

sVec3 add(const sVec3 &a, const sVec3 &b) {
    return makeVec3(a.x + b.x, a.y + b.y, a.z + b.z);
}

sVec3 sub(const sVec3 &a, const sVec3 &b) {
    return makeVec3(a.x - b.x, a.y - b.y, a.z - b.z);
}

sVec3 mul(const sVec3 &a, float s) {
    return makeVec3(a.x * s, a.y * s, a.z * s);
}

sVec3 mul(const sVec3 &a, const sVec3 &b) {
    return makeVec3(a.x * b.x, a.y * b.y, a.z * b.z);
}

float dot(const sVec3 &a, const sVec3 &b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

sVec3 cross(const sVec3 &a, const sVec3 &b) {
    return makeVec3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z,
                    a.x * b.y - a.y * b.x);
}

float length(const sVec3 &a) {
    return std::sqrt(dot(a, a));
}

sVec3 normalize(const sVec3 &a) {
    const float l = length(a);
    return l > 1.0e-8f ? mul(a, 1.0f / l) : makeVec3(0.0f, 0.0f, 0.0f);
}

sVec3 reflect(const sVec3 &d, const sVec3 &n) {
    return sub(d, mul(n, 2.0f * dot(d, n)));
}

bool intersectQuad(const sQuad &quad, const sRay &ray, float tMax, sHit *hit) {
    const float denom = dot(quad.aNormal, ray.aDir);
    if (std::fabs(denom) < 1.0e-7f) return false;
    const float t = dot(quad.aNormal, sub(quad.aP0, ray.aOrigin)) / denom;
    if (t <= EPS || t >= tMax) return false;
    const sVec3 p = add(ray.aOrigin, mul(ray.aDir, t));
    const sVec3 rel = sub(p, quad.aP0);
    const float uu = dot(quad.aU, quad.aU);
    const float vv = dot(quad.aV, quad.aV);
    if (uu < 1.0e-8f || vv < 1.0e-8f) return false;
    const float a = dot(rel, quad.aU) / uu;
    const float b = dot(rel, quad.aV) / vv;
    if (a < 0.0f || a > 1.0f || b < 0.0f || b > 1.0f) return false;
    hit->aT = t;
    hit->aNormal = dot(quad.aNormal, ray.aDir) > 0.0f ? mul(quad.aNormal, -1.0f)
                                                       : quad.aNormal;
    hit->aMaterial = quad.aMaterial;
    hit->aValid = true;
    return true;
}

bool intersectSphere(const sSphere &sphere, const sRay &ray, float tMax, sHit *hit) {
    const sVec3 oc = sub(ray.aOrigin, sphere.aCenter);
    const float b = dot(oc, ray.aDir);
    const float c = dot(oc, oc) - sphere.aRadius * sphere.aRadius;
    const float disc = b * b - c;
    if (disc < 0.0f) return false;
    const float root = std::sqrt(disc);
    float t = -b - root;
    if (t <= EPS) t = -b + root;
    if (t <= EPS || t >= tMax) return false;
    const sVec3 p = add(ray.aOrigin, mul(ray.aDir, t));
    sVec3 normal = normalize(sub(p, sphere.aCenter));
    if (dot(normal, ray.aDir) > 0.0f) normal = mul(normal, -1.0f);
    hit->aT = t;
    hit->aNormal = normal;
    hit->aMaterial = sphere.aMaterial;
    hit->aValid = true;
    return true;
}

sHit trace(const sCornellGeometry &geometry, const sRay &ray, float tMax) {
    sHit best;
    best.aValid = false;
    best.aT = tMax;
    sHit current;
    current.aValid = false;
    for (size_t i = 0; i < geometry.aQuads.size(); ++i) {
        if (intersectQuad(geometry.aQuads[i], ray, best.aT, &current) && current.aValid) {
            best = current;
            current.aValid = false;
        }
    }
    for (size_t i = 0; i < geometry.aSpheres.size(); ++i) {
        if (intersectSphere(geometry.aSpheres[i], ray, best.aT, &current) && current.aValid) {
            best = current;
            current.aValid = false;
        }
    }
    return best;
}

bool occluded(const sCornellGeometry &geometry, const sVec3 &origin, const sVec3 &dir,
              float distance) {
    sRay ray;
    ray.aOrigin = origin;
    ray.aDir = dir;
    const sHit hit = trace(geometry, ray, distance - EPS);
    // 命中面光本身不算遮挡；其他几何在阴影射线上即遮挡。
    return hit.aValid && hit.aMaterial != eMAT_LIGHT;
}

sVec3 cosineSample(const sVec3 &normal, sRng &rng) {
    const float r1 = rng.next();
    const float r2 = rng.next();
    const float phi = 2.0f * PI * r2;
    const float radius = std::sqrt(r1);
    const float z = std::sqrt(std::max(0.0f, 1.0f - r1));
    const sVec3 up = std::fabs(normal.y) < 0.99f ? makeVec3(0.0f, 1.0f, 0.0f)
                                                 : makeVec3(1.0f, 0.0f, 0.0f);
    const sVec3 tangent = normalize(cross(up, normal));
    const sVec3 bitangent = cross(normal, tangent);
    return normalize(add(add(mul(tangent, std::cos(phi) * radius),
                            mul(bitangent, std::sin(phi) * radius)),
                         mul(normal, z)));
}

sVec3 directLight(const sCornellGeometry &geometry, const sMaterial &material,
                  const sVec3 &p, const sVec3 &normal, sRng &rng) {
    if (geometry.aLightQuad < 0) {
        // 指向光源的方向 = -SUN_DIRECTION（SUN_DIRECTION 是传播方向）。
        const sVec3 toLight = mul(SUN_DIRECTION, -1.0f);
        const float cosSurface = dot(normal, toLight);
        if (cosSurface <= 0.0f) return makeVec3(0.0f, 0.0f, 0.0f);
        const float scale = cosSurface / PI * SUN_INTENSITY;
        return makeVec3(material.aAlbedo[0] * scale, material.aAlbedo[1] * scale,
                        material.aAlbedo[2] * scale);
    }

    const sQuad &light = geometry.aQuads[geometry.aLightQuad];
    const sVec3 point = add(add(light.aP0, mul(light.aU, rng.next())),
                            mul(light.aV, rng.next()));
    const sVec3 toLight = sub(point, p);
    const float distance = length(toLight);
    if (distance <= EPS) return makeVec3(0.0f, 0.0f, 0.0f);
    const sVec3 wi = mul(toLight, 1.0f / distance);
    const float cosSurface = dot(normal, wi);
    const float cosLight = dot(light.aNormal, mul(wi, -1.0f));
    if (cosSurface <= 0.0f || cosLight <= 0.0f) return makeVec3(0.0f, 0.0f, 0.0f);
    const sVec3 shadowOrigin = add(p, mul(normal, EPS * 10.0f));
    if (occluded(geometry, shadowOrigin, wi, distance)) return makeVec3(0.0f, 0.0f, 0.0f);

    const sVec3 areaVec = cross(light.aU, light.aV);
    const float area = length(areaVec);
    const float geometryTerm = cosSurface * cosLight / (distance * distance);
    const float scale = geometryTerm * area / PI;
    const sMaterial &lightMaterial = geometry.aMaterials[light.aMaterial];
    return makeVec3(lightMaterial.aEmission[0] * material.aAlbedo[0] * scale,
                    lightMaterial.aEmission[1] * material.aAlbedo[1] * scale,
                    lightMaterial.aEmission[2] * material.aAlbedo[2] * scale);
}

sVec3 radiance(const sCornellGeometry &geometry, const sRay &primary, sRng &rng) {
    sVec3 result = makeVec3(0.0f, 0.0f, 0.0f);
    sVec3 throughput = makeVec3(1.0f, 1.0f, 1.0f);
    sRay ray = primary;
    bool specular = false;

    for (int bounce = 0; bounce <= MAX_BOUNCES; ++bounce) {
        const sHit hit = trace(geometry, ray, 1.0e30f);
        if (!hit.aValid) break;
        const sMaterial &material = geometry.aMaterials[hit.aMaterial];
        if (material.aEmission[0] + material.aEmission[1] + material.aEmission[2] > 0.0f) {
            if (bounce == 0 || specular) {
                result = add(result, mul(throughput, makeVec3(material.aEmission[0],
                                                              material.aEmission[1],
                                                              material.aEmission[2])));
            }
            break;
        }

        if (!specular) {
            result = add(result, mul(throughput, directLight(geometry, material,
                                                             add(ray.aOrigin, mul(ray.aDir, hit.aT)),
                                                             hit.aNormal, rng)));
        }

        sVec3 nextDir;
        if (material.aMetallic > 0.5f) {
            nextDir = reflect(ray.aDir, hit.aNormal);
            specular = true;
        } else {
            nextDir = cosineSample(hit.aNormal, rng);
            specular = false;
        }
        throughput = mul(throughput, makeVec3(material.aAlbedo[0], material.aAlbedo[1],
                                              material.aAlbedo[2]));
        const sVec3 origin = add(add(ray.aOrigin, mul(ray.aDir, hit.aT)),
                                 mul(hit.aNormal, EPS * 10.0f));
        ray.aOrigin = origin;
        ray.aDir = nextDir;

        if (bounce >= 2) {
            const float survive = 0.85f;
            if (rng.next() > survive) break;
            throughput = mul(throughput, 1.0f / survive);
        }
        if (dot(throughput, throughput) < 1.0e-6f) break;
    }
    return result;
}

unsigned int pixelSeed(unsigned int seed, int x, int y) {
    unsigned int h = seed ^ (static_cast<unsigned int>(x) * 1973u) ^
                     (static_cast<unsigned int>(y) * 9277u);
    h ^= h >> 16;
    h *= 0x7feb352du;
    h ^= h >> 15;
    return h;
}

} // namespace

sPtOptions::sPtOptions()
    : aWidth(640), aHeight(400), aSamples(64), aPose(0), aAreaLight(true), aSeed(1u) {}

bool renderPathTracer(const sCornellGeometry &geometry, const sPtOptions &options,
                      std::vector<float> *outLinearRgb) {
    if (!outLinearRgb || options.aWidth <= 0 || options.aHeight <= 0 ||
        options.aSamples <= 0) {
        return false;
    }
    const int width = options.aWidth;
    const int height = options.aHeight;
    outLinearRgb->assign(static_cast<size_t>(width) * height * 3u, 0.0f);

    const sPose &pose = CORNELL_POSES[(options.aPose >= 0 && options.aPose < 4)
                                          ? options.aPose : 0];
    const sVec3 eye = pose.aPosition;
    const sVec3 forward = normalize(sub(pose.aFocus, eye));
    const sVec3 worldUp = makeVec3(0.0f, 1.0f, 0.0f);
    const sVec3 right = normalize(cross(forward, worldUp));
    const sVec3 up = cross(right, forward);
    const float tanHalf = std::tan(22.5f * PI / 180.0f); // FOV 45°
    const float aspect = static_cast<float>(width) / static_cast<float>(height);

    unsigned int threadCount = std::thread::hardware_concurrency();
    if (threadCount == 0) threadCount = 4;
    if (threadCount > 16) threadCount = 16;
    std::vector<std::thread> workers;
    workers.reserve(threadCount);
    for (unsigned int t = 0; t < threadCount; ++t) {
        workers.push_back(std::thread([&, t]() {
            for (int y = static_cast<int>(t); y < height; y += static_cast<int>(threadCount)) {
                for (int x = 0; x < width; ++x) {
                    sRng rng(pixelSeed(options.aSeed, x, y));
                    sVec3 sum = makeVec3(0.0f, 0.0f, 0.0f);
                    for (int s = 0; s < options.aSamples; ++s) {
                        const float u = (static_cast<float>(x) + rng.next()) / width;
                        const float v = (static_cast<float>(y) + rng.next()) / height;
                        const float sx = (2.0f * u - 1.0f) * tanHalf * aspect;
                        const float sy = (1.0f - 2.0f * v) * tanHalf;
                        sRay ray;
                        ray.aOrigin = eye;
                        ray.aDir = normalize(add(add(forward, mul(right, sx)), mul(up, sy)));
                        sum = add(sum, radiance(geometry, ray, rng));
                    }
                    const float inv = 1.0f / static_cast<float>(options.aSamples);
                    const size_t offset = (static_cast<size_t>(y) * width + x) * 3u;
                    (*outLinearRgb)[offset + 0] = sum.x * inv;
                    (*outLinearRgb)[offset + 1] = sum.y * inv;
                    (*outLinearRgb)[offset + 2] = sum.z * inv;
                }
            }
        }));
    }
    for (size_t i = 0; i < workers.size(); ++i) workers[i].join();
    return true;
}

bool savePngRgb(const char *path, const std::vector<float> &linearRgb, int width, int height,
                float exposure) {
    if (!path || width <= 0 || height <= 0 ||
        linearRgb.size() < static_cast<size_t>(width) * height * 3u) {
        return false;
    }
    FILE *file = std::fopen(path, "wb");
    if (!file) return false;

    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    if (!png) {
        std::fclose(file);
        return false;
    }
    png_infop info = png_create_info_struct(png);
    if (!info) {
        png_destroy_write_struct(&png, nullptr);
        std::fclose(file);
        return false;
    }
    if (setjmp(png_jmpbuf(png))) {
        png_destroy_write_struct(&png, &info);
        std::fclose(file);
        return false;
    }
    png_init_io(png, file);
    png_set_IHDR(png, info, static_cast<png_uint_32>(width), static_cast<png_uint_32>(height),
                 8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
                 PNG_FILTER_TYPE_DEFAULT);
    png_write_info(png, info);

    std::vector<png_byte> row(static_cast<size_t>(width) * 3u);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const size_t offset = (static_cast<size_t>(y) * width + x) * 3u;
            for (int c = 0; c < 3; ++c) {
                float value = linearRgb[offset + c] * exposure;
                value = (value * (2.51f * value + 0.03f)) /
                        (value * (2.43f * value + 0.59f) + 0.14f); // ACES fit
                if (value < 0.0f) value = 0.0f;
                if (value > 1.0f) value = 1.0f;
                value = std::pow(value, 1.0f / 2.2f);
                row[static_cast<size_t>(x) * 3u + c] =
                    static_cast<png_byte>(value * 255.0f + 0.5f);
            }
        }
        png_write_row(png, row.data());
    }
    png_write_end(png, nullptr);
    png_destroy_write_struct(&png, &info);
    std::fclose(file);
    return true;
}

} // namespace Cornell
