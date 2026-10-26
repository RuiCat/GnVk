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
    // 默认阴影贴图大小
    aData.shadowMapSize = 2048.0f;
}

void cLIGHT::Clear() {
    memset(&aData, 0, sizeof(aData));
    aData.ambientLight[0] = 0.15f;
    aData.ambientLight[1] = 0.15f;
    aData.ambientLight[2] = 0.2f;
    aData.ambientLight[3] = 1.0f;
    aData.shadowMapSize = 2048.0f; // 恢复默认阴影贴图尺寸
    aCount = 0;
    aDirty = true;
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
                     const sVEC3 &color, float intensity) {
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
    L.direction[0] = L.direction[1] = L.direction[2] = L.direction[3] = 0.0f;
    // direction.w = 0.0: no shadow (cube shadow map reserved for future)
    // 默认衰减系数 (基于 range)
    L.attenuation[0] = 1.0f;
    L.attenuation[1] = 2.0f / (range > 0.001f ? range : 10.0f);
    L.attenuation[2] = 1.0f / ((range > 0.001f ? range : 10.0f) * (range > 0.001f ? range : 10.0f));
    L.attenuation[3] = range;
    L.coneAngles[0] = L.coneAngles[1] = L.coneAngles[2] = L.coneAngles[3] = 0.0f;

    aData.lightCount = (float)aCount;
    aDirty = true;
    return idx;
}

int cLIGHT::AddSpot(const sVEC3 &pos, const sVEC3 &dir,
                    float range, float innerConeRad, float outerConeRad,
                    const sVEC3 &color, float intensity, bool enableShadow) {
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
    L.direction[3] = enableShadow ? 1.0f : 0.0f; // shadowType: 1=2D shadow, 0=none
    L.attenuation[0] = 1.0f;
    L.attenuation[1] = 2.0f / (range > 0.001f ? range : 10.0f);
    L.attenuation[2] = 1.0f / ((range > 0.001f ? range : 10.0f) * (range > 0.001f ? range : 10.0f));
    L.attenuation[3] = range;
    L.coneAngles[0] = cosf(innerConeRad);
    L.coneAngles[1] = cosf(outerConeRad);
    L.coneAngles[2] = L.coneAngles[3] = 0.0f;

    aData.lightCount = (float)aCount;
    aDirty = true;
    return idx;
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

void cLIGHT::SetShadowLightVP(const sMAT44 &lightVP) {
    memcpy(aData.shadowLightVP, lightVP.m, sizeof(aData.shadowLightVP));
    aDirty = true;
}

void cLIGHT::SetShadowMapSize(float size) {
    aData.shadowMapSize = size;
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

} // namespace GN3d
