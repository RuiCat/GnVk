// cornellRayLib.glsl — Cornell Box 软件解析光追的共用 GLSL 库（渲染 + 探针更新共享）
//
// 调用方（gn_cornell_rt.comp / gn_cornell_probe_update.comp）先声明自己的输出缓冲，
// 再 #include 本文件；本文件自带 binding 1（场景）、2（RtParams）、11–14（探针）声明。
// 布局契约与 tests/cornellBox/cornellGpuRt.cpp 逐字段一致。
// 探针更新路径额外用 binding 15（ProbeState），由更新 shader 自己声明。
#ifndef CORNELL_RAY_LIB_GLSL
#define CORNELL_RAY_LIB_GLSL

layout(std430, binding = 1) readonly buffer SceneData { vec4 data[]; } sceneData;

layout(std140, binding = 2) uniform RtParams {
    vec4 eye;      // xyz = 相机位置
    vec4 fwd;      // xyz = 前向
    vec4 camRight; // xyz = 右向
    vec4 camUp;    // xyz = 上向
    vec4 cam;      // x = tanHalfFov, y = aspect, z = lightMode(0=sun,1=point), w = sampleCount
    vec4 size;     // x = width, y = height
    vec4 sunDir;   // xyz = 太阳方向（与 CPU 的 SUN_DIRECTION 一致）
    vec4 lightPos; // xyz = 点光位置（与 CPU 一致）
    uvec4 counts;  // x = quadCount, y = sphereCount, z = materialCount
    vec4 accumParams; // x = frameStartSample, y = reset(1=清空累积), z/w 保留
    vec4 prevEye;
    vec4 prevFwd;
    vec4 prevRight;
    vec4 prevUp;
    vec4 temporal; // x=historyValid, y=moving, z=targetSamples
} u;

// 96 B 探针 UBO（前 48 B 与静态路径逐字段一致）与探针 SSBO。
layout(std140, binding = 11) uniform PROBE_UBO {
    vec4 origin;        // xyz = 网格原点
    vec4 spacingCounts; // x=spacing, yzw=counts
    vec4 weight;        // x=强度（0=关）, y=depthRes, z=normalBias, w=viewBias
    vec4 relocation;    // x=maxOffset, y=insideThreshold, z=iterations, w=enable
    vec4 update;        // x=rangeStart, y=rangeCount, z=hysteresis, w=flags
    vec4 state;         // x=spp, y=maxAge, z=epsilon, w=保留
} probe;
layout(std430, binding = 12) buffer ProbeSh { vec4 probeSh[]; } probeData;
layout(std430, binding = 13) buffer ProbeDepth { vec2 probeDepthMoments[]; } probeDepthData;
layout(std430, binding = 14) buffer ProbeOffset { vec4 probeOffsets[]; } probeOffsetData;

const float PI = 3.14159265359;
const float EPS = 1.0e-4;
const int PT_BOUNCES = 3;

struct Hit {
    float t;
    vec3 normal;
    int material;
    bool valid;
};

uint rngStep(inout uint state) {
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}

float rnd(inout uint state) {
    return float(rngStep(state) & 0x00FFFFFFu) / 16777216.0;
}

vec3 skyColor(vec3 dir) {
    const float t = clamp(dir.y * 0.5 + 0.5, 0.0, 1.0);
    const vec3 horizon = vec3(0.62, 0.70, 0.82);
    const vec3 zenith = vec3(0.12, 0.30, 0.68);
    return mix(horizon, zenith, t);
}

vec3 cosineDir(vec3 n, inout uint state) {
    const float r1 = rnd(state);
    const float r2 = rnd(state);
    const float phi = 2.0 * PI * r2;
    const float radius = sqrt(r1);
    const float z = sqrt(max(0.0, 1.0 - r1));
    const vec3 up = abs(n.y) < 0.99 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
    const vec3 t = normalize(cross(up, n));
    const vec3 b = cross(n, t);
    return normalize(t * cos(phi) * radius + b * sin(phi) * radius + n * z);
}

