// GN_3Dlod.cpp — P10 动态 LOD：屏幕像素误差与带滞后的选级（纯 CPU，可无 GPU 单测）。
//
// 契约见 GN_3Dlod.h：S = aMaxError * H / (2 * d * tan(fov/2))；d = 相机到世界 AABB 最近距离；
// 由粗到细取第一个可接受级；变粗需要 0.8x 余量防抖。
#include <GN.h> // 先引入引擎公共头（sVEC2/sVEC3 等基础类型），再引入 LOD 头
#include "GN_3Dlod.h"

#include <cmath>
#include <cfloat>

namespace GN3d {

namespace {

const float LOD_HYSTERESIS_RATIO = 0.8f;
const float LOD_MIN_DISTANCE = 1e-4f;
const float LOD_DEFAULT_MAX_ERROR_PX = 1.0f;

// 相机到 AABB 的最近距离；空盒/非法盒返回 0（保守：按最近处理 ⇒ 不得降级）。
float distanceToAabb(const GNg::sVEC3 &point, const sAABB &box) {
    if(!box.IsValid()) return 0.0f;
    float squared = 0.0f;
    const float clamped[3] = {
        point.x < box.aMin.x ? box.aMin.x : (point.x > box.aMax.x ? box.aMax.x : point.x),
        point.y < box.aMin.y ? box.aMin.y : (point.y > box.aMax.y ? box.aMax.y : point.y),
        point.z < box.aMin.z ? box.aMin.z : (point.z > box.aMax.z ? box.aMax.z : point.z)};
    const float delta[3] = {point.x - clamped[0], point.y - clamped[1], point.z - clamped[2]};
    for(int axis = 0; axis < 3; ++axis) squared += delta[axis] * delta[axis];
    return std::sqrt(squared);
}

bool validView(const sLOD_VIEW &view) {
    if(!(view.aScreenHeight > 0.0f) || !std::isfinite(view.aScreenHeight)) return false;
    if(!(view.aFovYRad > 0.0f) || view.aFovYRad >= 3.14159265358979323846f) return false;
    return true;
}

} // namespace

float lodScreenError(const sMESH_LOD &lod, const sAABB &worldAabb, const sLOD_VIEW &view) {
    if(!(lod.aMaxError > 0.0f)) return 0.0f; // LOD0（误差 0）永远可接受
    if(!std::isfinite(lod.aMaxError)) return INFINITY;
    if(!validView(view)) return INFINITY;
    const float distance = distanceToAabb(view.aCamPos, worldAabb);
    if(distance <= LOD_MIN_DISTANCE) return INFINITY; // 相机贴脸/在盒内：不得降级
    const float halfFovTan = std::tan(view.aFovYRad * 0.5f);
    if(!(halfFovTan > 0.0f)) return INFINITY;
    return lod.aMaxError * view.aScreenHeight / (2.0f * distance * halfFovTan);
}

uint32_t selectLod(const sMESH &mesh, const sAABB &worldAabb, const sLOD_VIEW &view,
                   float maxScreenErrorPx, uint32_t prevLevel) {
    if(mesh.aLods == 0 || mesh.aLodCount == 0) return 0; // 单级：旧行为
    const uint32_t count = mesh.aLodCount;
    const uint32_t previous = prevLevel < count ? prevLevel : count - 1;
    const float threshold =
        maxScreenErrorPx > 0.0f ? maxScreenErrorPx : LOD_DEFAULT_MAX_ERROR_PX;

    // 从粗到细找第一个 S <= 阈值（最粗的可接受级）；全不可接受时用最细级。
    uint32_t target = 0;
    for(uint32_t level = count; level-- > 0;) {
        if(lodScreenError(mesh.aLods[level], worldAabb, view) <= threshold) {
            target = level;
            break;
        }
    }

    // 由粗变细立即生效；由细变粗需要额外余量，避免阈值附近抖动。
    if(target > previous) {
        const float hysteresis = LOD_HYSTERESIS_RATIO * threshold;
        if(lodScreenError(mesh.aLods[target], worldAabb, view) <= hysteresis) return target;
        return previous;
    }
    return target;
}

uint32_t selectLodForModel(const cMODEL &model, const sLOD_VIEW &view, float maxScreenErrorPx,
                           uint32_t *levels, uint32_t levelCapacity) {
    if(!levels || levelCapacity == 0) return 0;
    const uint32_t count = model.aMeshCount < levelCapacity ? model.aMeshCount : levelCapacity;
    for(uint32_t i = 0; i < count; ++i) {
        levels[i] = selectLod(model.aMeshes[i], model.aWorldAABB, view, maxScreenErrorPx,
                              levels[i]);
    }
    return count;
}

} // namespace GN3d
