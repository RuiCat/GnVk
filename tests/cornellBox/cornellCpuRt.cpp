// tests/cornellBox/cornellCpuRt.cpp — CPU 软件光追预览
//
// 用途：cornellBox 的 T 键预览。对 Cornell 的 7 个解析体做真实 ray-scene 求交：
//   * 镜面球：反射射线回到场景，显示红/绿墙；
//   * 漫反射面：直接光 + 3 次弹射的路径追踪（8 spp），背光面有天空 fill。
// 有 Monte Carlo 噪声；用于在没有 GPU 光追后端时验证“真光追”效果。
#include "cornellCpuRt.h"

#include "cornellGeometry.h"

#include <algorithm>
#include <cmath>
#include <thread>
#include <vector>

namespace Cornell {

namespace {

const float PI = 3.14159265359f;
const float EPS = 1.0e-4f;
const int PT_SAMPLES = 8;   // 每像素路径数（CPU 预览；越大越平滑越慢）
const int PT_BOUNCES = 3;   // 最多 3 次弹射（含镜面）

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

sVec3 v3(float x, float y, float z) {
    sVec3 v;
    v.x = x;
    v.y = y;
    v.z = z;
    return v;
}

sVec3 add(const sVec3 &a, const sVec3 &b) { return v3(a.x + b.x, a.y + b.y, a.z + b.z); }
sVec3 sub(const sVec3 &a, const sVec3 &b) { return v3(a.x - b.x, a.y - b.y, a.z - b.z); }
sVec3 mul(const sVec3 &a, float s) { return v3(a.x * s, a.y * s, a.z * s); }
sVec3 mul(const sVec3 &a, const sVec3 &b) { return v3(a.x * b.x, a.y * b.y, a.z * b.z); }
float dot(const sVec3 &a, const sVec3 &b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
sVec3 cross(const sVec3 &a, const sVec3 &b) {
    return v3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
}
float length(const sVec3 &a) { return std::sqrt(dot(a, a)); }
sVec3 normalize(const sVec3 &a) {
    const float l = length(a);
    return l > 1e-8f ? mul(a, 1.0f / l) : v3(0.0f, 0.0f, 0.0f);
}

// 开口外的天空（CPU RT 的 miss）：简单的天顶→地平线渐变，替代原先的纯黑。
sVec3 skyColor(const sVec3 &dir) {
    const float t = std::min(1.0f, std::max(0.0f, dir.y * 0.5f + 0.5f));
    const sVec3 horizon = v3(0.62f, 0.70f, 0.82f);
    const sVec3 zenith = v3(0.12f, 0.30f, 0.68f);
    return add(mul(horizon, 1.0f - t), mul(zenith, t));
}

// 余弦加权半球采样（路径追踪的漫反射弹射）。
sVec3 cosineDir(const sVec3 &n, sRng &rng) {
    const float r1 = rng.next();
    const float r2 = rng.next();
    const float phi = 2.0f * PI * r2;
    const float radius = std::sqrt(r1);
    const float z = std::sqrt(std::max(0.0f, 1.0f - r1));
    const sVec3 up = std::fabs(n.y) < 0.99f ? v3(0.0f, 1.0f, 0.0f) : v3(1.0f, 0.0f, 0.0f);
    const sVec3 t = normalize(cross(up, n));
    const sVec3 b = cross(n, t);
    return normalize(add(add(mul(t, std::cos(phi) * radius), mul(b, std::sin(phi) * radius)),
                         mul(n, z)));
}
sVec3 reflect(const sVec3 &d, const sVec3 &n) { return sub(d, mul(n, 2.0f * dot(d, n))); }

bool intersectQuad(const sQuad &q, const sRay &ray, float tMax, sHit *hit) {
    const float denom = dot(q.aNormal, ray.aDir);
    if (std::fabs(denom) < 1e-7f) return false;
    const float t = dot(q.aNormal, sub(q.aP0, ray.aOrigin)) / denom;
    if (t <= EPS || t >= tMax) return false;
    const sVec3 p = add(ray.aOrigin, mul(ray.aDir, t));
    const sVec3 rel = sub(p, q.aP0);
    const float uu = dot(q.aU, q.aU);
    const float vv = dot(q.aV, q.aV);
    if (uu < 1e-8f || vv < 1e-8f) return false;
    const float a = dot(rel, q.aU) / uu;
    const float b = dot(rel, q.aV) / vv;
    if (a < 0.0f || a > 1.0f || b < 0.0f || b > 1.0f) return false;
    hit->aT = t;
    hit->aNormal = dot(q.aNormal, ray.aDir) > 0.0f ? mul(q.aNormal, -1.0f) : q.aNormal;
    hit->aMaterial = q.aMaterial;
    hit->aValid = true;
    return true;
}

bool intersectSphere(const sSphere &s, const sRay &ray, float tMax, sHit *hit) {
    const sVec3 oc = sub(ray.aOrigin, s.aCenter);
    const float b = dot(oc, ray.aDir);
    const float c = dot(oc, oc) - s.aRadius * s.aRadius;
    const float disc = b * b - c;
    if (disc < 0.0f) return false;
    const float root = std::sqrt(disc);
    float t = -b - root;
    if (t <= EPS) t = -b + root;
    if (t <= EPS || t >= tMax) return false;
    const sVec3 p = add(ray.aOrigin, mul(ray.aDir, t));
    sVec3 n = normalize(sub(p, s.aCenter));
    if (dot(n, ray.aDir) > 0.0f) n = mul(n, -1.0f);
    hit->aT = t;
    hit->aNormal = n;
    hit->aMaterial = s.aMaterial;
    hit->aValid = true;
    return true;
}

sHit trace(const sCornellGeometry &g, const sRay &ray, float tMax) {
    sHit best;
    best.aValid = false;
    best.aT = tMax;
    sHit cur;
    cur.aValid = false;
    for (size_t i = 0; i < g.aQuads.size(); ++i) {
        if (intersectQuad(g.aQuads[i], ray, best.aT, &cur) && cur.aValid) {
            best = cur;
            cur.aValid = false;
        }
    }
    for (size_t i = 0; i < g.aSpheres.size(); ++i) {
        if (intersectSphere(g.aSpheres[i], ray, best.aT, &cur) && cur.aValid) {
            best = cur;
            cur.aValid = false;
        }
    }
    return best;
}

bool occluded(const sCornellGeometry &g, const sVec3 &origin, const sVec3 &dir, float maxT) {
    sRay ray;
    ray.aOrigin = origin;
    ray.aDir = dir;
    return trace(g, ray, maxT).aValid;
}

// 命中点的出射辐亮度（直接光；与 PT 的面光/方向光同语义）。
sVec3 shadeDirect(const sCornellGeometry &g, const sHit &hit, const sVec3 &p, int lightMode) {
    const sMaterial &m = g.aMaterials[hit.aMaterial];
    const sVec3 albedo = v3(m.aAlbedo[0], m.aAlbedo[1], m.aAlbedo[2]);
    // 环境填充：没有多次弹射时，背光面必须至少保留天空 fill；否则左墙/天花板/
    // 球背面会纯黑，看起来像“阴影面没有任何间接光”。
    const sVec3 fill = mul(albedo, mul(skyColor(hit.aNormal), 0.22f));
    sVec3 result = v3(0.0f, 0.0f, 0.0f);

    if (lightMode == 0) {
        // 与引擎 sun 对齐：方向光不做阴影（引擎的 sun 也没有 shadow pass）。
        const sVec3 toLight = normalize(mul(SUN_DIRECTION, -1.0f));
        const float ndl = dot(hit.aNormal, toLight);
        if (ndl <= 0.0f) return fill;
        const float scale = ndl / PI * 3.0f; // 与引擎方向光 intensity=3 对齐
        result = mul(albedo, scale);
    } else {
        const sVec3 lightPos = v3(ROOM_WIDTH * 0.5f, ROOM_HEIGHT - 0.2f, ROOM_DEPTH * 0.5f);
        const sVec3 toLight = sub(lightPos, p);
        const float dist = length(toLight);
        if (dist <= EPS) return fill;
        const sVec3 L = mul(toLight, 1.0f / dist);
        const float ndl = dot(hit.aNormal, L);
        if (ndl <= 0.0f) return fill;
        const sVec3 origin = add(p, mul(hit.aNormal, EPS * 10.0f));
        if (occluded(g, origin, L, dist - EPS)) return fill;
        const float range = 9.0f;
        const float att = 1.0f / (1.0f + 2.0f / range * dist + 1.0f / (range * range) * dist * dist);
        const float scale = ndl / PI * 14.0f * att;
        result = mul(albedo, scale);
    }
    return add(result, fill);
}

sVec3 radiance(const sCornellGeometry &g, const sRay &ray, int lightMode, sRng &rng) {
    sVec3 result = v3(0.0f, 0.0f, 0.0f);
    sVec3 throughput = v3(1.0f, 1.0f, 1.0f);
    sRay current = ray;
    for (int bounce = 0; bounce < PT_BOUNCES; ++bounce) {
        const sHit hit = trace(g, current, 1.0e30f);
        if (!hit.aValid) {
            result = add(result, mul(throughput, skyColor(current.aDir)));
            break;
        }
        const sVec3 p = add(current.aOrigin, mul(current.aDir, hit.aT));
        const sMaterial &m = g.aMaterials[hit.aMaterial];
        const sVec3 albedo = v3(m.aAlbedo[0], m.aAlbedo[1], m.aAlbedo[2]);
        if (m.aMetallic > 0.5f) {
            current.aOrigin = add(p, mul(hit.aNormal, EPS * 10.0f));
            current.aDir = normalize(reflect(current.aDir, hit.aNormal));
            throughput = mul(throughput, albedo);
            continue;
        }
        // 直接光 + 天空 fill（shadeDirect 内）。
        result = add(result, mul(throughput, shadeDirect(g, hit, p, lightMode)));
        // 每次漫反射命中做一次余弦弹射：真实间接光（红/绿墙渗色、球体遮挡）。
        current.aOrigin = add(p, mul(hit.aNormal, EPS * 10.0f));
        current.aDir = cosineDir(hit.aNormal, rng);
        throughput = mul(throughput, albedo);
        if (bounce >= 2) {
            const float survive = 0.8f;
            if (rng.next() > survive) break;
            throughput = mul(throughput, 1.0f / survive);
        }
    }
    return result;
}


unsigned char toByte(float value) {
    value = (value * (2.51f * value + 0.03f)) / (value * (2.43f * value + 0.59f) + 0.14f);
    if (value < 0.0f) value = 0.0f;
    if (value > 1.0f) value = 1.0f;
    value = std::pow(value, 1.0f / 2.2f);
    return static_cast<unsigned char>(value * 255.0f + 0.5f);
}

} // namespace

bool renderCpuRt(int width, int height, const float eye[3], const float fwd[3],
                 const float right[3], const float up[3], float fovYDeg, int lightMode,
                 std::vector<unsigned char> *outRgba) {
    if (!outRgba || width <= 0 || height <= 0) return false;
    outRgba->assign(static_cast<size_t>(width) * height * 4u, 255u);

    sCornellGeometry geometry;
    buildCornellGeometry(false, &geometry);

    const sVec3 camera = v3(eye[0], eye[1], eye[2]);
    const sVec3 forward = normalize(v3(fwd[0], fwd[1], fwd[2]));
    const sVec3 cameraRight = normalize(v3(right[0], right[1], right[2]));
    const sVec3 cameraUp = normalize(v3(up[0], up[1], up[2]));
    const float tanHalf = std::tan(fovYDeg * 0.5f * PI / 180.0f);
    const float aspect = static_cast<float>(width) / static_cast<float>(height);

    unsigned int threadCount = std::thread::hardware_concurrency();
    if (threadCount == 0) threadCount = 4;
    if (threadCount > 8) threadCount = 8;
    std::vector<std::thread> workers;
    workers.reserve(threadCount);
    for (unsigned int t = 0; t < threadCount; ++t) {
        workers.push_back(std::thread([&, t]() {
            for (int y = static_cast<int>(t); y < height; y += static_cast<int>(threadCount)) {
                for (int x = 0; x < width; ++x) {
                    const float u = (static_cast<float>(x) + 0.5f) / static_cast<float>(width);
                    const float v = (static_cast<float>(y) + 0.5f) / static_cast<float>(height);
                    sRay ray;
                    ray.aOrigin = camera;
                    ray.aDir = normalize(add(add(forward, mul(cameraRight, (2.0f * u - 1.0f) * tanHalf * aspect)),
                                             mul(cameraUp, (1.0f - 2.0f * v) * tanHalf)));
                    sVec3 sum = v3(0.0f, 0.0f, 0.0f);
                    const unsigned int seed = static_cast<unsigned int>(x * 1973 + y * 9277 + 17);
                    for (int sample = 0; sample < PT_SAMPLES; ++sample) {
                        sRng rng(seed + static_cast<unsigned int>(sample) * 0x9E3779B9u);
                        sum = add(sum, radiance(geometry, ray, lightMode, rng));
                    }
                    const sVec3 c = mul(sum, 1.0f / static_cast<float>(PT_SAMPLES));
                    const size_t o = (static_cast<size_t>(y) * width + x) * 4u;
                    (*outRgba)[o + 0] = toByte(c.x);
                    (*outRgba)[o + 1] = toByte(c.y);
                    (*outRgba)[o + 2] = toByte(c.z);
                    (*outRgba)[o + 3] = 255u;
                }
            }
        }));
    }
    for (size_t i = 0; i < workers.size(); ++i) workers[i].join();
    return true;
}

} // namespace Cornell
