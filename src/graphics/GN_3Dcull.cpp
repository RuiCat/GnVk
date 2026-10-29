// GN_3Dcull.cpp — cMODEL 世界 AABB 与视锥剔除（CPU 侧；渲染器接入留 P8 的 GPU 验证）。
//
// 约定：
//   * 世界 AABB = 所有网格顶点（VkVertex3D 布局，取前 3 个 float）经 aTransform 变换后的包围盒。
//   * 可见性 = aVisible && 世界 AABB 与 sFRUSTUM 相交（保守：贴面算相交）；空盒/未计算 ⇒ 不可见。
//   * cullModels 只写 outIndices，不修改模型；最多写 maxOut 个，返回实际写入数。
//   * cullDrawModelsPBR 复用 cullModels 的可见列表，只对可见模型调用 DrawPBR。
#include <GN.h>
#include "GN_3Dmodel.h"

#include <cfloat>
#include <vector>

namespace GN3d {

bool cMODEL::computeWorldAABB() {
    if(aMeshes == 0 || aMeshCount == 0) {
        aWorldAABB = sAABB();
        return false;
    }
    float minX = FLT_MAX;
    float minY = FLT_MAX;
    float minZ = FLT_MAX;
    float maxX = -FLT_MAX;
    float maxY = -FLT_MAX;
    float maxZ = -FLT_MAX;
    bool any = false;
    for(uint32_t meshIndex = 0; meshIndex < aMeshCount; ++meshIndex) {
        const sMESH &mesh = aMeshes[meshIndex];
        if(mesh.aVertices == 0 || mesh.aVertexCount == 0) continue;
        for(uint32_t vertexIndex = 0; vertexIndex < mesh.aVertexCount; ++vertexIndex) {
            const float *vertex = mesh.aVertices + vertexIndex * 11u;
            const GNg::sVEC3 local = {vertex[0], vertex[1], vertex[2]};
            GNg::sVEC3 world;
            GNg::Mat44TransformCoord(&world, &local, &aTransform);
            if(world.x < minX) minX = world.x;
            if(world.y < minY) minY = world.y;
            if(world.z < minZ) minZ = world.z;
            if(world.x > maxX) maxX = world.x;
            if(world.y > maxY) maxY = world.y;
            if(world.z > maxZ) maxZ = world.z;
            any = true;
        }
    }
    if(!any) {
        aWorldAABB = sAABB();
        return false;
    }
    aWorldAABB.aMin.x = minX;
    aWorldAABB.aMin.y = minY;
    aWorldAABB.aMin.z = minZ;
    aWorldAABB.aMax.x = maxX;
    aWorldAABB.aMax.y = maxY;
    aWorldAABB.aMax.z = maxZ;
    return true;
}

bool cMODEL::isVisibleIn(const sFRUSTUM &frustum) const {
    if(!aVisible) return false;
    if(aWorldAABB.aMin.x > aWorldAABB.aMax.x) return false; // 空盒（未计算/无网格）
    return frustum.IntersectsAABB(aWorldAABB);
}

uint32_t cullModels(const sFRUSTUM &frustum, const cMODEL *const *models, uint32_t count,
                    uint32_t *outIndices, uint32_t maxOut) {
    if(models == 0 || outIndices == 0 || maxOut == 0) return 0;
    uint32_t visibleCount = 0;
    for(uint32_t i = 0; i < count; ++i) {
        if(models[i] == 0) continue;
        if(!models[i]->isVisibleIn(frustum)) continue;
        if(visibleCount >= maxOut) break;
        outIndices[visibleCount] = i;
        ++visibleCount;
    }
    return visibleCount;
}

uint32_t cullDrawModelsPBR(const sFRUSTUM &frustum, cMODEL *const *models, uint32_t count,
                           uint32_t *outVisible) {
    if(outVisible) *outVisible = 0;
    if(models == 0 || count == 0) return 0;

    // 复用 cullModels 的可见列表：可见下标按模型顺序写入，再逐个绘制。
    std::vector<uint32_t> visibleIndices(count);
    const uint32_t visibleCount = cullModels(frustum, models, count, visibleIndices.data(), count);
    for(uint32_t i = 0; i < visibleCount; ++i) {
        models[visibleIndices[i]]->DrawPBR();
    }
    if(outVisible) *outVisible = visibleCount;
    return visibleCount;
}

} // namespace GN3d
