// tests/imoScene/imoSceneShadow.cpp — 方向光 CSM：加阴影光、每帧拟合级联、画深度图。
//
// 室内只有角色投影：房间是封闭的，若墙面也参与投影，方向光会被自家墙/天花板挡死，
// 室内整片变黑；因此阴影 pass 只画角色，房间继续靠 SH9 环境光 + 吸顶点光照明。
// 切级/近平面/分割口径与 tests/mmdViewer 一致（契约 §8）。

#include "imoSceneShadow.h"

#include <cmath>
#include <cstdlib>

#include "GN_Vk.h"
#include "vkShadowCascade.h"

namespace imoScene {

namespace {

// 与引擎分配的阴影图一致：2048² × 4 层（vk3dInit.cpp 的 kShadowCascadeCount）。
const uint32_t ENGINE_SHADOW_MAP_SIZE = 2048;
const float SHADOW_NEAR = 0.1f;
// 阴影承接区：角色 AABB 向外扩的余量（米），让脚下地面进入拟合盒。
const float CATCHER_MARGIN = 2.5f;

GNg::sMAT44 gCascadeViewProj[GN3d::SHADOW_CASCADE_MAX];
float gCascadeSplits[GN3d::SHADOW_CASCADE_MAX];
float gCascadeTexelWorld[GN3d::SHADOW_CASCADE_MAX];
uint32_t gCascadeCount = 0;

// 归一化光线方向；零向量回退成竖直向下。
GNg::sVEC3 normalizeDirection(const GNg::sVEC3 &direction) {
    const float length =
        std::sqrt(direction.x * direction.x + direction.y * direction.y + direction.z * direction.z);
    if (!(length > 1.0e-5f)) {
        return GNg::sVEC3{0.0f, -1.0f, 0.0f};
    }
    return GNg::sVEC3{direction.x / length, direction.y / length, direction.z / length};
}

} // namespace

sShadowOptions::sShadowOptions()
    : aEnabled(true), aIntensity(0.45f), aDirection{0.12f, -1.0f, 0.10f},
      aCascadeCount(4), aMapSize(ENGINE_SHADOW_MAP_SIZE) {}

sShadowOptions shadowOptionsFromEnv() {
    sShadowOptions options;
    // 默认开启（E1/E2 修复后）；IMO_SCENE_SHADOWS=0 关闭，保留作调试/回退开关。
    const char *enabled = std::getenv("IMO_SCENE_SHADOWS");
    if (enabled != 0 && enabled[0] == '0') {
        options.aEnabled = false;
    }
    const char *intensity = std::getenv("IMO_SCENE_SHADOW_INTENSITY");
    if (intensity != 0 && intensity[0] != '\0') {
        options.aIntensity = static_cast<float>(std::atof(intensity));
    }
    return options;
}

bool shadowAttachLight(GN3d::cLIGHT *light, const sShadowOptions &options) {
    if (light == 0 || !options.aEnabled) {
        return false;
    }
    const GNg::sVEC3 direction = normalizeDirection(options.aDirection);
    const GNg::sVEC3 color = {1.0f, 0.96f, 0.90f};
    if (light->AddDirectional(direction, color, options.aIntensity, true) < 0) {
        return false;
    }
    return true;
}

bool shadowFit(GN3d::cLIGHT *light, const GNg::sMAT44 &cameraView, const GNg::sVEC3 &cameraPos,
               float fovYRad, float aspect, const GN3d::sAABB &casterBox,
               const sShadowOptions &options) {
    gCascadeCount = 0;
    if (!options.aEnabled || !(aspect > 0.0f) || !(fovYRad > 0.0f) || !casterBox.IsValid()) {
        return false;
    }
    const GN3d::sAABB catchBox(casterBox.aMin, casterBox.aMax);
    GN3d::sAABB sceneBox = catchBox;
    sceneBox.Expand(CATCHER_MARGIN);
    if (sceneBox.aMin.y < 0.0f) {
        sceneBox.aMin.y = 0.0f; // 地面高度 0：不要把拟合盒放到地板以下
    }
    const GNg::sVEC3 extent = sceneBox.Extent();
    const float diagonal =
        2.0f * std::sqrt(extent.x * extent.x + extent.y * extent.y + extent.z * extent.z);
    // far 口径（契约 §8）：max(盒对角, 相机到盒最远角距离 + 0.5)；只取对角时相机站在
    // 场景外会让各级切片全落在相机与角色之间，阴影整片消失。
    float farthestCorner = 0.0f;
    for (uint32_t corner = 0; corner < 8; ++corner) {
        const GNg::sVEC3 point = {(corner & 1u) ? sceneBox.aMax.x : sceneBox.aMin.x,
                                  (corner & 2u) ? sceneBox.aMax.y : sceneBox.aMin.y,
                                  (corner & 4u) ? sceneBox.aMax.z : sceneBox.aMin.z};
        const float dx = point.x - cameraPos.x;
        const float dy = point.y - cameraPos.y;
        const float dz = point.z - cameraPos.z;
        const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (distance > farthestCorner) {
            farthestCorner = distance;
        }
    }
    float farZ = diagonal > farthestCorner + 0.5f ? diagonal : farthestCorner + 0.5f;
    if (farZ < 1.0f) {
        farZ = 1.0f;
    }

    GN3d::sSHADOW_CASCADE_DESC desc;
    desc.aCameraView = cameraView;
    desc.aFovYRad = fovYRad;
    desc.aAspect = aspect;
    desc.aNear = SHADOW_NEAR;
    desc.aFar = farZ;
    desc.aLightDirection = normalizeDirection(options.aDirection);
    desc.aSceneAABB = sceneBox;
    desc.aCascadeCount = options.aCascadeCount;
    const uint32_t layerCount = GNg::GN_VkShadowCascadeCount();
    if (desc.aCascadeCount > layerCount) {
        desc.aCascadeCount = layerCount;
    }
    if (desc.aCascadeCount < 1u) {
        desc.aCascadeCount = 1u;
    }
    desc.aShadowMapSize = options.aMapSize;
    desc.aSplitLambda = 0.5f;
    desc.aTexelSnapping = true;

    GN3d::sSHADOW_CASCADE_RESULT result;
    if (!GN3d::buildShadowCascades(desc, &result)) {
        return false;
    }
    for (uint32_t index = 0; index < result.aCount; ++index) {
        gCascadeViewProj[index] = result.aLightViewProj[index];
        gCascadeSplits[index] = result.aSplits[index];
        gCascadeTexelWorld[index] = result.aTexelWorld[index];
    }
    gCascadeCount = result.aCount;
    if (light != 0) {
        for (uint32_t index = 0; index < gCascadeCount; ++index) {
            light->SetShadowLightVP(gCascadeViewProj[index], index);
        }
        light->SetShadowMapSize(static_cast<float>(ENGINE_SHADOW_MAP_SIZE));
        light->SetShadowCascades(gCascadeCount, gCascadeSplits, gCascadeTexelWorld);
    }
    return gCascadeCount > 0;
}

GNg::sMAT44 shadowCascadeViewProj(uint32_t index) {
    if (index >= GN3d::SHADOW_CASCADE_MAX) {
        GNg::sMAT44 identity;
        GNg::Mat44Identity(&identity);
        return identity;
    }
    return gCascadeViewProj[index];
}

uint32_t shadowCascadeCount() {
    return gCascadeCount;
}

void shadowDrawCaster(const GN3d::cMODEL &caster) {
    for (uint32_t index = 0; index < gCascadeCount; ++index) {
        GNg::GN_VkShadowPassBegin(&gCascadeViewProj[index], index);
        caster.DrawShadow();
        GNg::GN_VkShadowPassEnd();
    }
}

} // namespace imoScene
