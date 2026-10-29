// ╔══════════════════════════════════════════════════════════╗
// ║  GN_mini — 坐标空间约定实现（CPU，无 Vulkan 依赖）        ║
// ╚══════════════════════════════════════════════════════════╝
#include <GN.h>
#include "GN_Space.h"

#include <cmath>

namespace GN3d {

const GNg::sVEC3 SPACE_WORLD_RIGHT = {1.0f, 0.0f, 0.0f};
const GNg::sVEC3 SPACE_WORLD_UP = {0.0f, 1.0f, 0.0f};
const GNg::sVEC3 SPACE_CAMERA_FORWARD = {0.0f, 0.0f, -1.0f};
const GNg::sVEC3 SPACE_ASSET_FRONT = {0.0f, 0.0f, 1.0f};
const float SPACE_MMD_UNIT_SCALE = 1.0f;

eSPACE_FACE frontFaceForWorldPass(bool projectionFlipsY) {
    // 世界内容统一 CCW。正高度视口下，投影 Y 翻转把内容摆正（消除镜像），
    // 视觉绕序 = 世界绕序 = CCW（Vulkan 在 +Y 向下的帧缓冲坐标里判定）⇒ 用 CCW。
    // 不翻 Y 时内容被垂直镜像，CCW 内容呈 CW ⇒ 用 CW。
    // 实测依据：tests/gpu 在 llvmpipe 上按旧规则（CW）跑出 18 例失败，其中
    // ground.placement/ground.pbr 用真实透视（含 Y 翻转）+ 世界 CCW 内容被判为背面剔除。
    return projectionFlipsY ? eSPACE_FACE_CCW : eSPACE_FACE_CW;
}

eSPACE_FACE frontFaceForScreenPass() {
    return eSPACE_FACE_CW;
}

bool isValidViewportSize(int width, int height) {
    return width > 0 && height > 0;
}

bool reverseWinding(uint32_t *indices, uint32_t indexCount) {
    if (indexCount == 0) {
        return true;
    }
    if (!indices || (indexCount % 3u) != 0u) {
        return false;
    }
    for (uint32_t i = 0; i + 2u < indexCount; i += 3u) {
        const uint32_t tmp = indices[i + 1u];
        indices[i + 1u] = indices[i + 2u];
        indices[i + 2u] = tmp;
    }
    return true;
}

bool mirrorVectorsX(float *values, uint32_t vectorCount, uint32_t strideFloats) {
    if (vectorCount == 0) {
        return true;
    }
    if (!values || strideFloats < 3u) {
        return false;
    }
    for (uint32_t i = 0; i < vectorCount; ++i) {
        values[i * strideFloats] = -values[i * strideFloats];
    }
    return true;
}

float flipTextureV(float v) {
    if (!std::isfinite(v)) {
        return v;
    }
    return 1.0f - v;
}

GNg::sVEC3 sunDirectionToLightDirection(const GNg::sVEC3 &towardSun) {
    GNg::sVEC3 out = {-towardSun.x, -towardSun.y, -towardSun.z};
    return out;
}

GNg::sVEC3 lightDirectionToSunDirection(const GNg::sVEC3 &lightDirection) {
    GNg::sVEC3 out = {-lightDirection.x, -lightDirection.y, -lightDirection.z};
    return out;
}

} // namespace GN3d
