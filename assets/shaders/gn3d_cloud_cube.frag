#version 450
// gn3d_cloud_cube.frag — 方块包围盒内的体积云：射线-盒求交 + 盒内噪声 raymarch。
//
// 方块 = 包围盒（不是云的形状）：密度在盒内由三维噪声生成，并在盒内边界平滑收到 0，
// 因此云的外轮廓是噪声团块，**不会出现方块轮廓**。
// 光照：朝太阳一次密度采样做自阴影 + 天顶环境色；底部略暗。
// 混合：预乘 alpha（src=ONE / dst=ONE_MINUS_SRC_ALPHA）⇒ outColor = vec4(scatter, 1 - T)。
layout(location = 0) in vec3 vWorldPos; // 盒子远侧表面点（射线出盒点）
layout(location = 1) in vec3 vBoxMin;
layout(location = 2) in vec3 vBoxMax;
layout(location = 3) in vec3 vColor;
layout(location = 4) in float vAlpha; // 密度倍率
layout(location = 5) in float vViewDist;
layout(location = 6) in float vIsDeck; // 1 = 云盖（阴天整片），0 = 普通云块

layout(location = 0) out vec4 outColor;

layout(binding = 0, set = 0) uniform CubeUBO {
    mat4 viewProj;
    vec4 aCamera;
    vec4 aSun;      // xyz = 指向太阳，w = 太阳强度
    vec4 aAmbient;  // rgb = 天空环境色
    vec4 aFog;      // rgb = 雾色，w = 雾密度(1/m)
    vec4 aCount;    // x = 实例数，y = 全场景云底高度(m)，z = 阴雨程度 0..1
    vec4 aInstances[128];
    vec4 aInstanceTint[128];
} u;

const int MARCH_STEPS = 20;
const float DENSITY_SIGMA = 0.030; // 1/m：视线消光（包围盒边长 ~60–100 m ⇒ τ ≈ 2–3）
const float SUN_SIGMA = 0.012;     // 1/m：太阳方向消光
const float SUN_STEP = 160.0;      // 自阴影采样距离（m）
const float DECK_THICKNESS = 420.0; // 云盖薄板厚度（m）

// 3D value noise（两倍频）。
float hash13(vec3 p) {
    p = fract(p * 0.1031);
    p += dot(p, p.yzx + 33.33);
    return fract((p.x + p.y) * p.z);
}

float valueNoise(vec3 p) {
    const vec3 i = floor(p);
    const vec3 f = fract(p);
    const vec3 w = f * f * (3.0 - 2.0 * f);
    const float a = hash13(i + vec3(0.0, 0.0, 0.0));
    const float b = hash13(i + vec3(1.0, 0.0, 0.0));
    const float c = hash13(i + vec3(0.0, 1.0, 0.0));
    const float d = hash13(i + vec3(1.0, 1.0, 0.0));
    const float e = hash13(i + vec3(0.0, 0.0, 1.0));
    const float g = hash13(i + vec3(1.0, 0.0, 1.0));
    const float h = hash13(i + vec3(0.0, 1.0, 1.0));
    const float k = hash13(i + vec3(1.0, 1.0, 1.0));
    const float x00 = mix(a, b, w.x);
    const float x10 = mix(c, d, w.x);
    const float x01 = mix(e, g, w.x);
    const float x11 = mix(h, k, w.x);
    return mix(mix(x00, x10, w.y), mix(x01, x11, w.y), w.z);
}

// 盒内密度：平底 + 径向轮廓 + 方向噪声半径。
// 三条约束：
//   1. 衰减用"盒心归一化半径"，不用"到六个面的距离"（后者等值面平行于盒面，云会带平直边）。
//   2. 云的外缘半径 R 恒 < 1（盒子半径的分数）⇒ 密度在盒面之前归零，画面上永远看不到盒子。
//   3. **密度只依赖世界坐标**（不带每盒随机相位/各向异性）⇒ 相邻盒子里的噪声连续，
//      一个簇的 2–3 个盒子叠起来就是**一朵**云，而不是几朵拼在一起。
// 平底：全场景统一的云底高度（u.aCount.y）以下密度归零，向上 30 m 起坡 —— 积云的平底。
float cloudDensity(vec3 p) {
    const float baseAltitude = u.aCount.y;
    const float baseFade = smoothstep(baseAltitude, baseAltitude + 30.0, p.y);
    if(baseFade <= 0.0) return 0.0;

    const vec3 extent = max(vBoxMax - vBoxMin, vec3(1.0e-3));
    const vec3 local = (p - vBoxMin) / extent; // 0..1
    vec3 q = (local - 0.5) * 2.0;              // -1..1，1 = 面心
    q.y *= 1.25;                               // 全局压扁：云宽大于高
    const float r = length(q);

    // 方向相关的半径：低频噪声给出"瓣"，轮廓才不像球；R ≤ 0.88 < 1 ⇒ 绝不碰盒面。
    // 云盖（大盒子）用更粗的噪声尺度，否则几百米的板里全是高频细节、步进会走样。
    const bool isDeck = vIsDeck > 0.5;
    const float lobeScale = isDeck ? 0.0018 : 0.060;
    const float detailScale = isDeck ? 0.0045 : 0.035;
    const float fineScale = isDeck ? 0.0140 : 0.115;
    const float lobe = valueNoise(p * lobeScale);
    const float cloudRadius = 0.52 + 0.36 * lobe;
    const float falloff = 1.0 - smoothstep(cloudRadius - 0.20, cloudRadius, r);
    if(falloff <= 0.0) return 0.0;

    // 云团内部结构（纯世界坐标，跨盒连续）。
    const float n = 0.62 * valueNoise(p * detailScale) + 0.38 * valueNoise(p * fineScale);
    return smoothstep(0.40, 0.58, n) * falloff * baseFade;
}

