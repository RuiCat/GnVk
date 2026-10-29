// gn3d_skinned_ssbo.vert — P9-b SSBO 调色板蒙皮：4 关节 LBS，palette 存在 SSBO 中（无 128 上限）。
//
// 顶点布局与 GN3d::sSKINNED_VERTEX（stride 68）逐字段对应（与 P2 的 gn3d_skinned.vert 相同）。
// set 0：binding 0 = 场景 UBO，binding 1 = SSBO `mat4 palette[]`（v1 上限 4096，索引钳到 4095）。
#version 450

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

layout(binding = 1, set = 0) readonly buffer SkinPaletteSSBO {
    mat4 joints[];
} skin;

layout(push_constant) uniform ModelPC {
    mat4 model;
    vec4 color;
} pc;

layout(location = 0) out vec3 fragNormal;
layout(location = 1) out vec2 fragUv;
layout(location = 2) out vec3 fragWorldPos;

void main() {
    vec3 skinnedPosition = vec3(0.0);
    vec3 skinnedNormal = vec3(0.0);
    float weightSum = 0.0;

    for(uint slot = 0; slot < 4; ++slot) {
        float weight = max(inWeights[slot], 0.0);
        if(weight <= 0.0) continue; // 零权重槽位的关节索引可能是脏值
        uint joint = min(inJoints[slot], 4095u);
        mat4 palette = skin.joints[joint];
        skinnedPosition += (palette * vec4(inPos, 1.0)).xyz * weight;
        skinnedNormal += (mat3(palette) * inNormal) * weight;
        weightSum += weight;
    }

    vec3 localPosition = inPos;
    vec3 localNormal = normalize(inNormal);
    if(weightSum > 1e-6) {
        localPosition = skinnedPosition / weightSum;
        localNormal = normalize(skinnedNormal);
    }

    vec4 worldPosition = pc.model * vec4(localPosition, 1.0);
    gl_Position = scene.viewProj * worldPosition;
    fragWorldPos = worldPosition.xyz;
    fragNormal = normalize(mat3(pc.model) * localNormal);
    fragUv = inUv;
}
