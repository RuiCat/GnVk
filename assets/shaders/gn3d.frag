#version 450
// ╔══════════════════════════════════════════════════════════╗
// ║  GN_mini — Blinn-Phong 片段着色器                        ║
// ║  纹理采样 + 漫反射 + Blinn-Phong 镜面高光 + 多光源 + 衰减║
// ╚══════════════════════════════════════════════════════════╝
#define MAX_LIGHTS 8

layout(location = 0) in vec3 fragWorldPos;   // 世界空间位置 (顶点着色器插值)
layout(location = 1) in vec3 fragNormal;     // 世界空间法线 (mat3(model)变换后)
layout(location = 2) in vec2 fragUV;         // 纹理坐标

// ── UBO + 纹理 ──────────────────────────────────────
layout(binding = 0, set = 0) uniform SceneUBO {
    mat4 viewProj;        // 摄像机 VP 矩阵
    vec4 cameraPos;       // 摄像机世界位置
    vec4 ambientLight;    // 场景环境光
} scene;

layout(binding = 1, set = 0) uniform sampler2D texSampler;  // 漫反射纹理

struct Light {
    vec4 color;           // RGB + 强度
    vec4 position;        // xyz + 类型 (0=方向,1=点,2=聚光)
    vec4 direction;       // xyz + shadowType
    vec4 attenuation;     // 衰减参数
    vec4 coneAngles;      // 聚光灯锥角
};

layout(binding = 2, set = 0) uniform LightUBO {
    Light lights[MAX_LIGHTS];   // 8 光源
    vec4 ambientLight;          // 光源环境光
    float lightCount;           // 激活光源数
} lightData;

// ── Push Constant (Blinn-Phong 材质) ────────────────
layout(push_constant) uniform ModelPC {
    mat4 model;           // 模型矩阵
    vec4 diffuse;         // RGB = 漫反射颜色, A = _pad
    vec4 specular;        // RGB = 镜面反射颜色, A = shininess(1-256)
} pc;

layout(location = 0) out vec4 outColor;

void main() {
    // ── 法线 + 视线方向 ─────────────────────────────
    vec3 N = normalize(fragNormal);
    vec3 V = normalize(scene.cameraPos.xyz - fragWorldPos);

    // ── 材质颜色 ────────────────────────────────────
    // 纹理采样 × diffuse 颜色 → 最终漫反射颜色
    vec3 texColor = texture(texSampler, fragUV).rgb;
    vec3 Kd = texColor * pc.diffuse.rgb;       // 漫反射颜色
    vec3 Ks = pc.specular.rgb;                 // 镜面反射颜色
    float shininess = pc.specular.a;           // 光泽度 (值越大高光越集中)

    // ── 环境光 ─────────────────────────────────────
    // 场景环境光 × 光源环境光 × 漫反射颜色
    vec3 ambient = Kd
                 * scene.ambientLight.rgb * lightData.ambientLight.rgb
                 * scene.ambientLight.a * lightData.ambientLight.a;

    vec3 totalColor = vec3(0.0);
    int count = int(lightData.lightCount);

    // ── 逐光源计算 ─────────────────────────────────
    for (int i = 0; i < count && i < MAX_LIGHTS; i++) {
        Light L = lightData.lights[i];
        vec3 lightColor = L.color.rgb * L.color.a;
        if (lightColor.r + lightColor.g + lightColor.b < 0.001) continue;

        vec3 Ldir;                              // 光照方向 (表面→光源)
        float att = 1.0;                        // 衰减系数

        if (L.position.w < 0.5) {
            // 方向光: 平行光线, 无衰减
            Ldir = normalize(-L.direction.xyz);
        } else if (L.position.w < 1.5) {
            // 点光源: 距离二次衰减
            vec3 toLight = L.position.xyz - fragWorldPos;
            float d = length(toLight);
            Ldir = toLight / max(d, 0.0001);
            att = 1.0 / (L.attenuation.x + L.attenuation.y * d
                       + L.attenuation.z * d * d);
        } else {
            // 聚光灯: 锥角软化 + 距离衰减
            vec3 toLight = L.position.xyz - fragWorldPos;
            float d = length(toLight);
            Ldir = toLight / max(d, 0.0001);
            // 聚光因子: 内锥(全亮) → 外锥(0) 线性过渡
            float cosTheta = dot(-Ldir, normalize(L.direction.xyz));
            float innerCos = L.coneAngles.x;    // 内锥角余弦
            float outerCos = L.coneAngles.y;    // 外锥角余弦
            float spotFactor = clamp(
                (cosTheta - outerCos) / max(innerCos - outerCos, 0.0001),
                0.0, 1.0);
            att = spotFactor / (L.attenuation.x + L.attenuation.y * d
                              + L.attenuation.z * d * d);
        }

        float NdotL = max(dot(N, Ldir), 0.0);

        // ── 漫反射 (Lambert): Kd × N·L ────────────
        vec3 diffuse = Kd * NdotL;

        // ── Blinn-Phong 镜面高光 ──────────────────
        // H = normalize(L+V): 半角向量 (比 Phong 反射向量更高效)
        // pow(N·H, s): 镜面瓣, s 越大高光越集中
        vec3 H = normalize(Ldir + V);
        float NdotH = max(dot(N, H), 0.0);
        float specularFactor = pow(NdotH, shininess);
        vec3 specular = Ks * specularFactor;

        totalColor += (diffuse + specular) * lightColor * att;
    }

    outColor = vec4(ambient + totalColor, 1.0);
}