bool intersectQuad(vec3 p0, vec3 qu, vec3 qv, vec3 qn, int material, vec3 ro, vec3 rd,
                   float tMax, inout Hit hit) {
    const float denom = dot(qn, rd);
    if (abs(denom) < 1.0e-7) return false;
    const float t = dot(qn, p0 - ro) / denom;
    if (t <= EPS || t >= tMax) return false;
    const vec3 p = ro + rd * t;
    const vec3 rel = p - p0;
    const float uu = dot(qu, qu);
    const float vv = dot(qv, qv);
    if (uu < 1.0e-8 || vv < 1.0e-8) return false;
    const float a = dot(rel, qu) / uu;
    const float b = dot(rel, qv) / vv;
    if (a < 0.0 || a > 1.0 || b < 0.0 || b > 1.0) return false;
    hit.t = t;
    hit.normal = dot(qn, rd) > 0.0 ? -qn : qn;
    hit.material = material;
    hit.valid = true;
    return true;
}

bool intersectSphere(vec3 center, float radius, int material, vec3 ro, vec3 rd, float tMax,
                     inout Hit hit) {
    const vec3 oc = ro - center;
    const float b = dot(oc, rd);
    const float c = dot(oc, oc) - radius * radius;
    const float disc = b * b - c;
    if (disc < 0.0) return false;
    const float root = sqrt(disc);
    float t = -b - root;
    if (t <= EPS) t = -b + root;
    if (t <= EPS || t >= tMax) return false;
    const vec3 p = ro + rd * t;
    vec3 n = normalize(p - center);
    if (dot(n, rd) > 0.0) n = -n;
    hit.t = t;
    hit.normal = n;
    hit.material = material;
    hit.valid = true;
    return true;
}

Hit traceCornell(vec3 ro, vec3 rd, float tMax) {
    Hit best;
    best.valid = false;
    best.t = tMax;
    best.normal = vec3(0.0);
    best.material = 0;
    Hit cur;
    cur.valid = false;
    for (uint i = 0u; i < u.counts.x; ++i) {
        const uint base = i * 4u;
        const vec3 p0 = sceneData.data[base + 0u].xyz;
        const int material = int(sceneData.data[base + 0u].w);
        const vec3 qu = sceneData.data[base + 1u].xyz;
        const vec3 qv = sceneData.data[base + 2u].xyz;
        const vec3 qn = sceneData.data[base + 3u].xyz;
        if (intersectQuad(p0, qu, qv, qn, material, ro, rd, best.t, cur) && cur.valid) {
            best = cur;
            cur.valid = false;
        }
    }
    const uint sphereBase = u.counts.x * 4u;
    for (uint i = 0u; i < u.counts.y; ++i) {
        const uint base = sphereBase + i * 2u;
        const vec3 center = sceneData.data[base + 0u].xyz;
        const float radius = sceneData.data[base + 0u].w;
        const int material = int(sceneData.data[base + 1u].x);
        if (intersectSphere(center, radius, material, ro, rd, best.t, cur) && cur.valid) {
            best = cur;
            cur.valid = false;
        }
    }
    return best;
}

vec3 shadeDirect(Hit hit, vec3 p, int lightMode) {
    const uint materialBase =
        u.counts.x * 4u + u.counts.y * 2u + uint(hit.material) * 2u;
    const vec3 albedo = sceneData.data[materialBase + 0u].xyz;
    // 与 CPU 一致：背光/阴影面至少保留天空朝向的 fill，避免纯黑。
    const vec3 fill = albedo * skyColor(hit.normal) * 0.22;
    vec3 result = vec3(0.0);
    if (lightMode == 0) {
        // 与引擎 sun 对齐：方向光不做阴影（引擎 sun 也没有 shadow pass）。
        const vec3 toLight = normalize(-u.sunDir.xyz);
        const float ndl = dot(hit.normal, toLight);
        if (ndl <= 0.0) return fill;
        result = albedo * (ndl / PI * 3.0);
    } else {
        const vec3 toLight = u.lightPos.xyz - p;
        const float dist = length(toLight);
        if (dist <= EPS) return fill;
        const vec3 l = toLight / dist;
        const float ndl = dot(hit.normal, l);
        if (ndl <= 0.0) return fill;
        const vec3 origin = p + hit.normal * (EPS * 10.0);
        if (traceCornell(origin, l, dist - EPS).valid) return fill;
        const float range = 9.0;
        const float att = 1.0 / (1.0 + 2.0 / range * dist + 1.0 / (range * range) * dist * dist);
        result = albedo * (ndl / PI * 3.0 * att);
    }
    return result + fill;
}

