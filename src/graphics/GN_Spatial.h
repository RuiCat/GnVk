#pragma once
#ifndef _GN_SPATIAL_H_
#define _GN_SPATIAL_H_
// ╔══════════════════════════════════════════════════════════╗
// ║  GN_mini — 3D 空间划分 (AABB / BVH / 动态 AABB 树 / 视锥 / 射线)  ║
// ║  只依赖 GN_3Dmath.h，不依赖 Vulkan；可被渲染、物理、拾取共用     ║
// ╚══════════════════════════════════════════════════════════╝
//
// 契约（冻结）：本文件是 3D 空间查询的唯一公开接口，实现见
//   GN_Spatial.cpp（AABB / 视锥 / 射线）、GN_SpatialBVH.cpp（静态 BVH）、
//   GN_SpatialTree.cpp（动态 AABB 树）。
// 约定：
//   * 矩阵沿用 sMAT44 的列主序存储（Elem(col,row) = m[col*4+row]），与 Vulkan 后端一致；
//   * 视锥平面归一化，片内判定为 n·p + d >= 0；深度范围按 Vulkan NDC [0,1]；
//   * 射线-包围盒的 t 为“进入”距离，起点在盒内时 t = 0；t 超过 maxT 视为未命中；
//     dir **必须归一化**，否则 aT 不是世界距离（实现不再重复归一化）；
//   * 平行于某轴的射线依赖 IEEE 754 的 1/0 = ±inf 判平行（构建未开 -ffast-math）；
//   * 查询顺序：cBVH::Query* 结果为升序；cDYNAMIC_AABB_TREE::Query* 为遍历序（不排序）；
//   * cDYNAMIC_AABB_TREE::Insert 接受空盒（插入成功，但永远查询不命中）；Update 用向上 refit
//     保持正确性、不重插入，极端移动下树质量可能退化；
//   * sFRUSTUM 默认构造为 6 个零平面（ContainsPoint 恒真，保守）；sRAY_HIT::aIndex 初值 0xFFFFFFFF；
//   * 一切查询对空结构、退化盒（零体积）与无效句柄都不得崩溃。

#include "GN_3Dmath.h"
#include <vector>
#include <cstddef>
#include <cstdint>

namespace GN3d {

// ═══════════════════════════════════════════════════════════
//  sAABB — 轴对齐包围盒
// ═══════════════════════════════════════════════════════════
struct sAABB {
    GNg::sVEC3 aMin;
    GNg::sVEC3 aMax;

    sAABB(); // 默认是空盒（min = +FLT_MAX, max = -FLT_MAX）
    sAABB(const GNg::sVEC3 &minValue, const GNg::sVEC3 &maxValue);

    void Reset(); // 回到空盒
    bool IsValid() const; // min 各分量 <= max 各分量
    void Expand(const GNg::sVEC3 &point); // 包住一个点
    void Expand(const sAABB &box); // 包住另一个盒
    void Expand(float pad); // 六面向外扩张 pad
    GNg::sVEC3 Center() const;
    GNg::sVEC3 Extent() const; // 半尺寸 (max - min) * 0.5
    float SurfaceArea() const;
    float Volume() const;
    int LongestAxis() const; // 0/1/2 = x/y/z；空盒返回 0
    bool Overlaps(const sAABB &box) const; // 接触即算重叠（<= / >=）
    bool Contains(const GNg::sVEC3 &point) const;
    bool Contains(const sAABB &box) const;
    static sAABB FromCenterExtent(const GNg::sVEC3 &center, const GNg::sVEC3 &extent);
};

// ═══════════════════════════════════════════════════════════
//  sRAY_HIT — 射线命中结果
// ═══════════════════════════════════════════════════════════
struct sRAY_HIT {
    bool aHit; // 是否命中
    float aT; // 进入距离（起点在盒内为 0）
    GNg::sVEC3 aPoint; // 命中点
    uint32_t aIndex; // 命中对象索引：BVH 为 Build 时的原始盒序号，动态树为句柄

    sRAY_HIT();
    void Reset();
};

// ═══════════════════════════════════════════════════════════
//  sFRUSTUM — 六面视锥
// ═══════════════════════════════════════════════════════════
struct sFRUSTUM {
    GNg::sVEC4 aPlanes[6]; // 顺序：左/右/下/上/近/远，已归一化，内部 n·p + d >= 0

    sFRUSTUM();

    // 从 viewProj 提取平面（Gribb-Hartmann，Vulkan NDC z∈[0,1]）
    static sFRUSTUM FromViewProj(const GNg::sMAT44 &viewProj);

    // 相机空间 → 视锥平面的辅助：按 6 个平面逐个判定
    bool ContainsPoint(const GNg::sVEC3 &point) const;
    bool IntersectsAABB(const sAABB &box) const; // 保守测试：完全落在任一面外侧 ⇒ false
    void Normalize();
};

// ═══════════════════════════════════════════════════════════
//  cBVH — 静态几何 BVH（地形 / 建筑 / 合并网格）
// ═══════════════════════════════════════════════════════════
class cBVH {
  public:
    cBVH();
    ~cBVH();

