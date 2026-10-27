#pragma once
#ifndef _GN_3DSCENE_H_
#define _GN_3DSCENE_H_
// ╔══════════════════════════════════════════════════════════╗
// ║  GN_mini — 场景装配：节点世界矩阵 / 局部姿势 / 蒙皮调色板 ║
// ╚══════════════════════════════════════════════════════════╝
//
// 契约（P2-B v1）：
//   * 节点层级用 sASSET_NODE::aParent（-1 = 根），本层要求父先于子（拓扑序）。
//     父索引越界、自指或指向自身之后节点的，一律按根降级（与 sSKELETON 契约一致），不递归、不崩溃。
//   * world = parentWorld * localTRS（T * R * S，列主序 sMAT44，平移在 Elem(3, 0..2)）。
//   * sampleLocalPose 先取节点默认 TRS；clip 中该节点有通道时用 sANIM_CLIP::sample 的结果整体覆盖，
//     无通道的节点保持默认。
//   * 蒙皮：sSKELETON 由 skin.aJoints 的父关系（buildSkinParents 映射）与 aInverseBinds 构建，
//     palette = jointWorld * inverseBind；localPose 是“每节点”姿势（索引与 aNodes 一致）。
//   * 关节顺序同样要求父先于子；父关节不在 skin 内时按根处理。
//   * 本层只做 CPU 装配：不导入 glTF、不渲染、不做蒙皮顶点混合与 GPU 关节 UBO（P2 后续 / P8）。
#include "GN_3Dasset.h"
#include "GN_3Danim.h"
#include "GN_3Dmath.h"

#include <cstdint>
#include <vector>

namespace GN3d {

// 一次装配结果：每节点局部姿势与世界矩阵（索引与 sASSET::aNodes 一致）。
struct sASSET_SCENE {
    std::vector<sANIM_TRANSFORM> aLocal; // sampleLocalPose 的结果
    std::vector<GNg::sMAT44> aWorld; // 由 aLocal 算出的世界矩阵

    void clear();
    // 采样 clip（0 = 仅默认姿势）的局部姿势并据此重算世界矩阵；失败时清空并返回 false。
    bool build(const sASSET &asset, const sANIM_CLIP *clip, float time);
};

// 用节点默认 TRS 计算全部节点世界矩阵（坏父降级为根）。失败返回 false 且不改 outWorld。
bool buildWorld(const sASSET &asset, std::vector<GNg::sMAT44> &outWorld);

// 用给定的每节点局部姿势计算全部世界矩阵（localPose 必须与 aNodes 等长）。失败返回 false。
bool buildWorld(const sASSET &asset, const std::vector<sANIM_TRANSFORM> &localPose,
                std::vector<GNg::sMAT44> &outWorld);

// 每节点局部姿势 = 默认 TRS + clip 通道覆盖；time 非有限时返回 false。
bool sampleLocalPose(const sASSET &asset, const sANIM_CLIP *clip, float time,
                     std::vector<sANIM_TRANSFORM> &outLocal);

// 把 sASSET_SKIN::aJoints 映射为骨架内的父索引（-1 = 根）。
// skinIndex 非法或逆绑定长度与 aJoints 不符时返回 false；父关节不在 skin 内或不在自身之前则映射为 -1。
bool buildSkinParents(const sASSET &asset, uint32_t skinIndex, std::vector<int32_t> &outParents);

// palette = jointWorld * inverseBind（长度 = skin.aJoints 长度）。
// skinIndex 非法、逆绑定长度不符、关节节点越界或 localPose 不足时返回 false。
bool computeSkinPalette(const sASSET &asset, uint32_t skinIndex,
                        const std::vector<sANIM_TRANSFORM> &localPose,
                        std::vector<GNg::sMAT44> &outPalette);

} // namespace GN3d
#endif // _GN_3DSCENE_H_