// 调用方自有的探针 SH 采样（渲染路径）：三次线性 + depth moments 可见性。
vec3 evalProbeSh(vec3 wp, vec3 n) {
    // B3a：weight.z = normalBias（世界单位，沿表面法线偏移查询点）；weight.w 预留给 viewBias。
    const float normalBias = max(probe.weight.z, 0.0);
    const vec3 wpB = wp + n * normalBias;
    vec3 counts = max(probe.spacingCounts.yzw, vec3(1.0));
    vec3 grid = (wpB - probe.origin.xyz) / max(probe.spacingCounts.x, 1.0e-6);
    grid = clamp(grid, vec3(0.0), counts - vec3(1.0));
    ivec3 i0 = ivec3(floor(grid));
    ivec3 i1 = min(i0 + 1, ivec3(counts) - 1);
    vec3 f = fract(grid);
    vec4 c[9];
    for (int k = 0; k < 9; ++k) c[k] = vec4(0.0);
    const int cx = int(counts.x);
    const int cy = int(counts.y);
    float wsum = 0.0;
    for (int dz = 0; dz < 2; ++dz) {
        for (int dy = 0; dy < 2; ++dy) {
            for (int dx = 0; dx < 2; ++dx) {
                float w = (dx == 0 ? 1.0 - f.x : f.x) *
                          (dy == 0 ? 1.0 - f.y : f.y) *
                          (dz == 0 ? 1.0 - f.z : f.z);
                const int px = dx == 0 ? i0.x : i1.x;
                const int py = dy == 0 ? i0.y : i1.y;
                const int pz = dz == 0 ? i0.z : i1.z;
                const int probeIndex = (pz * cy + py) * cx + px;
                const vec3 basePos = probe.origin.xyz + probe.spacingCounts.x *
                                    vec3(float(px), float(py), float(pz));
                const vec3 probePos = basePos + probeOffsetData.probeOffsets[probeIndex].xyz;
                // 背面探针（探针在表面背面）降权，减少墙角/球体轮廓的跨面串色。
                if (dot(probePos - wpB, n) <= 0.0) w *= 0.5;
                const vec3 toSurface = wpB - probePos;
                const float dist = length(toSurface);
                const vec3 d = toSurface / max(dist, 1.0e-6);
                const vec3 a = abs(d);
                int face = 4;
                float u = 0.0;
                float v = 0.0;
                if (a.x >= a.y && a.x >= a.z) {
                    if (d.x >= 0.0) {
                        face = 0;
                        u = -d.z / a.x;
                        v = -d.y / a.x;
                    } else {
                        face = 1;
                        u = d.z / a.x;
                        v = -d.y / a.x;
                    }
                } else if (a.y >= a.z) {
                    if (d.y >= 0.0) {
                        face = 2;
                        u = d.x / a.y;
                        v = d.z / a.y;
                    } else {
                        face = 3;
                        u = d.x / a.y;
                        v = -d.z / a.y;
                    }
                } else {
                    if (d.z >= 0.0) {
                        face = 4;
                        u = d.x / a.z;
                        v = -d.y / a.z;
                    } else {
                        face = 5;
                        u = -d.x / a.z;
                        v = -d.y / a.z;
                    }
                }
                const int res = max(1, int(probe.weight.y));
                const vec2 texel = (vec2(u, v) * 0.5 + 0.5) * float(res) - 0.5;
                const ivec2 di0 = ivec2(floor(texel));
                const vec2 df = fract(texel);
                vec2 depthMomentSum = vec2(0.0);
                float depthWeight = 0.0;
                for (int dd = 0; dd < 4; ++dd) {
                    const int dx = dd & 1;
                    const int dy = (dd >> 1) & 1;
                    const int ix = clamp(di0.x + dx, 0, res - 1);
                    const int iy = clamp(di0.y + dy, 0, res - 1);
                    const float ww = (dx == 0 ? 1.0 - df.x : df.x) *
                                     (dy == 0 ? 1.0 - df.y : df.y);
                    const int idx = ((probeIndex * 6 + face) * res + iy) * res + ix;
                    depthMomentSum += probeDepthData.probeDepthMoments[idx] * ww;
                    depthWeight += ww;
                }
                const vec2 moments = depthWeight > 1.0e-6 ? depthMomentSum / depthWeight
                                                          : vec2(1.0e30, 1.0e30);
                const float mean = moments.x;
                const float variance = max(moments.y - mean * mean, 1.0e-4);
                const float diff = dist - mean;
                float visible = 1.0;
                if (diff > 0.0) visible = variance / (variance + diff * diff);
                w *= clamp(visible, 0.0, 1.0);
                wsum += w;
                const int base = probeIndex * 9;
                for (int k = 0; k < 9; ++k) c[k] += probeData.probeSh[base + k] * w;
            }
        }
    }
    if (wsum < 1.0e-4) return vec3(0.0);
    for (int k = 0; k < 9; ++k) c[k] /= wsum;
    const float b0 = 0.2820947918;
    const float b1 = 0.4886025119 * n.y;
    const float b2 = 0.4886025119 * n.z;
    const float b3 = 0.4886025119 * n.x;
    const float b4 = 1.0925484306 * n.x * n.y;
    const float b5 = 1.0925484306 * n.y * n.z;
    const float b6 = 0.3153915653 * (3.0 * n.z * n.z - 1.0);
    const float b7 = 1.0925484306 * n.x * n.z;
    const float b8 = 0.5462742153 * (n.x * n.x - n.y * n.y);
    vec3 irr = (3.14159265359 * b0) * c[0].rgb;
    irr += (2.0 * 3.14159265359 / 3.0) * (b1 * c[1].rgb + b2 * c[2].rgb + b3 * c[3].rgb);
    irr += (3.14159265359 / 4.0) * (b4 * c[4].rgb + b5 * c[5].rgb + b6 * c[6].rgb + b7 * c[7].rgb +
                         b8 * c[8].rgb);
    return max(irr, vec3(0.0));
}

