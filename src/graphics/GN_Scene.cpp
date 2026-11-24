// GN_Scene.cpp — 场景级剔除 / LOD / 排序（P6）。
//
// 三条口径（都有用例，见 tests/unit/testScene.cpp）：
//   1. 排序键是"相机到实体世界 AABB 中心的距离"（不是到原点、也不是到某个顶点）：
//      AABB 中心对模型平移稳定，不会因为网格顶点分布不均而抖动。
//   2. 不透明由近到远（早写深度、挡住后面的 overdraw）；透明由远到近（混合正确）。
//      两组各自排序，不混在一起 —— 透明物体必须全部在不透明之后。
//   3. 剔除一律走 `cMESH_CONTROLLER::Prepare`（它刷新 AABB 并与视锥求交），
//      本文件**不复制一份** AABB/视锥判断：两份实现必然漂移。

#include <GN.h>
#include "GN_Scene.h"

#include <algorithm>
#include <cmath>

namespace GN3d {

sDRAW_ITEM::sDRAW_ITEM() : aEntity(nullptr), aDistance(0.0f), aTransparent(false) {}

cSCENE::cSCENE() : aPolicy(), aCull(true), aSort(true), aCulled(0), aRejected(0) {}

bool cSCENE::Add(cENTITY *entity) {
    if(entity == nullptr) return false;
    for(size_t i = 0; i < aEntities.size(); ++i) {
        if(aEntities[i] == entity) return false; // 已在场景里：忽略（避免一帧画两次）
    }
    aEntities.push_back(entity);
    return true;
}

bool cSCENE::Remove(cENTITY *entity) {
    for(size_t i = 0; i < aEntities.size(); ++i) {
        if(aEntities[i] != entity) continue;
        aEntities.erase(aEntities.begin() + (long)i);
        return true;
    }
    return false;
}

void cSCENE::Clear() {
    aEntities.clear();
    aDrawOrder.clear();
    aCulled = 0;
    aRejected = 0;
}

uint32_t cSCENE::Count() const { return (uint32_t)aEntities.size(); }

cENTITY *cSCENE::At(uint32_t index) const {
    if(index >= aEntities.size()) return nullptr;
    return aEntities[index];
}

void cSCENE::Update(float dt) {
    for(size_t i = 0; i < aEntities.size(); ++i) {
        if(aEntities[i] != nullptr) aEntities[i]->Update(dt);
    }
}

namespace {

// 两个绘制项的排序：先按透明分组（不透明在前），组内按距离。
// 不透明：近 → 远；透明：远 → 近。
bool drawItemLess(const sDRAW_ITEM &lhs, const sDRAW_ITEM &rhs) {
    if(lhs.aTransparent != rhs.aTransparent) return !lhs.aTransparent; // 不透明先
    if(lhs.aTransparent) return lhs.aDistance > rhs.aDistance; // 透明：远先
    return lhs.aDistance < rhs.aDistance; // 不透明：近先
}

} // namespace

uint32_t cSCENE::PrepareRender(const sLOD_VIEW &view, const sFRUSTUM *frustum) {
    aDrawOrder.clear();
    aCulled = 0;
    aRejected = 0;
    const GNg::sVEC3 camera = view.aCamPos;

    for(size_t i = 0; i < aEntities.size(); ++i) {
        cENTITY *entity = aEntities[i];
        if(entity == nullptr) continue;
        // 剔除 + LOD 都在控制器里：这里只拿结论，不重复实现
        const bool visible = entity->PrepareRender(view, aCull ? frustum : nullptr);
        const cMESH_CONTROLLER &mesh = entity->aMesh;
        if(mesh.Model() == nullptr) continue; // 没有网格的实体（纯逻辑实体）不参与绘制
        ++aCulled;
        if(!visible) {
            ++aRejected;
            continue;
        }

        sDRAW_ITEM item;
        item.aEntity = entity;
        const sAABB &box = mesh.Model()->aWorldAABB;
        const GNg::sVEC3 center = box.Center();
        const float dx = center.x - camera.x;
        const float dy = center.y - camera.y;
        const float dz = center.z - camera.z;
        item.aDistance = std::sqrt(dx * dx + dy * dy + dz * dz);
        item.aTransparent = mesh.HasTransparentMaterial();
        aDrawOrder.push_back(item);
    }

    if(aSort) {
        std::stable_sort(aDrawOrder.begin(), aDrawOrder.end(), drawItemLess);
    }
    return (uint32_t)aDrawOrder.size();
}

uint32_t cSCENE::VisibleCount() const { return (uint32_t)aDrawOrder.size(); }

const sDRAW_ITEM &cSCENE::DrawItem(uint32_t index) const {
    static const sDRAW_ITEM kEmpty;
    if(index >= aDrawOrder.size()) return kEmpty;
    return aDrawOrder[index];
}

const std::vector<sDRAW_ITEM> &cSCENE::DrawOrder() const { return aDrawOrder; }

uint32_t cSCENE::Draw(const GNg::sVEC3 &cameraPos) const {
    uint32_t drawn = 0;
    for(size_t i = 0; i < aDrawOrder.size(); ++i) {
        const sDRAW_ITEM &item = aDrawOrder[i];
        if(item.aEntity == nullptr) continue;
        item.aEntity->Draw(cameraPos);
        ++drawn;
    }
    return drawn;
}

uint32_t cSCENE::DrawShadow() const {
    uint32_t drawn = 0;
    for(size_t i = 0; i < aDrawOrder.size(); ++i) {
        const sDRAW_ITEM &item = aDrawOrder[i];
        if(item.aEntity == nullptr) continue;
        item.aEntity->DrawShadow();
        ++drawn;
    }
    return drawn;
}

void cSCENE::SetLodPolicy(const sLOD_POLICY &policy) { aPolicy = policy; }

const sLOD_POLICY &cSCENE::LodPolicy() const { return aPolicy; }

void cSCENE::SetCullEnabled(bool enabled) { aCull = enabled; }

bool cSCENE::CullEnabled() const { return aCull; }

void cSCENE::SetSortEnabled(bool enabled) { aSort = enabled; }

bool cSCENE::SortEnabled() const { return aSort; }

uint32_t cSCENE::LastCulledCount() const { return aCulled; }

uint32_t cSCENE::LastRejectedCount() const { return aRejected; }

} // namespace GN3d
