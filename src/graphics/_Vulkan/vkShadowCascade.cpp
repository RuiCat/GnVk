// src/graphics/_Vulkan/vkShadowCascade.cpp — L3c CSM（CPU）实现：
// 对数/均匀混合分割 → 相机视锥角点 → 与场景 AABB 求交 → 光源空间正交拟合 → texel snapping。
// 纯 CPU，不碰 Vulkan 资源；输出契约见 docs/L4_L3_CONTRACT.md §8。
#include <GN.h> // 仓库惯例：先建 GN_API / 模块宏，再包含本项目图形头
#include "vkShadowCascade.h"

#include <cmath>
#include <cstring>

namespace GN3d {

namespace {

// 退化保护：拟合盒最小半尺寸（m）与 texel 最小步长（m）。
const float SHADOW_CASCADE_MIN_HALF = 1.0e-3f;
const float SHADOW_CASCADE_MIN_TEXEL = 1.0e-5f;
// 光空间近/远额外余量：固定 5 cm 覆盖深度量化，另按半径 2% 覆盖贴合误差。
const float SHADOW_CASCADE_DEPTH_PAD_MIN = 0.05f;
const float SHADOW_CASCADE_DEPTH_PAD_RATIO = 0.02f;

float clampFloat(float value, float low, float high) {
    if (!(value == value)) { // NaN
        return low;
    }
    if (value < low) {
        return low;
    }
    return value > high ? high : value;
}

uint32_t clampCascadeCount(uint32_t count) {
    if (count < 1u) {
        return 1u;
    }
    return count > SHADOW_CASCADE_MAX ? SHADOW_CASCADE_MAX : count;
}

uint32_t clampShadowMapSize(uint32_t size) {
    if (size < 16u) {
        return 16u;
    }
    return size > 8192u ? 8192u : size;
}

bool matrixIsFinite(const GNg::sMAT44 &matrix) {
    for (int i = 0; i < 16; ++i) {
        if (!std::isfinite(matrix.m[i])) {
            return false;
        }
    }
    return true;
}

// 仿射变换（w 恒为 1）：世界点 → 视图 / 光空间。
void transformPoint(const GNg::sMAT44 &matrix, const GNg::sVEC3 &point, GNg::sVEC3 *out) {
    out->x = matrix.m[0] * point.x + matrix.m[4] * point.y + matrix.m[8] * point.z + matrix.m[12];
    out->y = matrix.m[1] * point.x + matrix.m[5] * point.y + matrix.m[9] * point.z + matrix.m[13];
    out->z = matrix.m[2] * point.x + matrix.m[6] * point.y + matrix.m[10] * point.z + matrix.m[14];
}

// 相机视锥某一深度切片的 4 角点（视图空间 (x,y,-z) → 世界空间，共 8 个）。
void frustumCorners(const GNg::sMAT44 &inverseView, float fovYRad, float aspect, float zNear,
                    float zFar, GNg::sVEC3 outCorners[8]) {
    const float tanHalf = std::tan(fovYRad * 0.5f);
    const float distances[2] = {zNear, zFar};
    uint32_t index = 0;
    for (uint32_t plane = 0; plane < 2; ++plane) {
        const float halfHeight = tanHalf * distances[plane];
        const float halfWidth = halfHeight * aspect;
        const float xs[2] = {-halfWidth, halfWidth};
        const float ys[2] = {-halfHeight, halfHeight};
        for (uint32_t yIndex = 0; yIndex < 2; ++yIndex) {
            for (uint32_t xIndex = 0; xIndex < 2; ++xIndex) {
                const GNg::sVEC3 viewPoint = {xs[xIndex], ys[yIndex], -distances[plane]};
                transformPoint(inverseView, viewPoint, &outCorners[index]);
                ++index;
            }
        }
    }
}

void boxFromPoints(const GNg::sVEC3 *points, uint32_t count, sAABB *out) {
    out->Reset();
    for (uint32_t i = 0; i < count; ++i) {
        out->Expand(points[i]);
    }
}

// 两个 AABB 的交；无交（或结果退化到无效）返回 false。
bool intersectBox(const sAABB &a, const sAABB &b, sAABB *out) {
    sAABB box;
    box.aMin.x = a.aMin.x > b.aMin.x ? a.aMin.x : b.aMin.x;
    box.aMin.y = a.aMin.y > b.aMin.y ? a.aMin.y : b.aMin.y;
    box.aMin.z = a.aMin.z > b.aMin.z ? a.aMin.z : b.aMin.z;
    box.aMax.x = a.aMax.x < b.aMax.x ? a.aMax.x : b.aMax.x;
    box.aMax.y = a.aMax.y < b.aMax.y ? a.aMax.y : b.aMax.y;
    box.aMax.z = a.aMax.z < b.aMax.z ? a.aMax.z : b.aMax.z;
    if (!box.IsValid()) {
        return false;
    }
    *out = box;
    return true;
}

// 8 个体素盒角点。
void boxCorners(const sAABB &box, GNg::sVEC3 outCorners[8]) {
    uint32_t index = 0;
    for (uint32_t zIndex = 0; zIndex < 2; ++zIndex) {
        for (uint32_t yIndex = 0; yIndex < 2; ++yIndex) {
            for (uint32_t xIndex = 0; xIndex < 2; ++xIndex) {
                outCorners[index].x = xIndex == 0 ? box.aMin.x : box.aMax.x;
                outCorners[index].y = yIndex == 0 ? box.aMin.y : box.aMax.y;
                outCorners[index].z = zIndex == 0 ? box.aMin.z : box.aMax.z;
                ++index;
            }
        }
    }
}

} // namespace

sSHADOW_CASCADE_DESC::sSHADOW_CASCADE_DESC()
    : aCameraView(true), aFovYRad(1.0471975511965976f), aAspect(1.0f), aNear(0.1f), aFar(100.0f),
      aCascadeCount(SHADOW_CASCADE_MAX), aShadowMapSize(2048u), aSplitLambda(0.5f),
      aTexelSnapping(true) {
    aLightDirection = GNg::sVEC3{0.0f, -1.0f, 0.0f};
}

sSHADOW_CASCADE_RESULT::sSHADOW_CASCADE_RESULT() {
    std::memset(this, 0, sizeof(*this));
}

void computeCascadeSplits(float nearZ, float farZ, uint32_t count, float lambda,
                          float outSplits[SHADOW_CASCADE_MAX]) {
    if (!outSplits) {
        return;
    }
    const uint32_t cascadeCount = clampCascadeCount(count);
    const float mix = clampFloat(lambda, 0.0f, 1.0f);
    for (uint32_t i = 0; i < SHADOW_CASCADE_MAX; ++i) {
        outSplits[i] = 0.0f;
    }
    if (!std::isfinite(nearZ) || !(nearZ > 0.0f) || !std::isfinite(farZ) || !(farZ > nearZ)) {
        return;
    }
    const float ratio = farZ / nearZ;
    for (uint32_t i = 0; i < cascadeCount; ++i) {
        const float t = static_cast<float>(i + 1) / static_cast<float>(cascadeCount);
        const float logarithmic = nearZ * std::pow(ratio, t);
        const float uniform = nearZ + (farZ - nearZ) * t;
        outSplits[i] = logarithmic + (uniform - logarithmic) * mix;
    }
}

bool buildShadowCascades(const sSHADOW_CASCADE_DESC &desc, sSHADOW_CASCADE_RESULT *out) {
    if (!out) {
        return false;
    }
    if (!desc.aSceneAABB.IsValid()) {
        return false;
    }
    if (!std::isfinite(desc.aFovYRad) || !(desc.aFovYRad > 0.001f) || desc.aFovYRad >= 3.1f) {
        return false;
    }
    if (!std::isfinite(desc.aAspect) || !(desc.aAspect > 1.0e-4f)) {
        return false;
    }
    if (!std::isfinite(desc.aNear) || !(desc.aNear > 1.0e-4f)) {
        return false;
    }
    if (!std::isfinite(desc.aFar) || !(desc.aFar > desc.aNear + 1.0e-4f)) {
        return false;
    }

    const uint32_t cascadeCount = clampCascadeCount(desc.aCascadeCount);
    const uint32_t shadowMapSize = clampShadowMapSize(desc.aShadowMapSize);
    const float splitMix = clampFloat(desc.aSplitLambda, 0.0f, 1.0f);

    // 光线传播方向（指向场景）；零向量回退顶光，避免 LookAt 叉积塌成 NaN。
    GNg::sVEC3 lightDirection = desc.aLightDirection;
    const float directionLength = std::sqrt(GNg::Vec3LengthSq(&lightDirection));
    if (!std::isfinite(directionLength) || directionLength < 1.0e-6f) {
        lightDirection = GNg::sVEC3{0.0f, -1.0f, 0.0f};
    } else {
        const float inverseLength = 1.0f / directionLength;
        lightDirection.x *= inverseLength;
        lightDirection.y *= inverseLength;
        lightDirection.z *= inverseLength;
    }

    GNg::sMAT44 inverseView;
    GNg::Mat44Inverse(&inverseView, &desc.aCameraView);
    if (!matrixIsFinite(inverseView)) {
        return false;
    }

    // 光源视图只保留旋转（锚点固定为世界原点）：texel snapping 因此在世界空间稳定，
    // 相机连续移动只会让拟合盒按整 texel 跳格，不会逐帧抖动。光源眼睛放在原点等价于
    // 「正交相机无透视」，深度由正交矩阵的 near/far 承担。
    const GNg::sVEC3 lightEye = {0.0f, 0.0f, 0.0f};
    const GNg::sVEC3 lightTarget = {-lightDirection.x, -lightDirection.y, -lightDirection.z};
    const GNg::sVEC3 lightUp = (lightDirection.y > 0.99f || lightDirection.y < -0.99f)
                                   ? GNg::sVEC3{0.0f, 0.0f, 1.0f}
                                   : GNg::sVEC3{0.0f, 1.0f, 0.0f};
    GNg::sMAT44 lightView;
    GNg::Mat44LookAtRH(&lightView, &lightEye, &lightTarget, &lightUp);
    if (!matrixIsFinite(lightView)) {
        return false;
    }

    float splits[SHADOW_CASCADE_MAX] = {0.0f, 0.0f, 0.0f, 0.0f};
    computeCascadeSplits(desc.aNear, desc.aFar, cascadeCount, splitMix, splits);

    sSHADOW_CASCADE_RESULT result;
    GNg::sVEC3 cascadeCorners[8];
    GNg::sVEC3 worldCorners[8];
    GNg::sVEC3 lightCorners[8];

    for (uint32_t cascade = 0; cascade < cascadeCount; ++cascade) {
        const float sliceNear = cascade == 0 ? desc.aNear : splits[cascade - 1];
        const float sliceFar = splits[cascade];
        if (!(sliceFar > sliceNear)) {
            continue; // splits 升序时不可达；保底不让非法级写出矩阵
        }
        frustumCorners(inverseView, desc.aFovYRad, desc.aAspect, sliceNear, sliceFar,
                       cascadeCorners);
        sAABB frustumBox;
        boxFromPoints(cascadeCorners, 8, &frustumBox);

        // 契约 §8「以场景 AABB 求交/包裹」：求交把拟合盒收紧到该级真有几何的范围；
        // 该级没有场景几何（求交为空）时退回视锥盒本身，矩阵仍合法（该区域本无投影物）。
        sAABB fitBox;
        if (!intersectBox(frustumBox, desc.aSceneAABB, &fitBox)) {
            fitBox = frustumBox;
        }
        if (!fitBox.IsValid()) {
            continue;
        }

        boxCorners(fitBox, worldCorners);
        for (uint32_t i = 0; i < 8; ++i) {
            transformPoint(lightView, worldCorners[i], &lightCorners[i]);
        }
        float minX = lightCorners[0].x;
        float maxX = lightCorners[0].x;
        float minY = lightCorners[0].y;
        float maxY = lightCorners[0].y;
        float minZ = lightCorners[0].z;
        float maxZ = lightCorners[0].z;
        for (uint32_t i = 1; i < 8; ++i) {
            minX = lightCorners[i].x < minX ? lightCorners[i].x : minX;
            maxX = lightCorners[i].x > maxX ? lightCorners[i].x : maxX;
            minY = lightCorners[i].y < minY ? lightCorners[i].y : minY;
            maxY = lightCorners[i].y > maxY ? lightCorners[i].y : maxY;
            minZ = lightCorners[i].z < minZ ? lightCorners[i].z : minZ;
            maxZ = lightCorners[i].z > maxZ ? lightCorners[i].z : maxZ;
        }

        // 光空间 x/y 取正方形（阴影图是正方形，非均匀尺度会让 texel 各向异性）。
        float halfExtent = (maxX - minX) * 0.5f;
        const float halfExtentY = (maxY - minY) * 0.5f;
        if (halfExtentY > halfExtent) {
            halfExtent = halfExtentY;
        }
        if (!std::isfinite(halfExtent) || !(halfExtent > SHADOW_CASCADE_MIN_HALF)) {
            halfExtent = SHADOW_CASCADE_MIN_HALF;
        }
        const float texelWorld = 2.0f * halfExtent / static_cast<float>(shadowMapSize);

        float centerX = (minX + maxX) * 0.5f;
        float centerY = (minY + maxY) * 0.5f;
        if (desc.aTexelSnapping && texelWorld > SHADOW_CASCADE_MIN_TEXEL) {
            // L3a：把拟合盒中心平移到该级 texel 世界尺寸的整数倍（平移 ≤ 0.5 texel）。
            centerX = std::floor(centerX / texelWorld + 0.5f) * texelWorld;
            centerY = std::floor(centerY / texelWorld + 0.5f) * texelWorld;
        }

        const GNg::sVEC3 fitExtent = fitBox.Extent();
        const float fitRadius =
            std::sqrt(fitExtent.x * fitExtent.x + fitExtent.y * fitExtent.y +
                      fitExtent.z * fitExtent.z);
        const float depthPad = SHADOW_CASCADE_DEPTH_PAD_MIN + fitRadius * SHADOW_CASCADE_DEPTH_PAD_RATIO;
        float lightNear = -maxZ - depthPad;
        float lightFar = -minZ + depthPad;
        if (!(lightFar > lightNear + 1.0e-4f)) {
            lightFar = lightNear + 1.0e-4f;
        }

        GNg::sMAT44 projection;
        GNg::Mat44OrthoVK(&projection, centerX - halfExtent, centerX + halfExtent,
                          centerY - halfExtent, centerY + halfExtent, lightNear, lightFar);
        GNg::sMAT44 viewProjection;
        GNg::Mat44Mul(&viewProjection, &projection, &lightView);

        result.aSplits[cascade] = splits[cascade];
        result.aTexelWorld[cascade] = texelWorld;
        if (matrixIsFinite(viewProjection)) {
            result.aLightViewProj[cascade] = viewProjection;
        }
    }

    result.aCount = cascadeCount;
    result.aShadowMapSize = shadowMapSize;
    *out = result;
    return true;
}

} // namespace GN3d
