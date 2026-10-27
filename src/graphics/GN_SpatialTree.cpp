// GN_SpatialTree.cpp — 动态 AABB 树（broadphase）的插入/更新/删除/查询与树维护实现。
//
// 采用 Box2D 式二叉 AABB 树：内部节点的盒恒为两个孩子的并集，叶节点保存对象槽位号；
// 句柄即槽位序号，无效句柄的所有操作均为安全无操作（契约见 GN_Spatial.h）。
#include "GN.h"
#include "GN_Spatial.h"

#include <cfloat>
#include <cmath>

namespace GN3d {

cDYNAMIC_AABB_TREE::cDYNAMIC_AABB_TREE()
    : aRoot(INVALID_HANDLE), aFreeNode(INVALID_HANDLE), aCount(0) {
}

cDYNAMIC_AABB_TREE::~cDYNAMIC_AABB_TREE() {
}

void cDYNAMIC_AABB_TREE::Clear() {
    aNodes.clear();
    aSlots.clear();
    aFreeSlots.clear();
    aRoot = INVALID_HANDLE;
    aFreeNode = INVALID_HANDLE;
    aCount = 0;
}

uint32_t cDYNAMIC_AABB_TREE::Insert(const sAABB &box, void *user) {
    uint32_t slotIndex;
    if(!aFreeSlots.empty()) {
        slotIndex = aFreeSlots.back();
        aFreeSlots.pop_back();
    } else {
        slotIndex = (uint32_t)aSlots.size();
        aSlots.push_back(sSLOT());
    }

    const uint32_t leaf = AllocNode();
    aNodes[leaf].aBox = box;
    aNodes[leaf].aHandle = slotIndex;

    aSlots[slotIndex].aBox = box;
    aSlots[slotIndex].aUser = user;
    aSlots[slotIndex].aLeaf = leaf;
    aSlots[slotIndex].aUsed = true;

    if(aRoot == INVALID_HANDLE) {
        aRoot = leaf;
        aNodes[leaf].aParent = INVALID_HANDLE;
    } else {
        InsertLeaf(leaf);
    }
    ++aCount;
    return slotIndex;
}

void cDYNAMIC_AABB_TREE::Update(uint32_t handle, const sAABB &box) {
    if(handle >= aSlots.size() || !aSlots[handle].aUsed) return;

    const uint32_t leaf = aSlots[handle].aLeaf;
    if(leaf >= aNodes.size() || !aNodes[leaf].aUsed) return;

    aSlots[handle].aBox = box;
    aNodes[leaf].aBox = box;
    RefitUp(aNodes[leaf].aParent);
}

void cDYNAMIC_AABB_TREE::Remove(uint32_t handle) {
    if(handle >= aSlots.size() || !aSlots[handle].aUsed) return;

    const uint32_t leaf = aSlots[handle].aLeaf;
    aSlots[handle].aUsed = false;
    aSlots[handle].aLeaf = INVALID_HANDLE;
    aSlots[handle].aBox.Reset();
    aSlots[handle].aUser = 0;
    aFreeSlots.push_back(handle);

    if(leaf < aNodes.size() && aNodes[leaf].aUsed) {
        RemoveLeaf(leaf);
        FreeNode(leaf);
    }
    if(aCount > 0) --aCount;
}

void cDYNAMIC_AABB_TREE::Query(const sAABB &box, std::vector<void *> &out) const {
    out.clear();
    if(aRoot == INVALID_HANDLE || !box.IsValid()) return;

    std::vector<uint32_t> stack;
    stack.push_back(aRoot);
    while(!stack.empty()) {
        const uint32_t nodeIndex = stack.back();
        stack.pop_back();
        if(nodeIndex >= aNodes.size()) continue;

        const sNODE &node = aNodes[nodeIndex];
        if(!node.aBox.Overlaps(box)) continue;

        if(node.IsLeaf()) {
            if(node.aHandle < aSlots.size() && aSlots[node.aHandle].aUsed) {
                out.push_back(aSlots[node.aHandle].aUser);
            }
            continue;
        }
        stack.push_back(node.aLeft);
        stack.push_back(node.aRight);
    }
}

void cDYNAMIC_AABB_TREE::QueryHandles(const sAABB &box, std::vector<uint32_t> &out) const {
    out.clear();
    if(aRoot == INVALID_HANDLE || !box.IsValid()) return;

    std::vector<uint32_t> stack;
    stack.push_back(aRoot);
    while(!stack.empty()) {
        const uint32_t nodeIndex = stack.back();
        stack.pop_back();
        if(nodeIndex >= aNodes.size()) continue;

        const sNODE &node = aNodes[nodeIndex];
        if(!node.aBox.Overlaps(box)) continue;

        if(node.IsLeaf()) {
            if(node.aHandle < aSlots.size() && aSlots[node.aHandle].aUsed) {
                out.push_back(node.aHandle);
            }
            continue;
        }
        stack.push_back(node.aLeft);
        stack.push_back(node.aRight);
    }
}

bool cDYNAMIC_AABB_TREE::GetBox(uint32_t handle, sAABB &out) const {
    if(handle >= aSlots.size() || !aSlots[handle].aUsed) return false;
    out = aSlots[handle].aBox;
    return true;
}

void *cDYNAMIC_AABB_TREE::GetUser(uint32_t handle) const {
    if(handle >= aSlots.size() || !aSlots[handle].aUsed) return 0;
    return aSlots[handle].aUser;
}

// ═══════════════════════════════════════════════════════════
//  节点池与树维护
// ═══════════════════════════════════════════════════════════

uint32_t cDYNAMIC_AABB_TREE::AllocNode() {
    uint32_t nodeIndex;
    if(aFreeNode != INVALID_HANDLE) {
        nodeIndex = aFreeNode;
        aFreeNode = aNodes[nodeIndex].aNext;
    } else {
        nodeIndex = (uint32_t)aNodes.size();
        aNodes.push_back(sNODE());
    }

    sNODE &node = aNodes[nodeIndex];
    node.aBox.Reset();
    node.aParent = INVALID_HANDLE;
    node.aLeft = INVALID_HANDLE;
    node.aRight = INVALID_HANDLE;
    node.aNext = INVALID_HANDLE;
    node.aHandle = INVALID_HANDLE;
    node.aUsed = true;
    return nodeIndex;
}

void cDYNAMIC_AABB_TREE::FreeNode(uint32_t node) {
    if(node >= aNodes.size()) return;
    aNodes[node].aUsed = false;
    aNodes[node].aNext = aFreeNode;
    aFreeNode = node;
}

void cDYNAMIC_AABB_TREE::InsertLeaf(uint32_t leaf) {
    if(aRoot == INVALID_HANDLE) {
        aRoot = leaf;
        aNodes[leaf].aParent = INVALID_HANDLE;
        return;
    }

    const uint32_t sibling = FindBestSibling(aNodes[leaf].aBox);
    if(sibling == INVALID_HANDLE) return;

    const uint32_t oldParent = aNodes[sibling].aParent;
    const uint32_t newNode = AllocNode();

    // AllocNode 可能使 aNodes 扩容，之后一律按索引访问。
    sAABB combined = aNodes[sibling].aBox;
    combined.Expand(aNodes[leaf].aBox);
    aNodes[newNode].aBox = combined;
    aNodes[newNode].aParent = oldParent;
    aNodes[newNode].aLeft = sibling;
    aNodes[newNode].aRight = leaf;
    aNodes[newNode].aHandle = INVALID_HANDLE;

    aNodes[sibling].aParent = newNode;
    aNodes[leaf].aParent = newNode;

    if(oldParent != INVALID_HANDLE) {
        if(aNodes[oldParent].aLeft == sibling) {
            aNodes[oldParent].aLeft = newNode;
        } else {
            aNodes[oldParent].aRight = newNode;
        }
        RefitUp(oldParent);
    } else {
        aRoot = newNode;
        RefitUp(newNode);
    }
}

void cDYNAMIC_AABB_TREE::RemoveLeaf(uint32_t leaf) {
    if(leaf >= aNodes.size()) return;
    if(leaf == aRoot) {
        aRoot = INVALID_HANDLE;
        aNodes[leaf].aParent = INVALID_HANDLE;
        return;
    }

    const uint32_t parent = aNodes[leaf].aParent;
    if(parent == INVALID_HANDLE || parent >= aNodes.size()) {
        aNodes[leaf].aParent = INVALID_HANDLE;
        return;
    }

    const uint32_t grand = aNodes[parent].aParent;
    const uint32_t sibling =
        (aNodes[parent].aLeft == leaf) ? aNodes[parent].aRight : aNodes[parent].aLeft;
    aNodes[leaf].aParent = INVALID_HANDLE;

    if(grand != INVALID_HANDLE && grand < aNodes.size()) {
        if(aNodes[grand].aLeft == parent) {
            aNodes[grand].aLeft = sibling;
        } else {
            aNodes[grand].aRight = sibling;
        }
        aNodes[sibling].aParent = grand;
        FreeNode(parent);
        RefitUp(grand);
    } else {
        aRoot = sibling;
        aNodes[sibling].aParent = INVALID_HANDLE;
        FreeNode(parent);
    }
}

void cDYNAMIC_AABB_TREE::RefitUp(uint32_t node) {
    while(node != INVALID_HANDLE && node < aNodes.size()) {
        if(aNodes[node].IsLeaf()) break;
        const uint32_t left = aNodes[node].aLeft;
        const uint32_t right = aNodes[node].aRight;
        if(left >= aNodes.size() || right >= aNodes.size()) break;

        sAABB box = aNodes[left].aBox;
        box.Expand(aNodes[right].aBox);
        aNodes[node].aBox = box;
        node = aNodes[node].aParent;
    }
}

uint32_t cDYNAMIC_AABB_TREE::FindBestSibling(const sAABB &box) const {
    if(aRoot == INVALID_HANDLE) return INVALID_HANDLE;

    uint32_t best = INVALID_HANDLE;
    float bestCost = FLT_MAX;
    std::vector<uint32_t> stack;
    stack.push_back(aRoot);
    while(!stack.empty()) {
        const uint32_t nodeIndex = stack.back();
        stack.pop_back();
        if(nodeIndex >= aNodes.size()) continue;

        const sNODE &node = aNodes[nodeIndex];
        sAABB combined = node.aBox;
        combined.Expand(box);
        const float cost = combined.SurfaceArea() - node.aBox.SurfaceArea();

        if(node.IsLeaf()) {
            const bool better = cost < bestCost - 1e-6f ||
                                (std::fabs(cost - bestCost) <= 1e-6f &&
                                 (best == INVALID_HANDLE || nodeIndex < best));
            if(better) {
                bestCost = cost;
                best = nodeIndex;
            }
            continue;
        }
        stack.push_back(node.aLeft);
        stack.push_back(node.aRight);
    }
    return best;
}

} // namespace GN3d
