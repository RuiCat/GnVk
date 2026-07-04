#version 450
// ╔══════════════════════════════════════════════════════════╗
// ║  GN_mini — SSAO 片段着色器                              ║
// ║  屏幕空间环境光遮蔽: 半球采样 + 深度比较                  ║
// ╚══════════════════════════════════════════════════════════╝

layout(location = 0) in vec2 fragUV;

layout(binding = 1, set = 0) uniform sampler2D gNormalTex;
layout(binding = 2, set = 0) uniform sampler2D gDepthTex;

layout(binding = 0, set = 0) uniform SSAO_UBO {
    mat4 proj;          // 投影矩阵 (用于重建视空间位置)
    vec4 samples[16];   // 半球采样方向 (16个)
    float radius;
    float bias;
    float _pad0, _pad1;
} ssao;

layout(location = 0) out float outAO;

// 从深度 + UV 重建视空间位置
vec3 ViewPosFromDepth(float depth, vec2 uv) {
    vec4 clip = vec4(uv * 2.0 - 1.0, depth, 1.0);
    vec4 view = inverse(ssao.proj) * clip;
    return view.xyz / view.w;
}

void main() {
    float depth = texture(gDepthTex, fragUV).r;
    if (depth >= 0.9999) { outAO = 1.0; return; }
    
    vec3 viewPos = ViewPosFromDepth(depth, fragUV);
    vec3 N = normalize(texture(gNormalTex, fragUV).rgb * 2.0 - 1.0);
    
    // 随机旋转 (避免带状伪影)
    float angle = fract(sin(dot(fragUV, vec2(12.9898, 78.233))) * 43758.5453) * 6.28318;
    float ca = cos(angle), sa = sin(angle);
    
    float occlusion = 0.0;
    float radius = ssao.radius;
    float bias = ssao.bias;
    
    for (int i = 0; i < 16; i++) {
        // 旋转采样方向到切线空间
        vec3 sampleDir = ssao.samples[i].xyz;
        // 确保采样方向在半球内 (朝向法线)
        if (dot(sampleDir, N) < 0.0) sampleDir = -sampleDir;
        
        // 旋转
        vec2 rotDir = vec2(sampleDir.x * ca - sampleDir.y * sa,
                           sampleDir.x * sa + sampleDir.y * ca);
        sampleDir = vec3(rotDir, sampleDir.z);
        
        vec3 samplePos = viewPos + sampleDir * radius;
        
        // 投影到屏幕空间
        vec4 clipSample = ssao.proj * vec4(samplePos, 1.0);
        vec3 ndcSample = clipSample.xyz / max(clipSample.w, 0.0001);
        vec2 sampleUV = ndcSample.xy * 0.5 + 0.5;
        
        float sampleDepth = texture(gDepthTex, sampleUV).r;
        vec3 sampleViewPos = ViewPosFromDepth(sampleDepth, sampleUV);
        
        // 比较: 采样点是否被遮挡
        float rangeCheck = smoothstep(0.0, 1.0, radius / abs(viewPos.z - sampleViewPos.z));
        occlusion += (sampleViewPos.z >= samplePos.z + bias ? 1.0 : 0.0) * rangeCheck;
    }
    
    outAO = 1.0 - (occlusion / 16.0);
}