void main() {
    const vec3 cameraPos = u.aCamera.xyz;
    const vec3 sunDir = normalize(u.aSun.xyz);
    const float sunIntensity = max(u.aSun.w, 0.0);
    const vec3 ray = normalize(vWorldPos - cameraPos);

    // 射线-盒求交（slab）：近交点 tn、远交点 tf。
    const vec3 invRay = 1.0 / ray;
    const vec3 t0 = (vBoxMin - cameraPos) * invRay;
    const vec3 t1 = (vBoxMax - cameraPos) * invRay;
    const vec3 tMin = min(t0, t1);
    const vec3 tMax = max(t0, t1);
    float tn = max(max(tMin.x, tMin.y), tMin.z);
    float tf = min(min(tMax.x, tMax.y), tMax.z);
    tn = max(tn, 0.0);

    // 云盖：盒子覆盖全天空，但云只占"云底→云顶"这块薄板 ⇒ 把步进区间夹到板上。
    if(vIsDeck > 0.5) {
        if(abs(ray.y) < 1.0e-4) discard;
        const float slabBottom = u.aCount.y;
        const float slabTop = u.aCount.y + DECK_THICKNESS;
        const float ty0 = (slabBottom - cameraPos.y) / ray.y;
        const float ty1 = (slabTop - cameraPos.y) / ray.y;
        tn = max(tn, min(ty0, ty1));
        tf = min(tf, max(ty0, ty1));
    }
    if(tf <= tn) discard;

    // 云盖是大面积像素：步数少一点（薄板 + 粗噪声已经足够），把预算留给近处的云团。
    const int stepCount = vIsDeck > 0.5 ? 16 : MARCH_STEPS;
    const float dt = (tf - tn) / float(stepCount);
    const vec3 ambCol = mix(vec3(0.34, 0.40, 0.52), u.aAmbient.rgb, 0.55);
    float transmittance = 1.0;
    vec3 scatter = vec3(0.0);

    for(int i = 0; i < MARCH_STEPS; ++i) {
        if(i >= stepCount) break;
        const vec3 p = cameraPos + ray * (tn + (float(i) + 0.5) * dt);
        const float density = cloudDensity(p) * vAlpha;
        if(density <= 0.002) continue;

        // 朝太阳采样一次：云顶/外侧薄 ⇒ 亮，云内部厚 ⇒ 暗。
        const float sunDensity = cloudDensity(p + sunDir * SUN_STEP) * vAlpha;
        const float sunT = exp(-SUN_SIGMA * sunDensity * SUN_STEP);

        // 盒内高度：0 = 底，1 = 顶（底部略暗，顶部提亮）。
        const float h = clamp((p.y - vBoxMin.y) / max(vBoxMax.y - vBoxMin.y, 1.0e-3), 0.0, 1.0);
        // 阴雨（overcast，来自天气状态）：云体压暗压灰 —— 不能只靠太阳强度，否则雨天仍是白团。
        const float overcast = clamp(u.aCount.z, 0.0, 1.0);
        vec3 lit;
        if(vIsDeck > 0.5) {
            // 云盖：阴天太阳被完全漫射，sunT 只会把整片压成"深蓝天空"，所以只用它做起伏。
            const vec3 deckColor = vColor * mix(vec3(1.0), vec3(0.78, 0.80, 0.84), overcast);
            lit = deckColor * (0.72 + 0.22 * h) * (1.0 - 0.22 * overcast);
            lit += ambCol * 0.34;
            lit *= 0.92 + 0.18 * (1.0 - sunT); // 团块起伏（云厚的方向略暗）
        } else {
            const vec3 bodyColor = vColor * mix(vec3(1.0), vec3(0.50, 0.53, 0.58), overcast);
            lit = bodyColor * sunT * (0.55 + 0.45 * h) * (0.35 + 1.65 * sunIntensity);
            lit += ambCol * (0.42 + 0.28 * h) * (1.0 - 0.45 * overcast);
        }

        const float stepAlpha = 1.0 - exp(-DENSITY_SIGMA * density * dt);
        scatter += transmittance * stepAlpha * lit;
        transmittance *= 1.0 - stepAlpha;
        if(transmittance < 0.02) break;
    }

    // 距离雾（与 gn3d_pbr 同形）：远处云块与天空/地形同色。
    const float fogAmount = 1.0 - exp(-max(u.aFog.w, 0.0) * vViewDist);
    scatter = mix(scatter, u.aFog.rgb * (1.0 - transmittance), clamp(fogAmount, 0.0, 1.0));

    outColor = vec4(scatter, 1.0 - transmittance);
}
