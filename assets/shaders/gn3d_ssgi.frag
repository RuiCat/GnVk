#version 450
// ╔══════════════════════════════════════════════════════════╗
// ║  GN_mini — SSGI（半分辨率，G4）                          ║
// ║  世界空间 ray march：8 rays × 8 steps；hit=albedo×SH9，   ║
// ║  miss 回退 SH9 环境辐照度                                 ║
// ╚══════════════════════════════════════════════════════════╝

layout(location = 0) in vec2 fragUV;
layout(location = 0) out vec4 outColor;

layout(binding = 0, set = 0) uniform SSGI_UBO {
    mat4 viewProj;
    mat4 invViewProj;
    vec4 params; // x=强度(0=关) y=步数 z=厚度 w=最大距离
    vec4 screen; // x=1/宽 y=1/高 z=半宽 w=半高
} ssgi;

layout(binding = 1, set = 0) uniform sampler2D gNormalTex; // RGBA16F 世界法线
layout(binding = 2, set = 0) uniform sampler2D gDepthTex;  // D32
layout(binding = 3, set = 0) uniform sampler2D gAlbedoTex; // RGBA8_SRGB → 自动 linear
layout(binding = 4, set = 0) uniform GIUBO {
    vec4 indirect;
    vec4 aoParams;
    vec4 envParams;
    vec4 sh[9];
    mat4 invViewProj;
} gi;

vec3 worldPosFromDepth(float d, vec2 uv) {
    vec4 clip = vec4(uv * 2.0 - 1.0, d, 1.0);
    vec4 world = ssgi.invViewProj * clip;
    return world.xyz / world.w;
}

// 与 giParams::evalSh9 一致（y 向上世界空间；A_l = π, 2π/3, π/4）。
vec3 evalSH9(vec3 n) {
    float b0 = 0.2820947918;
    float b1 = 0.4886025119 * n.y;
    float b2 = 0.4886025119 * n.z;
    float b3 = 0.4886025119 * n.x;
    float b4 = 1.0925484306 * n.x * n.y;
    float b5 = 1.0925484306 * n.y * n.z;
    float b6 = 0.3153915653 * (3.0 * n.z * n.z - 1.0);
    float b7 = 1.0925484306 * n.x * n.z;
    float b8 = 0.5462742153 * (n.x * n.x - n.y * n.y);
    vec3 irr = (3.14159265359 * b0) * gi.sh[0].rgb;
    irr += (2.0 * 3.14159265359 / 3.0) *
           (b1 * gi.sh[1].rgb + b2 * gi.sh[2].rgb + b3 * gi.sh[3].rgb);
    irr += (3.14159265359 / 4.0) *
           (b4 * gi.sh[4].rgb + b5 * gi.sh[5].rgb + b6 * gi.sh[6].rgb +
            b7 * gi.sh[7].rgb + b8 * gi.sh[8].rgb);
    return max(irr, vec3(0.0));
}

void main() {
    if (ssgi.params.x <= 0.0) {
        outColor = vec4(0.0);
        return;
    }
    float depth = texture(gDepthTex, fragUV).r;
    if (depth >= 0.9999) {
        outColor = vec4(0.0);
        return;
    }
    vec3 P = worldPosFromDepth(depth, fragUV);
    vec3 N = normalize(texture(gNormalTex, fragUV).rgb * 2.0 - 1.0);

    // 逐像素随机旋转的 TBN（固定哈希，无时域噪声）。
    float rnd = fract(sin(dot(fragUV, vec2(12.9898, 78.233))) * 43758.5453);
    float angle = rnd * 6.2831853;
    vec3 randomVec = vec3(cos(angle), sin(angle), 0.0);
    vec3 tangent = randomVec - N * dot(randomVec, N);
    if (dot(tangent, tangent) < 1e-6) {
        tangent = abs(N.y) < 0.99 ? cross(N, vec3(0.0, 1.0, 0.0)) : cross(N, vec3(1.0, 0.0, 0.0));
    }
    tangent = normalize(tangent);
    vec3 bitangent = cross(N, tangent);
    mat3 tbn = mat3(tangent, bitangent, N);

    const int RAYS = 8;
    const int STEPS = 8;
    float stepLength = max(ssgi.params.w, 0.1) / float(STEPS);
    vec3 environment = evalSH9(N);
    vec3 radianceSum = vec3(0.0);
    for (int r = 0; r < RAYS; ++r) {
        // 余弦加权半球方向（golden-angle + 逐像素扰动）。
        float sinTheta = sqrt((float(r) + rnd) / float(RAYS));
        float cosTheta = sqrt(max(1.0 - sinTheta * sinTheta, 0.0));
        float phi = (float(r) + rnd) * 2.39996323;
        vec3 dirLocal = vec3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);
        vec3 dir = normalize(tbn * dirLocal);
        vec3 hit = environment; // 未命中 → 环境辐照度
        float jitter = fract(rnd * 71.0 + float(r) * 0.37);
        for (int s = 1; s <= STEPS; ++s) {
            vec3 samplePos = P + dir * (float(s) + jitter) * stepLength;
            vec4 clip = ssgi.viewProj * vec4(samplePos, 1.0);
            if (clip.w <= 1e-6) break;
            vec3 ndc = clip.xyz / clip.w;
            vec2 uv = ndc.xy * 0.5 + 0.5;
            if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) break;
            float storedDepth = texture(gDepthTex, uv).r;
            if (storedDepth >= 0.9999) continue;
            vec3 storedPos = worldPosFromDepth(storedDepth, uv);
            // 命中：采样点与已存表面距离在厚度内（世界空间，避免把前景当遮挡）。
            if (length(storedPos - samplePos) <= max(ssgi.params.z, 1e-3)) {
                vec3 hitNormal =
                    normalize(texture(gNormalTex, uv).rgb * 2.0 - 1.0);
                vec3 hitAlbedo = texture(gAlbedoTex, uv).rgb;
                hit = hitAlbedo * evalSH9(hitNormal);
                break;
            }
        }
        radianceSum += hit;
    }
    outColor = vec4(radianceSum / float(RAYS), 1.0);
}