// 路径追踪：indirectOnly=true 时跳过首个命中的直接光、且不采样探针（探针更新/烘焙语义，
// 与 cornellCpuRt.cpp 的 radiance(..., true) 一致）；false 为渲染预览语义。
vec3 radiance(vec3 ro, vec3 rd, int lightMode, bool indirectOnly, inout uint state,
              out vec3 primaryP, out bool primaryHit) {
    primaryP = vec3(0.0);
    primaryHit = false;
    vec3 result = vec3(0.0);
    vec3 throughput = vec3(1.0);
    vec3 currentOrigin = ro;
    vec3 currentDir = rd;
    for (int bounce = 0; bounce < PT_BOUNCES; ++bounce) {
        const Hit hit = traceCornell(currentOrigin, currentDir, 1.0e30);
        if (!hit.valid) {
            result += throughput * skyColor(currentDir);
            break;
        }
        const vec3 p = currentOrigin + currentDir * hit.t;
        if (!primaryHit) {
            primaryP = p;
            primaryHit = true;
        }
        const uint materialBase =
            u.counts.x * 4u + u.counts.y * 2u + uint(hit.material) * 2u;
        const vec3 albedo = sceneData.data[materialBase + 0u].xyz;
        const float metallic = sceneData.data[materialBase + 0u].w;
        if (metallic > 0.5) {
            currentOrigin = p + hit.normal * (EPS * 10.0);
            currentDir = normalize(reflect(currentDir, hit.normal));
            throughput *= albedo;
            continue;
        }
        if (!indirectOnly || bounce > 0) {
            const vec3 direct = shadeDirect(hit, p, lightMode);
            if (!any(isnan(direct)) && !any(isinf(direct))) result += throughput * direct;
        }
        if (!indirectOnly && probe.weight.x > 0.5) {
            // 探针只烘间接光；直接光在上一步加，跳过第 2/3 次漫反射射线。
            const vec3 indirect = evalProbeSh(p, hit.normal);
            result += throughput * albedo * indirect * 0.31830988618;
            break;
        }
        currentOrigin = p + hit.normal * (EPS * 10.0);
        currentDir = cosineDir(hit.normal, state);
        throughput *= albedo;
        if (bounce >= 2) {
            const float survive = 0.8;
            if (rnd(state) > survive) break;
            throughput *= 1.0 / survive;
        }
    }
    if (any(isnan(result)) || any(isinf(result))) return vec3(0.0);
    return clamp(result, vec3(0.0), vec3(1.0e4));
}

#endif // CORNELL_RAY_LIB_GLSL