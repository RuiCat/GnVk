#pragma once
#ifndef _GN_3DSKELETON_H_
#define _GN_3DSKELETON_H_
// ╔══════════════════════════════════════════════════════════╗
// ║  GN_mini — 骨骼（关节层级 / 绑定姿势 / 调色板，CPU）      ║
// ╚══════════════════════════════════════════════════════════╝
//
// 契约：
//   * 关节用父索引组织；本层要求 `aParent < 自身索引`（拓扑序），因此一次正序循环即可算世界矩阵。
//     父索引越界/大于等于自身时按根处理（不崩溃，便于坏数据降级）。
//   * 调色板 = world(local TRS) * inverseBind；`inverseBind` 为绑定姿势世界矩阵的逆。
//   * 与动画层的接口：局部姿势用 `sANIM_TRANSFORM`（T/R/S），由 `GN_3Danim.h` 提供。
//   * 本层不做蒙皮（skin 顶点混合）/glTF 导入/GPU 关节 UBO（P2 后续与 P8）。
#include "GN_3Dmath.h"
#include "GN_3Danim.h"

#include <cstdint>
#include <vector>

namespace GN3d {

struct sJOINT {
    int32_t aParent; // < 0 = 根
    GNg::sMAT44 aInverseBind; // 绑定姿势世界矩阵的逆
    GNg::sVEC3 aBindTranslation;
    GNg::sQUAT aBindRotation;
    GNg::sVEC3 aBindScale;
};

struct sSKIN {
    uint16_t aJoint[4]; // 关节索引
    float aWeight[4]; // 权重（调用方负责归一化）
};

struct sSKELETON {
    std::vector<sJOINT> aJoints;

    uint32_t jointCount() const { return (uint32_t)aJoints.size(); }
    // local 必须与 aJoints 等长；palette 输出 world * inverseBind（等长）。失败返回 false。
    bool computePalette(const std::vector<sANIM_TRANSFORM> &local,
                        std::vector<GNg::sMAT44> &palette) const;
    // 仅计算世界矩阵（等长时返回 true）。
    bool computeWorld(const std::vector<sANIM_TRANSFORM> &local,
                      std::vector<GNg::sMAT44> &world) const;
};

} // namespace GN3d
#endif // _GN_3DSKELETON_H_
