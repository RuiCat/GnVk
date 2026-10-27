// GN_3Dscene.cpp — 场景装配：节点世界矩阵、clip 局部姿势采样、蒙皮调色板。
//
// 父链求值与 TRS 组合复用 sSKELETON（computeWorld/computePalette），本层只做节点↔关节映射，
// 不另写一份矩阵数学，避免与骨骼层分叉。
#include <GN.h>
#include "GN_3Dscene.h"
#include "GN_3Dskeleton.h"

#include <cmath>

namespace GN3d {

namespace {

// 节点默认局部 TRS（glTF 缺省通道）。未设置字段的节点由调用方保证已初始化。
sANIM_TRANSFORM makeDefaultTransform(const sASSET_NODE &node) {
    sANIM_TRANSFORM transform;
    transform.aTranslation = node.aTranslation;
    transform.aRotation = node.aRotation;
    transform.aScale = node.aScale;
    return transform;
}

} // namespace

bool buildWorld(const sASSET &asset, const std::vector<sANIM_TRANSFORM> &localPose,
                std::vector<GNg::sMAT44> &outWorld) {
    if(localPose.size() != asset.aNodes.size()) return false;

    // 节点层级按 sSKELETON 的父索引契约求值（父 >= 自身或越界 → 根降级）。
    sSKELETON skeleton;
    skeleton.aJoints.resize(asset.aNodes.size());
    for(size_t i = 0; i < asset.aNodes.size(); ++i) {
        skeleton.aJoints[i].aParent = asset.aNodes[i].aParent;
        GNg::Mat44Identity(&skeleton.aJoints[i].aInverseBind);
    }

    std::vector<GNg::sMAT44> world;
    if(!skeleton.computeWorld(localPose, world)) return false;
    outWorld.swap(world);
    return true;
}

bool buildWorld(const sASSET &asset, std::vector<GNg::sMAT44> &outWorld) {
    // 默认姿势只有一处来源（sampleLocalPose），避免两套“默认 TRS”实现分叉。
    std::vector<sANIM_TRANSFORM> localPose;
    if(!sampleLocalPose(asset, 0, 0.0f, localPose)) return false;
    return buildWorld(asset, localPose, outWorld);
}

bool sampleLocalPose(const sASSET &asset, const sANIM_CLIP *clip, float time,
                     std::vector<sANIM_TRANSFORM> &outLocal) {
    if(!std::isfinite(time)) return false;

    std::vector<sANIM_TRANSFORM> local(asset.aNodes.size());
    for(size_t i = 0; i < asset.aNodes.size(); ++i) {
        local[i] = makeDefaultTransform(asset.aNodes[i]);
        if(clip == 0) continue;
        sANIM_TRANSFORM sampled;
        // 通道存在时整体覆盖：未覆盖的 T/R/S 分量取动画层默认值（与 sANIM_CLIP 契约一致）。
        if(clip->sample((uint32_t)i, time, sampled)) local[i] = sampled;
    }
    outLocal.swap(local);
    return true;
}

bool buildSkinParents(const sASSET &asset, uint32_t skinIndex, std::vector<int32_t> &outParents) {
    if(skinIndex >= asset.aSkins.size()) return false;
    const sASSET_SKIN &skin = asset.aSkins[skinIndex];
    if(skin.aInverseBinds.size() != skin.aJoints.size()) return false;

    std::vector<int32_t> parents(skin.aJoints.size(), -1);
    for(size_t joint = 0; joint < skin.aJoints.size(); ++joint) {
        const uint32_t node = skin.aJoints[joint];
        if(node >= asset.aNodes.size()) continue; // 坏节点索引 → 根
        const int32_t parentNode = asset.aNodes[node].aParent;
        if(parentNode < 0) continue; // 节点本身就是根

        int32_t parentJoint = -1;
        for(size_t other = 0; other < skin.aJoints.size(); ++other) {
            if(skin.aJoints[other] == (uint32_t)parentNode) {
                parentJoint = (int32_t)other;
                break;
            }
        }
        // 父不在 skin 内、或不在自身之前（sSKELETON 只接受拓扑序）→ 按根处理。
        if(parentJoint >= 0 && (size_t)parentJoint < joint) parents[joint] = parentJoint;
    }
    outParents.swap(parents);
    return true;
}

bool computeSkinPalette(const sASSET &asset, uint32_t skinIndex,
                        const std::vector<sANIM_TRANSFORM> &localPose,
                        std::vector<GNg::sMAT44> &outPalette) {
    if(skinIndex >= asset.aSkins.size()) return false;
    const sASSET_SKIN &skin = asset.aSkins[skinIndex];
    if(skin.aInverseBinds.size() != skin.aJoints.size()) return false;

    std::vector<int32_t> parents;
    if(!buildSkinParents(asset, skinIndex, parents)) return false;

    // 关节姿势按 skin.aJoints 的顺序从“每节点”姿势中取出；节点越界或姿势不足即失败。
    std::vector<sANIM_TRANSFORM> jointLocal(skin.aJoints.size());
    for(size_t joint = 0; joint < skin.aJoints.size(); ++joint) {
        const uint32_t node = skin.aJoints[joint];
        if(node >= asset.aNodes.size() || node >= localPose.size()) return false;
        jointLocal[joint] = localPose[node];
    }

    sSKELETON skeleton;
    skeleton.aJoints.resize(skin.aJoints.size());
    for(size_t joint = 0; joint < skin.aJoints.size(); ++joint) {
        skeleton.aJoints[joint].aParent = parents[joint];
        skeleton.aJoints[joint].aInverseBind = skin.aInverseBinds[joint];
    }

    std::vector<GNg::sMAT44> palette;
    if(!skeleton.computePalette(jointLocal, palette)) return false;
    outPalette.swap(palette);
    return true;
}

void sASSET_SCENE::clear() {
    aLocal.clear();
    aWorld.clear();
}

bool sASSET_SCENE::build(const sASSET &asset, const sANIM_CLIP *clip, float time) {
    std::vector<sANIM_TRANSFORM> local;
    std::vector<GNg::sMAT44> world;
    if(!sampleLocalPose(asset, clip, time, local) || !buildWorld(asset, local, world)) {
        clear();
        return false;
    }
    aLocal.swap(local);
    aWorld.swap(world);
    return true;
}

} // namespace GN3d
