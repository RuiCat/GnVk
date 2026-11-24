// tests/imoScene/imoSceneShadow.cpp — 方向光 CSM：加阴影光、每帧拟合级联、画深度图。
//
// 室内只有角色投影：房间是封闭的，若墙面也参与投影，方向光会被自家墙/天花板挡死，
// 室内整片变黑；因此阴影 pass 只画角色，房间继续靠 SH9 环境光 + 吸顶点光照明。
// 切级/近平面/分割口径与 tests/mmdViewer 一致（契约 §8）。

#include "imoSceneShadow.h"

#include <cmath>
#include <cstdlib>

#include "GN_Vk.h"
#include "GN_ShadowCascade.h"

namespace imoScene {

namespace {

// 与引擎分配的阴影图一致：2048² × 4 层（vk3dInit.cpp 的 kShadowCascadeCount）。
const uint32_t ENGINE_SHADOW_MAP_SIZE = 2048;
// 阴影承接区：角色 AABB 向外扩的余量（米），让脚下地面进入拟合盒。
const float CATCHER_MARGIN = 2.5f;

// P1：级联结果收进引擎的 sSHADOW_STATE（原先这里 4 个散装全局量 + 一个逐字段搬运循环）。
GN3d::sSHADOW_STATE gCascadeState;

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

// 拟合策略（far 口径、承接区外扩、抬到地面以上、级联数钳制、写 sLIGHT_UBO）
// 已收进引擎 GN3d::fitShadowCascades；原先这里有一份 74 行的私有实现，
// 与 tests/mmdViewer 里的副本逐字相同（那 22 行 farZ 循环完全一样）。
bool shadowFit(GN3d::cLIGHT *light, const GNg::sMAT44 &cameraView, const GNg::sVEC3 &cameraPos,
               float fovYRad, float aspect, const GN3d::sAABB &casterBox,
               const sShadowOptions &options) {
    gCascadeState.Clear();
    if (!options.aEnabled) {
        return false;
    }
    GN3d::sSHADOW_FIT_DESC desc;
    desc.aCameraView = cameraView;
    desc.aCameraPos = cameraPos;
    desc.aFovYRad = fovYRad;
    desc.aAspect = aspect;
    desc.aLightDirection = normalizeDirection(options.aDirection);
    desc.aCasterBox = casterBox;
    desc.aCatchMargin = CATCHER_MARGIN;   // 让脚下地面进入拟合盒
    desc.aClampToGround = true;           // 地板建在 y=0，拟合盒不要沉到地下
    desc.aCascadeCount = options.aCascadeCount;
    desc.aShadowMapSize = options.aMapSize;
    desc.aSplitLambda = 0.5f;
    desc.aTexelSnapping = true;
    desc.aEngineCascadeLayers = GNg::GN_VkShadowCascadeCount();

    return GN3d::fitShadowState(desc, light, &gCascadeState);
}

GNg::sMAT44 shadowCascadeViewProj(uint32_t index) {
    if (index >= GN3d::SHADOW_CASCADE_MAX) {
        GNg::sMAT44 identity;
        GNg::Mat44Identity(&identity);
        return identity;
    }
    return gCascadeState.aLightViewProj[index];
}

uint32_t shadowCascadeCount() {
    return gCascadeState.aCount;
}

void shadowDrawCaster(const GN3d::cMODEL &caster) {
    // 逐级 Begin/End 的循环已收进引擎（与 mmdViewer 用的是同一个函数）。
    GNg::GN_VkDrawShadowCascades(caster, gCascadeState.aLightViewProj, gCascadeState.aCount);
}

} // namespace imoScene
