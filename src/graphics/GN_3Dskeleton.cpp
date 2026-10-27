// GN_3Dskeleton.cpp — 骨骼世界矩阵与调色板：world = parentWorld * localTRS；palette = world * inverseBind。
//
// 局部 TRS 组合：R * S 的 3x3 + 平移列（列主序，平移在 Elem(3, 0..2)）。
#include <GN.h>
#include "GN_3Dskeleton.h"

#include <cmath>

namespace GN3d {

namespace {

GNg::sMAT44 makeLocalMatrix(const sANIM_TRANSFORM &local) {
    GNg::sMAT44 matrix;
    GNg::QuatToMatrix44(&matrix, &local.aRotation);
    for(int column = 0; column < 3; ++column) {
        const float scale = (column == 0) ? local.aScale.x : ((column == 1) ? local.aScale.y : local.aScale.z);
        for(int row = 0; row < 3; ++row) matrix.Elem(column, row) *= scale;
    }
    matrix.Elem(3, 0) = local.aTranslation.x;
    matrix.Elem(3, 1) = local.aTranslation.y;
    matrix.Elem(3, 2) = local.aTranslation.z;
    matrix.Elem(3, 3) = 1.0f;
    return matrix;
}

} // namespace

bool sSKELETON::computeWorld(const std::vector<sANIM_TRANSFORM> &local,
                             std::vector<GNg::sMAT44> &world) const {
    if(local.size() != aJoints.size()) return false;
    world.resize(aJoints.size());
    for(size_t i = 0; i < aJoints.size(); ++i) {
        const GNg::sMAT44 localMatrix = makeLocalMatrix(local[i]);
        const int32_t parent = aJoints[i].aParent;
        if(parent >= 0 && (size_t)parent < i) {
            GNg::Mat44Mul(&world[i], &world[(size_t)parent], &localMatrix);
        } else {
            world[i] = localMatrix; // 根（或坏父索引按根降级）
        }
    }
    return true;
}

bool sSKELETON::computePalette(const std::vector<sANIM_TRANSFORM> &local,
                               std::vector<GNg::sMAT44> &palette) const {
    if(local.size() != aJoints.size()) return false;
    std::vector<GNg::sMAT44> world;
    if(!computeWorld(local, world)) return false;
    palette.resize(aJoints.size());
    for(size_t i = 0; i < aJoints.size(); ++i) {
        GNg::Mat44Mul(&palette[i], &world[i], &aJoints[i].aInverseBind);
    }
    return true;
}

} // namespace GN3d
