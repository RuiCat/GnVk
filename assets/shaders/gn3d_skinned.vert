#version 450
// ╔══════════════════════════════════════════════════════════╗
// ║  GN_mini — P2 蒙皮顶点着色器                             ║
// ║  4 关节 LBS（palette UBO）→ model → viewProj             ║
// ╚══════════════════════════════════════════════════════════╝
//
// 顶点布局与 GN3d::sSKINNED_VERTEX（stride 68）逐字段对应：
//   loc0 pos / loc1 normal / loc2 uv / loc3 tangent / loc4 u16vec4 joints / loc5 weights。
// set 0：binding 0 = 场景 UBO（VkSceneUBO），binding 1 = 128 × mat4 关节调色板。
// 关节索引钳到 127：调色板最多上传 128 个矩阵，越界索引不得越界读 UBO。

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUv;
layout(location = 3) in vec3 inTangent;
layout(location = 4) in uvec4 inJoints;
layout(location = 5) in vec4 inWeights;

layout(binding = 0, set = 0) uniform SceneUBO {
    mat4 viewProj;
    vec4 cameraPos;
    vec4 ambientLight;
} scene;

layout(binding = 1, set = 0) uniform SkinPaletteUBO {
    mat4 joints[128];
} skin;

layout(push_constant) uniform ModelPC {
    mat4 model;
    vec4 color;
} pc;

layout(location = 0) out vec3 fragNormal;
layout(location = 1) out vec2 fragUv;
layout(location = 2) out vec3 fragWorldPos;

// 非均匀缩放必须用逆转置矩阵变换法线；det≈0 时退回单位矩阵。
mat3 normalMatrix(mat4 model) {
    mat3 linear = mat3(model);
    float det = determinant(linear);
    return abs(det) > 1.0e-8 ? transpose(inverse(linear)) : mat3(1.0);
}

void main() {
    vec3 skinnedPosition = vec3(0.0);
    vec3 skinnedNormal = vec3(0.0);
    float weightSum = 0.0;

    for(uint slot = 0; slot < 4; ++slot) {
        float weight = max(inWeights[slot], 0.0);
        if(weight <= 0.0) continue; // 零权重槽位的关节索引可能是脏值
        uint joint = min(inJoints[slot], 127u);
        mat4 palette = skin.joints[joint];
        skinnedPosition += (palette * vec4(inPos, 1.0)).xyz * weight;
        skinnedNormal += (mat3(palette) * inNormal) * weight;
        weightSum += weight;
    }

    vec3 localPosition = inPos;
    vec3 localNormal = inNormal;
    if(dot(localNormal, localNormal) < 1.0e-12) localNormal = vec3(0.0, 0.0, 1.0);
    localNormal = normalize(localNormal);
    if(weightSum > 1e-6) {
        localPosition = skinnedPosition / weightSum;
        // 权重和为 0 或各关节法线互相抵消时保持原法线，避免 normalize(0)=NaN。
        if(dot(skinnedNormal, skinnedNormal) > 1.0e-12) localNormal = normalize(skinnedNormal);
    }

    vec4 worldPosition = pc.model * vec4(localPosition, 1.0);
    gl_Position = scene.viewProj * worldPosition;
    fragWorldPos = worldPosition.xyz;
    vec3 n = normalMatrix(pc.model) * localNormal;
    if(dot(n, n) < 1.0e-12) n = vec3(0.0, 0.0, 1.0);
    fragNormal = normalize(n);
    fragUv = inUv;
}