    // 用一组成员盒建树；盒数组只在此次调用内读取，不持有外部指针
    void Build(const sAABB *boxes, uint32_t count);
    void Clear();

    bool IsBuilt() const { return !aNodes.empty(); }
    uint32_t GetBoxCount() const { return (uint32_t)aBoxes.size(); }
    uint32_t GetNodeCount() const { return (uint32_t)aNodes.size(); }

    // 查询结果写入 out（先清空），元素为 Build 时的原始盒序号，升序由实现保证
    void QueryFrustum(const sFRUSTUM &frustum, std::vector<uint32_t> &out) const;
    void QueryAABB(const sAABB &box, std::vector<uint32_t> &out) const;

    // 最近命中；同 t 时取索引较小的盒子
    bool Raycast(const GNg::sVEC3 &origin, const GNg::sVEC3 &dir, float maxT, sRAY_HIT &hit) const;

  private:
    struct sNODE {
        sAABB aBox;
        uint32_t aLeft; // 内部节点左孩子；叶子为 INVALID_NODE
        uint32_t aRight; // 内部节点右孩子；叶子为 INVALID_NODE
        uint32_t aFirst; // 叶子在 aOrder 中的起点
        uint32_t aCount; // 叶子元素数；内部节点为 0
        bool IsLeaf() const { return aCount > 0; }
    };
    static const uint32_t INVALID_NODE = 0xFFFFFFFFu;

    uint32_t BuildRange(uint32_t first, uint32_t count);
    void QueryNodeAABB(uint32_t node, const sAABB &box, std::vector<uint32_t> &out) const;
    void QueryNodeFrustum(uint32_t node, const sFRUSTUM &frustum, std::vector<uint32_t> &out) const;
    void RaycastNode(uint32_t node, const GNg::sVEC3 &origin, const GNg::sVEC3 &invDir,
                     bool *insideOut, float maxT, sRAY_HIT &hit) const;

    std::vector<sNODE> aNodes;
    std::vector<sAABB> aBoxes;
    std::vector<uint32_t> aOrder; // 叶子元素的索引重排
    uint32_t aRoot;
};

// ═══════════════════════════════════════════════════════════
//  cDYNAMIC_AABB_TREE — 动态对象 broadphase（刚体 / 发射器）
// ═══════════════════════════════════════════════════════════
class cDYNAMIC_AABB_TREE {
  public:
    static const uint32_t INVALID_HANDLE = 0xFFFFFFFFu;

    cDYNAMIC_AABB_TREE();
    ~cDYNAMIC_AABB_TREE();

    void Clear();
    uint32_t Insert(const sAABB &box, void *user = 0); // 返回句柄；失败返回 INVALID_HANDLE
    void Update(uint32_t handle, const sAABB &box); // 无效句柄时无操作
    void Remove(uint32_t handle); // 无效句柄时无操作
    void Query(const sAABB &box, std::vector<void *> &out) const;
    void QueryHandles(const sAABB &box, std::vector<uint32_t> &out) const;

    uint32_t GetCount() const { return aCount; }
    bool GetBox(uint32_t handle, sAABB &out) const;
    void *GetUser(uint32_t handle) const;

  private:
    struct sNODE {
        sAABB aBox;
        uint32_t aParent;
        uint32_t aLeft;
        uint32_t aRight;
        uint32_t aNext; // 空闲链表
        uint32_t aHandle; // 叶子对应的槽位号
        bool aUsed;
        bool IsLeaf() const { return aLeft == INVALID_HANDLE; }
    };
    struct sSLOT {
        sAABB aBox;
        void *aUser;
        uint32_t aLeaf; // 叶子节点索引
        bool aUsed;
    };

    uint32_t AllocNode();
    void FreeNode(uint32_t node);
    void InsertLeaf(uint32_t leaf);
    void RemoveLeaf(uint32_t leaf);
    void RefitUp(uint32_t node);
    uint32_t FindBestSibling(const sAABB &box) const;

    std::vector<sNODE> aNodes;
    std::vector<sSLOT> aSlots;
    std::vector<uint32_t> aFreeSlots;
    uint32_t aRoot;
    uint32_t aFreeNode;
    uint32_t aCount;
};

// ═══════════════════════════════════════════════════════════
//  射线与包围盒（渲染 / 物理 / 拾取共用的底层判定）
// ═══════════════════════════════════════════════════════════
bool RayIntersectAABB(const GNg::sVEC3 &origin, const GNg::sVEC3 &dir, const sAABB &box,
                      float maxT, float &outT);
bool RayIntersectAABBInside(const GNg::sVEC3 &origin, const GNg::sVEC3 &dir, const sAABB &box,
                            float maxT, float &outT, bool *originInside);


} // namespace GN3d
#endif // _GN_SPATIAL_H_
