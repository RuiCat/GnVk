// ╔══════════════════════════════════════════════════════════╗
// ║  GN_mini — 3D 光源系统 实现                              ║
// ╚══════════════════════════════════════════════════════════╝
#include <GN.h>
#include "GN_3Dlight.h"
#include <cstring>
#include <cmath>

namespace GN3d {

using namespace GNg;

cLIGHT::cLIGHT() : aCount(0), aDirty(true) {
    memset(&aData, 0, sizeof(aData));
    // 默认环境光
    aData.ambientLight[0] = 0.15f;
    aData.ambientLight[1] = 0.15f;
    aData.ambientLight[2] = 0.2f;
    aData.ambientLight[3] = 1.0f;
    // 默认阴影贴图大小 + CSM 默认 4 级（分割/texel 由查看器每帧写入）
    aData.shadowParams[0] = 2048.0f;
    aData.shadowParams[1] = 1.0f;

    // 多光源 cube shadow 的默认值。
    // **必须显式写 layer = -1**：memset 后全是 0，而 0 是**有效的**层号 ——
    // 着色器会以为"这盏灯有 cube shadow、层号 0"，于是去采样一张从没渲染过的深度图，
    // 结果是整片场景被错误遮挡（比没有阴影黑得多）。
    for(uint32_t i = 0; i < 4; ++i) {
        aData.cubeShadowRange[i][0] = 0.0f;
        aData.cubeShadowRange[i][1] = 0.0f;
        aData.cubeShadowRange[i][2] = -1.0f;
        aData.cubeShadowRange[i][3] = 0.0f;
    }
}

void cLIGHT::Clear() {
    memset(&aData, 0, sizeof(aData));
    aData.ambientLight[0] = 0.15f;
    aData.ambientLight[1] = 0.15f;
    aData.ambientLight[2] = 0.2f;
    aData.ambientLight[3] = 1.0f;
    aData.shadowParams[0] = 2048.0f; // 恢复默认阴影贴图尺寸
    aData.shadowParams[1] = 1.0f; // 单级阴影（回退路径）
    aCount = 0;
    aDirty = true;
    // 清空光源后也要把 cube shadow 的层号置回 -1（见构造函数里的说明）
    for(uint32_t i = 0; i < 4; ++i) aData.cubeShadowRange[i][2] = -1.0f;
}

int cLIGHT::AddDirectional(const sVEC3 &dir, const sVEC3 &color, float intensity, bool enableShadow) {
    if(aCount >= GN_MAX_LIGHTS) return -1;
    int idx = aCount++;

    sLIGHT_GPU &L = aData.lights[idx];
    L.color[0] = color.x;
    L.color[1] = color.y;
    L.color[2] = color.z;
    L.color[3] = intensity;
    // 标准化方向并反转 (方向光的方向指向光源, 着色器中取反)
    sVEC3 d = dir;
    Vec3Normalize(&d, &d);
    L.direction[0] = d.x;
    L.direction[1] = d.y;
    L.direction[2] = d.z;
    L.direction[3] = enableShadow ? 1.0f : 0.0f; // shadowType: 1=2D shadow, 0=none
    L.position[0] = L.position[1] = L.position[2] = 0.0f;
    L.position[3] = (float)eLIGHT_DIRECTIONAL;
    L.attenuation[0] = 1.0f;
    L.attenuation[1] = L.attenuation[2] = L.attenuation[3] = 0.0f;
    L.coneAngles[0] = L.coneAngles[1] = L.coneAngles[2] = L.coneAngles[3] = 0.0f;

    aData.lightCount = (float)aCount;
    aDirty = true;
    return idx;
}

int cLIGHT::AddPoint(const sVEC3 &pos, float range,
                     const sVEC3 &color, float intensity, bool enableShadow,
                     float lightRadius) {
    if(aCount >= GN_MAX_LIGHTS) return -1;
    int idx = aCount++;

    sLIGHT_GPU &L = aData.lights[idx];
    L.color[0] = color.x;
    L.color[1] = color.y;
    L.color[2] = color.z;
    L.color[3] = intensity;
    L.position[0] = pos.x;
    L.position[1] = pos.y;
    L.position[2] = pos.z;
    L.position[3] = (float)eLIGHT_POINT;
    // direction.w = shadowType：0=无影，2=cube shadow（B2）。
    // 点光没有方向，所以 xyz 留 0，只用 w 当"有没有 cube 阴影"的开关。
    L.direction[0] = L.direction[1] = L.direction[2] = 0.0f;
    L.direction[3] = enableShadow ? 2.0f : 0.0f;
    // 默认衰减系数 (基于 range)
    L.attenuation[0] = 1.0f;
    L.attenuation[1] = 2.0f / (range > 0.001f ? range : 10.0f);
    L.attenuation[2] = 1.0f / ((range > 0.001f ? range : 10.0f) * (range > 0.001f ? range : 10.0f));
    L.attenuation[3] = range;
    L.coneAngles[0] = L.coneAngles[1] = 0.0f; // 点光无锥角
    L.coneAngles[2] = lightRadius > 0.0f ? lightRadius : 0.1f; // PCSS 半影
    L.coneAngles[3] = 0.0f;

    aData.lightCount = (float)aCount;
    aDirty = true;
    return idx;
}

int cLIGHT::AddSpot(const sVEC3 &pos, const sVEC3 &dir,
                    float range, float innerConeRad, float outerConeRad,
                    const sVEC3 &color, float intensity, bool enableShadow,
                    float lightRadius) {
    if(aCount >= GN_MAX_LIGHTS) return -1;
    int idx = aCount++;

    sLIGHT_GPU &L = aData.lights[idx];
    L.color[0] = color.x;
    L.color[1] = color.y;
    L.color[2] = color.z;
    L.color[3] = intensity;
    L.position[0] = pos.x;
    L.position[1] = pos.y;
    L.position[2] = pos.z;
    L.position[3] = (float)eLIGHT_SPOT;
    sVEC3 d = dir;
    Vec3Normalize(&d, &d);
    L.direction[0] = d.x;
    L.direction[1] = d.y;
    L.direction[2] = d.z;
    // shadowType: 2 = cube shadow（B2）。聚光与点光共用同一张 cube map。
    // **不要用 1**：1 是"方向光的 2D 级联"，聚光走那条会去采样 CSM 的级联矩阵，画面全黑。
    L.direction[3] = enableShadow ? 2.0f : 0.0f;
    L.attenuation[0] = 1.0f;
    L.attenuation[1] = 2.0f / (range > 0.001f ? range : 10.0f);
    L.attenuation[2] = 1.0f / ((range > 0.001f ? range : 10.0f) * (range > 0.001f ? range : 10.0f));
    L.attenuation[3] = range;
    L.coneAngles[0] = cosf(innerConeRad);
    L.coneAngles[1] = cosf(outerConeRad);
    L.coneAngles[2] = lightRadius > 0.0f ? lightRadius : 0.1f; // PCSS 半影
    L.coneAngles[3] = 0.0f;

    aData.lightCount = (float)aCount;
    aDirty = true;
    return idx;
}

void cLIGHT::SetCubeShadowRange(float nearZ, float farZ) {
    // 旧接口：等价于"给 cube 层 0 设置范围"，写回 shadowParams.z/w 保持兼容
    // （老着色器/老示例读的是这两个字段）。
    if(!(nearZ > 0.0f)) nearZ = 0.05f;
    if(!(farZ > nearZ)) farZ = nearZ + 1.0f;
    aData.shadowParams[2] = nearZ;
    aData.shadowParams[3] = farZ;
    aDirty = true;
}

float cLIGHT::CubeShadowNear() const { return aData.shadowParams[2]; }

float cLIGHT::CubeShadowFar() const { return aData.shadowParams[3]; }

// ═══════════════════════════════════════════════════════════
//  多光源 cube shadow（B2 扩展）
// ═══════════════════════════════════════════════════════════
void cLIGHT::SetCubeShadowRangeForLight(uint32_t lightSlot, float nearZ, float farZ,
                                        int32_t layer) {
    if(lightSlot >= 4) return; // cubeShadowRange 只有 4 项
    if(!(nearZ > 0.0f)) nearZ = 0.05f;
    if(!(farZ > nearZ)) farZ = nearZ + 1.0f;
    // layer < 0 表示"这盏灯没有 cube shadow"：把 near 写成 0，
    // 着色器看到 near <= 0 就保守返回全亮（与旧行为一致）。
    aData.cubeShadowRange[lightSlot][0] = (layer >= 0) ? nearZ : 0.0f;
    aData.cubeShadowRange[lightSlot][1] = (layer >= 0) ? farZ : 0.0f;
    aData.cubeShadowRange[lightSlot][2] = (layer >= 0) ? (float)layer : -1.0f;
    aData.cubeShadowRange[lightSlot][3] = 0.0f;
    aDirty = true;
}

void cLIGHT::ClearCubeShadowRanges() {
    for(uint32_t i = 0; i < 4; ++i) {
        for(uint32_t k = 0; k < 4; ++k) aData.cubeShadowRange[i][k] = 0.0f;
        aData.cubeShadowRange[i][2] = -1.0f; // 层号 -1 = 无
    }
    aDirty = true;
}

float cLIGHT::GetLightRadius(int index) const {
    if(index < 0 || index >= (int)aCount) return 0.0f;
    return aData.lights[index].coneAngles[2];
}

void cLIGHT::SetLightRadius(int index, float radius) {
    if(index < 0 || index >= (int)aCount) return;
    aData.lights[index].coneAngles[2] = radius > 0.0f ? radius : 0.0f;
    aDirty = true;
}

void cLIGHT::SetColor(int index, const sVEC3 &color) {
    if(index < 0 || index >= (int)aCount) return;
    aData.lights[index].color[0] = color.x;
    aData.lights[index].color[1] = color.y;
    aData.lights[index].color[2] = color.z;
    aDirty = true;
}

void cLIGHT::SetIntensity(int index, float intensity) {
    if(index < 0 || index >= (int)aCount) return;
    aData.lights[index].color[3] = intensity;
    aDirty = true;
}

void cLIGHT::SetPosition(int index, const sVEC3 &pos) {
    if(index < 0 || index >= (int)aCount) return;
    aData.lights[index].position[0] = pos.x;
    aData.lights[index].position[1] = pos.y;
    aData.lights[index].position[2] = pos.z;
    aDirty = true;
}

void cLIGHT::SetDirection(int index, const sVEC3 &dir) {
    if(index < 0 || index >= (int)aCount) return;
    sVEC3 d = dir;
    Vec3Normalize(&d, &d);
    aData.lights[index].direction[0] = d.x;
    aData.lights[index].direction[1] = d.y;
    aData.lights[index].direction[2] = d.z;
    aDirty = true;
}

void cLIGHT::Remove(int index) {
    if(index < 0 || index >= (int)aCount) return;
    // 将后面的光源前移
    for(uint32_t i = index; i + 1 < aCount; ++i) {
        aData.lights[i] = aData.lights[i + 1];
    }
    aCount--;
    aData.lightCount = (float)aCount;
    aDirty = true;
}

void cLIGHT::SetAmbient(const sVEC3 &color, float intensity) {
    aData.ambientLight[0] = color.x;
    aData.ambientLight[1] = color.y;
    aData.ambientLight[2] = color.z;
    aData.ambientLight[3] = intensity;
    aDirty = true;
}

void cLIGHT::SetShadowLightVP(const sMAT44 &lightVP, uint32_t cascade) {
    if(cascade >= GN_MAX_SHADOW_CASCADES) cascade = GN_MAX_SHADOW_CASCADES - 1;
    memcpy(aData.shadowLightVP[cascade], lightVP.m, sizeof(aData.shadowLightVP[cascade]));
    aDirty = true;
}

void cLIGHT::SetShadowMapSize(float size) {
    aData.shadowParams[0] = size;
    aDirty = true;
}

void cLIGHT::SetShadowCascades(uint32_t count, const float *splits, const float *texelWorld) {
    if(count < 1) count = 1;
    if(count > GN_MAX_SHADOW_CASCADES) count = GN_MAX_SHADOW_CASCADES;
    aData.shadowParams[1] = (float)count;
    for(uint32_t i = 0; i < GN_MAX_SHADOW_CASCADES; ++i) {
        aData.cascadeSplits[i] = (splits && i < count) ? splits[i] : 0.0f;
        aData.shadowTexelWorld[i] = (texelWorld && i < count) ? texelWorld[i] : 0.0f;
    }
    aDirty = true;
}

// ═══════════════════════════════════════════════════════════
//  预置光照方案
// ═══════════════════════════════════════════════════════════

cLIGHT *CreateThreePointLighting(const sVEC3 &targetPos) {
    cLIGHT *lt = new cLIGHT();
    // Key light (主光) — 从右上方向
    lt->AddDirectional(sVEC3{-0.5f, -1.0f, -0.5f}, sVEC3{1.0f, 0.95f, 0.9f}, 1.0f);
    // Fill light (补光) — 从左侧
    lt->AddDirectional(sVEC3{0.8f, -0.3f, 0.0f}, sVEC3{0.4f, 0.5f, 0.7f}, 0.4f);
    // Back/Rim light (轮廓光) — 从后方
    lt->AddDirectional(sVEC3{0.0f, -0.5f, 1.0f}, sVEC3{0.8f, 0.7f, 0.6f}, 0.5f);
    lt->SetAmbient(sVEC3{0.1f, 0.1f, 0.15f}, 1.0f);
    return lt;
}

cLIGHT *CreateOutdoorLighting() {
    cLIGHT *lt = new cLIGHT();
    // 太阳光
    lt->AddDirectional(sVEC3{0.3f, -1.0f, 0.5f}, sVEC3{1.0f, 0.95f, 0.85f}, 1.2f);
    // 天空散射光 (从上方)
    lt->AddDirectional(sVEC3{0.0f, -1.0f, 0.0f}, sVEC3{0.3f, 0.4f, 0.6f}, 0.6f);
    lt->SetAmbient(sVEC3{0.2f, 0.25f, 0.35f}, 1.0f);
    return lt;
}


// ═══════════════════════════════════════════════════════════
//  B2c：多光源 cube shadow 的选灯
// ═══════════════════════════════════════════════════════════
sCUBE_SHADOW_PICK::sCUBE_SHADOW_PICK() : aLightIndex(-1), aScore(0.0f) {}

sCUBE_SHADOW_PICK pickCubeShadowLight(const sLIGHT_UBO &light, const GNg::sVEC3 &cameraPos,
                                      float maxDistance) {
    sCUBE_SHADOW_PICK pick;
    if(!(maxDistance > 0.0f)) maxDistance = 40.0f;
    const int count = (int)light.lightCount;
    if(count <= 0) return pick;
    if(count > GN_MAX_LIGHTS) return pick; // 越界视为数据损坏，宁可不选也不读脏

    float bestScore = 0.0f;
    int bestIndex = -1;
    for(int i = 0; i < count; ++i) {
        const sLIGHT_GPU &L = light.lights[i];
        // 只考虑"开了 cube shadow 的点光/聚光"：shadowType == 2
        if(L.direction[3] < 1.5f || L.direction[3] > 2.5f) continue;
        // 方向光没有位置（position.w < 0.5），不参与 cube shadow
        if(L.position[3] < 0.5f) continue;

        const float dx = L.position[0] - cameraPos.x;
        const float dy = L.position[1] - cameraPos.y;
        const float dz = L.position[2] - cameraPos.z;
        const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
        if(distance > maxDistance) continue; // 太远：阴影只有几个像素

        // 评分：距离越近越好（1/(1+d) 而不是 1/d —— 避免 d→0 时爆掉导致永远选同一盏），
        // 强度按平方根参与（一盏 4 倍亮的光不该拿 4 倍权重，否则亮度差异会盖过位置差异）。
        const float intensity = L.color[3] > 0.0f ? L.color[3] : 0.0f;
        const float proximity = 1.0f / (1.0f + distance * 0.08f);
        const float score = proximity * std::sqrt(intensity);
        if(!(score > bestScore)) continue; // 严格大于：并列时**先注册的赢**，行为可预测
        bestScore = score;
        bestIndex = i;
    }
    pick.aLightIndex = bestIndex;
    pick.aScore = bestScore;
    return pick;
}

// ═══════════════════════════════════════════════════════════
//  多灯选择
// ═══════════════════════════════════════════════════════════
sCUBE_SHADOW_PICKS::sCUBE_SHADOW_PICKS() : aCount(0) {
    for(uint32_t i = 0; i < GN_MAX_CUBE_SHADOW_PICKS; ++i) {
        aLightIndex[i] = -1;
        aScore[i] = 0.0f;
    }
}

sCUBE_SHADOW_PICKS pickCubeShadowLights(const sLIGHT_UBO &light, const GNg::sVEC3 &cameraPos,
                                        float maxDistance, uint32_t maxCount) {
    sCUBE_SHADOW_PICKS picks;
    if(!(maxDistance > 0.0f)) maxDistance = 40.0f;
    if(maxCount > GN_MAX_CUBE_SHADOW_PICKS) maxCount = GN_MAX_CUBE_SHADOW_PICKS;
    if(maxCount == 0) return picks;

    const int count = (int)light.lightCount;
    if(count <= 0 || count > GN_MAX_LIGHTS) return picks;

    // 先算全部候选的分数，再取前 N 名。
    // 用"插入排序进结果数组"而不是 sort 整个列表：N ≤ 4，插入更省且稳定
    // （稳定 = 并列时先注册的在前，与单灯版一致）。
    for(int i = 0; i < count; ++i) {
        const sLIGHT_GPU &L = light.lights[i];
        if(L.direction[3] < 1.5f || L.direction[3] > 2.5f) continue; // 只有 cube shadow 的灯
        if(L.position[3] < 0.5f) continue; // 方向光没有位置

        const float dx = L.position[0] - cameraPos.x;
        const float dy = L.position[1] - cameraPos.y;
        const float dz = L.position[2] - cameraPos.z;
        const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
        if(distance > maxDistance) continue;

        const float intensity = L.color[3] > 0.0f ? L.color[3] : 0.0f;
        const float proximity = 1.0f / (1.0f + distance * 0.08f);
        const float score = proximity * std::sqrt(intensity);
        if(!(score > 0.0f)) continue;

        // 找到插入位置：保持降序，**严格大于**才往后挪（并列时已有的在前）
        uint32_t pos = picks.aCount;
        for(uint32_t k = 0; k < picks.aCount; ++k) {
            if(score > picks.aScore[k]) {
                pos = k;
                break;
            }
        }
        if(pos >= maxCount) continue; // 进不了前 N 名

        // 后移一位
        const uint32_t last = (picks.aCount < maxCount) ? picks.aCount : (maxCount - 1);
        for(uint32_t k = last; k > pos; --k) {
            picks.aLightIndex[k] = picks.aLightIndex[k - 1];
            picks.aScore[k] = picks.aScore[k - 1];
        }
        picks.aLightIndex[pos] = i;
        picks.aScore[pos] = score;
        if(picks.aCount < maxCount) ++picks.aCount;
    }
    return picks;
}

} // namespace GN3d