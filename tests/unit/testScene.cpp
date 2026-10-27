// testScene.cpp — 场景装配：父链世界矩阵 / 坏父降级 / 动画采样覆盖 / 绑定姿势调色板。
#include "testFramework.h"

#include <GN.h>
#include "GN_3Dscene.h"

#include <cmath>
#include <vector>

using namespace GN3d;

namespace {

GNg::sVEC3 makeVec3(float x, float y, float z) {
    GNg::sVEC3 value = {x, y, z};
    return value;
}

// 节点全字段显式赋值（sASSET_NODE 无默认构造，避免读到未初始化字段）。
void setNode(sASSET_NODE &node, int32_t parent, const GNg::sVEC3 &translation,
             const GNg::sQUAT &rotation, const GNg::sVEC3 &scale) {
    node.aName.clear();
    node.aParent = parent;
    node.aTranslation = translation;
    node.aRotation = rotation;
    node.aScale = scale;
    node.aMesh = -1;
    node.aSkin = -1;
}

GNg::sMAT44 translationMatrix(float x, float y, float z) {
    GNg::sMAT44 matrix;
    GNg::Mat44Translate(&matrix, x, y, z);
    return matrix;
}

void addTranslationChannel(sANIM_CLIP &clip, uint32_t node, float t0, float t1,
                           const GNg::sVEC3 &v0, const GNg::sVEC3 &v1) {
    sANIM_CHANNEL channel;
    channel.aNode = node;
    channel.aPath = eANIM_TRANSLATION;
    channel.aInterp = eANIM_LINEAR;
    channel.aTimes.push_back(t0);
    channel.aTimes.push_back(t1);
    channel.aVecValues.push_back(v0);
    channel.aVecValues.push_back(v1);
    clip.aChannels.push_back(channel);
    clip.updateDuration();
}

bool finiteVec3(const GNg::sVEC3 &v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

bool finiteQuat(const GNg::sQUAT &q) {
    return std::isfinite(q.x) && std::isfinite(q.y) && std::isfinite(q.z) && std::isfinite(q.w);
}

bool finiteMatrix(const GNg::sMAT44 &m) {
    for(int i = 0; i < 16; ++i) {
        if(!std::isfinite(m.m[i])) return false;
    }
    return true;
}

bool finiteTransform(const sANIM_TRANSFORM &t) {
    return finiteVec3(t.aTranslation) && finiteQuat(t.aRotation) && finiteVec3(t.aScale);
}

} // namespace

// 父链：root(T,R,S) 与子/孙的平移逐级合成；旋转绕 Z 90°、root 缩放 2。
TEST_CASE(sceneBuildWorldParentChain) {
    sASSET asset;
    asset.aNodes.resize(3);
    const GNg::sQUAT identity;
    GNg::sQUAT rotateZ;
    const GNg::sVEC3 axisZ = {0.0f, 0.0f, 1.0f};
    GNg::QuatFromAxisAngle(&rotateZ, &axisZ, 3.14159265f * 0.5f);
    setNode(asset.aNodes[0], -1, makeVec3(1.0f, 0.0f, 0.0f), rotateZ, makeVec3(2.0f, 2.0f, 2.0f));
    setNode(asset.aNodes[1], 0, makeVec3(0.0f, 1.0f, 0.0f), identity, makeVec3(1.0f, 1.0f, 1.0f));
    setNode(asset.aNodes[2], 1, makeVec3(0.0f, 0.0f, 3.0f), identity, makeVec3(1.0f, 1.0f, 1.0f));

    std::vector<GNg::sMAT44> world;
    CHECK(buildWorld(asset, world));
    CHECK_EQ(world.size(), 3u);
    if(world.size() != 3u) return;

    // 子世界 3x3 = R(root) * S(root)：列 0 = 2 * R * (1,0,0) = (0,2,0)
    CHECK_NEAR(world[1].Elem(0, 0), 0.0f, 1e-5f);
    CHECK_NEAR(world[1].Elem(0, 1), 2.0f, 1e-5f);
    CHECK_NEAR(world[1].Elem(0, 2), 0.0f, 1e-5f);
    // 子平移 = (1,0,0) + 2 * R * (0,1,0) = (-1,0,0)
    CHECK_NEAR(world[1].Elem(3, 0), -1.0f, 1e-5f);
    CHECK_NEAR(world[1].Elem(3, 1), 0.0f, 1e-5f);
    CHECK_NEAR(world[1].Elem(3, 2), 0.0f, 1e-5f);
    // 孙平移 = 子 3x3 * (0,0,3) + 子平移 = (-1,0,6)
    CHECK_NEAR(world[2].Elem(3, 0), -1.0f, 1e-5f);
    CHECK_NEAR(world[2].Elem(3, 1), 0.0f, 1e-5f);
    CHECK_NEAR(world[2].Elem(3, 2), 6.0f, 1e-5f);

    for(size_t i = 0; i < world.size(); ++i) {
        CHECK(finiteMatrix(world[i]));
        CHECK_NEAR(world[i].Elem(3, 3), 1.0f, 1e-6f);
    }
}

// 坏父：父索引越界、自指都按根降级，世界矩阵等于自身局部 TRS；不崩溃、不读到别人。
TEST_CASE(sceneBuildWorldDegradesBadParents) {
    sASSET asset;
    asset.aNodes.resize(3);
    const GNg::sQUAT identity;
    const GNg::sVEC3 unitScale = makeVec3(1.0f, 1.0f, 1.0f);
    setNode(asset.aNodes[0], -1, makeVec3(1.0f, 2.0f, 3.0f), identity, unitScale);
    setNode(asset.aNodes[1], 99, makeVec3(4.0f, 5.0f, 6.0f), identity, unitScale);
    setNode(asset.aNodes[2], 2, makeVec3(7.0f, 8.0f, 9.0f), identity, unitScale);

    std::vector<GNg::sMAT44> world;
    CHECK(buildWorld(asset, world));
    CHECK_EQ(world.size(), 3u);
    if(world.size() != 3u) return;

    CHECK_NEAR(world[0].Elem(3, 0), 1.0f, 1e-6f);
    CHECK_NEAR(world[0].Elem(3, 1), 2.0f, 1e-6f);
    CHECK_NEAR(world[0].Elem(3, 2), 3.0f, 1e-6f);
    CHECK_NEAR(world[1].Elem(3, 0), 4.0f, 1e-6f);
    CHECK_NEAR(world[1].Elem(3, 1), 5.0f, 1e-6f);
    CHECK_NEAR(world[1].Elem(3, 2), 6.0f, 1e-6f);
    CHECK_NEAR(world[2].Elem(3, 0), 7.0f, 1e-6f);
    CHECK_NEAR(world[2].Elem(3, 1), 8.0f, 1e-6f);
    CHECK_NEAR(world[2].Elem(3, 2), 9.0f, 1e-6f);
    // 降级为根后 3x3 是单位阵（不再继承任何父的缩放/旋转）
    CHECK_NEAR(world[1].Elem(0, 0), 1.0f, 1e-6f);
    CHECK_NEAR(world[1].Elem(0, 1), 0.0f, 1e-6f);
    for(size_t i = 0; i < world.size(); ++i) CHECK(finiteMatrix(world[i]));
}

// 动画采样：clip 有通道的节点被覆盖，无通道的节点保持默认；世界矩阵由采样后的姿势驱动。
TEST_CASE(sceneSampleLocalPoseOverridesChannels) {
    sASSET asset;
    asset.aNodes.resize(2);
    const GNg::sQUAT identity;
    setNode(asset.aNodes[0], -1, makeVec3(1.0f, 2.0f, 3.0f), identity, makeVec3(1.0f, 1.0f, 1.0f));
    setNode(asset.aNodes[1], 0, makeVec3(5.0f, 5.0f, 5.0f), identity, makeVec3(2.0f, 2.0f, 2.0f));

    sANIM_CLIP clip;
    addTranslationChannel(clip, 0u, 0.0f, 1.0f, makeVec3(0.0f, 0.0f, 0.0f), makeVec3(10.0f, 0.0f, 0.0f));

    std::vector<sANIM_TRANSFORM> local;
    CHECK(sampleLocalPose(asset, &clip, 0.5f, local));
    CHECK_EQ(local.size(), 2u);
    if(local.size() != 2u) return;
    // 节点 0：通道覆盖默认 T(1,2,3)，LINEAR 中点 (5,0,0)
    CHECK_NEAR(local[0].aTranslation.x, 5.0f, 1e-5f);
    CHECK_NEAR(local[0].aTranslation.y, 0.0f, 1e-6f);
    CHECK_NEAR(local[0].aTranslation.z, 0.0f, 1e-6f);
    // 节点 1：无通道 → 默认姿势不变
    CHECK_NEAR(local[1].aTranslation.x, 5.0f, 1e-6f);
    CHECK_NEAR(local[1].aTranslation.z, 5.0f, 1e-6f);
    CHECK_NEAR(local[1].aScale.y, 2.0f, 1e-6f);

    // clip 为空 → 全部默认姿势
    std::vector<sANIM_TRANSFORM> defaultPose;
    CHECK(sampleLocalPose(asset, 0, 0.0f, defaultPose));
    CHECK_EQ(defaultPose.size(), 2u);
    if(defaultPose.size() != 2u) return;
    CHECK_NEAR(defaultPose[0].aTranslation.z, 3.0f, 1e-6f);
    CHECK_NEAR(defaultPose[1].aScale.y, 2.0f, 1e-6f);

    std::vector<GNg::sMAT44> world;
    CHECK(buildWorld(asset, local, world));
    CHECK_EQ(world.size(), 2u);
    if(world.size() != 2u) return;
    CHECK_NEAR(world[0].Elem(3, 0), 5.0f, 1e-5f);
    CHECK_NEAR(world[0].Elem(3, 2), 0.0f, 1e-5f);
    for(size_t i = 0; i < local.size(); ++i) CHECK(finiteTransform(local[i]));
    for(size_t i = 0; i < world.size(); ++i) CHECK(finiteMatrix(world[i]));
}

// 绑定姿势：world * inverseBind ≈ identity；父关节不在 skin 内时该关节按根处理。
TEST_CASE(sceneComputeSkinPaletteBindPose) {
    sASSET asset;
    asset.aNodes.resize(3);
    const GNg::sQUAT identity;
    const GNg::sVEC3 unitScale = makeVec3(1.0f, 1.0f, 1.0f);
    setNode(asset.aNodes[0], -1, makeVec3(1.0f, 0.0f, 0.0f), identity, unitScale);
    setNode(asset.aNodes[1], 0, makeVec3(0.0f, 2.0f, 0.0f), identity, unitScale);
    setNode(asset.aNodes[2], 1, makeVec3(0.0f, 0.0f, 3.0f), identity, unitScale);

    sASSET_SKIN skin;
    skin.aJoints.push_back(0u);
    skin.aJoints.push_back(1u);
    skin.aJoints.push_back(2u);
    skin.aInverseBinds.push_back(translationMatrix(-1.0f, 0.0f, 0.0f)); // 绑定世界 T(1,0,0) 的逆
    skin.aInverseBinds.push_back(translationMatrix(-1.0f, -2.0f, 0.0f)); // T(1,2,0) 的逆
    skin.aInverseBinds.push_back(translationMatrix(-1.0f, -2.0f, -3.0f)); // T(1,2,3) 的逆
    asset.aSkins.push_back(skin);

    std::vector<sANIM_TRANSFORM> localPose;
    CHECK(sampleLocalPose(asset, 0, 0.0f, localPose));
    CHECK_EQ(localPose.size(), 3u);
    if(localPose.size() != 3u) return;

    std::vector<int32_t> parents;
    CHECK(buildSkinParents(asset, 0, parents));
    CHECK_EQ(parents.size(), 3u);
    if(parents.size() != 3u) return;
    CHECK_EQ(parents[0], -1);
    CHECK_EQ(parents[1], 0);
    CHECK_EQ(parents[2], 1);

    std::vector<GNg::sMAT44> palette;
    CHECK(computeSkinPalette(asset, 0, localPose, palette));
    CHECK_EQ(palette.size(), 3u);
    if(palette.size() != 3u) return;
    for(size_t i = 0; i < palette.size(); ++i) {
        CHECK(finiteMatrix(palette[i]));
        for(int column = 0; column < 4; ++column) {
            for(int row = 0; row < 4; ++row) {
                const float expected = (column == row) ? 1.0f : 0.0f;
                CHECK_NEAR(palette[i].Elem(column, row), expected, 1e-6f);
            }
        }
    }

    // 单关节 skin：节点 1 的父（节点 0）不在 skin 内 → 该关节按根处理。
    sASSET_SKIN singleJoint;
    singleJoint.aJoints.push_back(1u);
    singleJoint.aInverseBinds.push_back(translationMatrix(-1.0f, -2.0f, 0.0f));
    asset.aSkins.push_back(singleJoint);
    std::vector<int32_t> singleParents;
    CHECK(buildSkinParents(asset, 1, singleParents));
    CHECK_EQ(singleParents.size(), 1u);
    if(singleParents.size() != 1u) return;
    CHECK_EQ(singleParents[0], -1);

    std::vector<GNg::sMAT44> singlePalette;
    CHECK(computeSkinPalette(asset, 1, localPose, singlePalette));
    CHECK_EQ(singlePalette.size(), 1u);
    if(singlePalette.size() != 1u) return;
    // 根降级后 world = local(节点 1) = T(0,2,0)，palette = T(-1,0,0)
    CHECK_NEAR(singlePalette[0].Elem(3, 0), -1.0f, 1e-6f);
    CHECK_NEAR(singlePalette[0].Elem(3, 1), 0.0f, 1e-6f);
    CHECK_NEAR(singlePalette[0].Elem(3, 2), 0.0f, 1e-6f);
}

// 非法输入：越界索引/长度不符/非有限时间都返回 false；空资产不崩溃。
TEST_CASE(sceneRejectsInvalidInput) {
    sASSET empty;
    std::vector<GNg::sMAT44> world;
    CHECK(buildWorld(empty, world));
    CHECK(world.empty());
    std::vector<sANIM_TRANSFORM> local;
    CHECK(sampleLocalPose(empty, 0, 0.0f, local));
    CHECK(local.empty());
    CHECK(!sampleLocalPose(empty, 0, std::nanf(""), local));

    std::vector<int32_t> parents;
    std::vector<GNg::sMAT44> palette;
    CHECK(!buildSkinParents(empty, 0, parents)); // 无 skin
    CHECK(!computeSkinPalette(empty, 0, local, palette));

    sASSET asset;
    asset.aNodes.resize(1);
    const GNg::sQUAT identity;
    setNode(asset.aNodes[0], -1, makeVec3(0.0f, 0.0f, 0.0f), identity, makeVec3(1.0f, 1.0f, 1.0f));
    sASSET_SKIN broken;
    broken.aJoints.push_back(0u); // 逆绑定缺失（长度 0 != 1）
    asset.aSkins.push_back(broken);
    CHECK(!buildSkinParents(asset, 0, parents));
    CHECK(!computeSkinPalette(asset, 0, local, palette));
    CHECK(!buildSkinParents(asset, 7, parents));

    // 逆绑定合法但 localPose 未覆盖关节节点 → 失败，不静默造根。
    asset.aSkins[0].aInverseBinds.push_back(translationMatrix(0.0f, 0.0f, 0.0f));
    std::vector<sANIM_TRANSFORM> emptyPose;
    CHECK(computeSkinPalette(asset, 0, emptyPose, palette) == false);
}
