// testEntity.cpp — 实体与四套控制器（P1）纯 CPU 单测。
//
// 覆盖 docs/ENTITY_CONTROLLERS_PLAN.md §5.1 P1 的验收口径：
//   * 控制器只做接线，不改变既有算法行为（绑定姿势下 palette == 单位阵）；
//   * cENTITY::Update 的固定顺序（物理 → 动画 → 骨骼）；
//   * 物理映射表：登记/解绑/覆盖/世界换掉后失效；
//   * 动画 clip 表按名切换、非法名/非法 dt 不崩；
//   * fixtures（程序化骨架/蒙皮）能跑通 palette → LBS 这条链。
#include "testFramework.h"

#include <GN.h>

#include "GN_Entity.h"
#include "GN_ShadowCascade.h"
#include "vkRenderFrame.h" // P1：一帧的编排（GN_Vk.h 末尾会带进它）
#include "fixtures.h"

#include <cmath>
#include <cstring>
#include <vector>

using namespace GN3d;

namespace {

const float kEps = 1e-5f;

// 绑定姿势下 palette 必须是单位阵（world × inverseBind 完全抵消）。
void checkIdentityMatrix(const GNg::sMAT44 &m, float eps) {
    for (int i = 0; i < 16; ++i) {
        const float expected = (i % 5 == 0) ? 1.0f : 0.0f;
        CHECK_NEAR(m.m[i], expected, eps);
    }
}

sRIGID_BODY makeSphereBody(float x, float y, float z) {
    sRIGID_BODY body;
    std::memset(&body, 0, sizeof(body)); // sRIGID_BODY 无默认构造：显式清零，别依赖栈内容
    body.aType = eBODY_DYNAMIC;
    body.aShape = eSHAPE_SPHERE;
    body.aMass = 1.0f;
    body.aRadius = 0.5f;
    body.aPos = GNg::sVEC3{x, y, z};
    body.aRot = GNg::sQUAT{0.0f, 0.0f, 0.0f, 1.0f};
    body.aFriction = 0.5f;
    return body;
}

// 一段只有单通道的 clip：某节点绕 Z 轴 0 → 90°。
sANIM_CLIP makeSpinClip(uint32_t node) {
    sANIM_CLIP clip;
    clip.aDuration = 1.0f;
    sANIM_CHANNEL channel;
    channel.aNode = node;
    channel.aPath = eANIM_ROTATION;
    channel.aInterp = eANIM_LINEAR;
    channel.aTimes.push_back(0.0f);
    channel.aTimes.push_back(1.0f);
    channel.aQuatValues.push_back(GNg::sQUAT{0.0f, 0.0f, 0.0f, 1.0f});
    const float half = 1.57079632679f * 0.5f;
    channel.aQuatValues.push_back(GNg::sQUAT{0.0f, 0.0f, std::sin(half), std::cos(half)});
    clip.aChannels.push_back(channel);
    return clip;
}

} // namespace

// ── 网格控制器 ──────────────────────────────────────────
TEST_CASE(entityMeshControllerWithoutModel) {
    cMESH_CONTROLLER controller;
    CHECK(controller.Model() == nullptr);
    CHECK(!controller.Visible());
    CHECK_EQ(controller.LodLevel(0), 0u);
    CHECK_EQ(controller.LodCount(), 0u);
    sLOD_VIEW view;
    view.aScreenHeight = 800.0f;
    view.aFovYRad = 0.8f;
    view.aCamPos = GNg::sVEC3{0.0f, 0.0f, 5.0f};
    CHECK(!controller.Prepare(view, nullptr)); // 没模型 = 不可见，且不崩
    CHECK(!controller.Visible());
}

TEST_CASE(entityMeshControllerPrepareWithCube) {
    cMODEL *cube = CreateCubeModel(1.0f);
    CHECK(cube != nullptr);
    cMESH_CONTROLLER controller;
    controller.SetModel(cube);
    CHECK(controller.Model() == cube);

    sLOD_VIEW view;
    view.aScreenHeight = 800.0f;
    view.aFovYRad = 0.8f;
    view.aCamPos = GNg::sVEC3{0.0f, 0.0f, 5.0f};
    CHECK(controller.Prepare(view, nullptr));
    CHECK(controller.Visible());
    CHECK_EQ(controller.LodCount(), cube->aMeshCount);
    // 单级网格：选级恒为 0（与旧路径一致）
    CHECK_EQ(controller.LodLevel(0), 0u);
    CHECK_EQ(controller.LodLevel(99), 0u); // 越界不崩

    // 关闭剔除后，即使视锥把它整片切掉也照旧可见（对照：开启剔除时不可见）
    sFRUSTUM empty; // 默认 6 个零平面 → ContainsPoint 恒真，这里只验开关语义
    controller.SetCullEnabled(false);
    CHECK(controller.Prepare(view, &empty));
    CHECK(controller.Visible());

    controller.SetModel(nullptr);
    CHECK(!controller.Visible());
    CHECK_EQ(controller.LodCount(), 0u);
    delete cube;
}

// ── 动画控制器 ──────────────────────────────────────────
TEST_CASE(entityAnimControllerClipsByName) {
    cANIM_CONTROLLER controller;
    const sANIM_CLIP idle = makeSpinClip(0);
    const sANIM_CLIP walk = makeSpinClip(1);
    CHECK(controller.RegisterClip("idle", &idle));
    CHECK(controller.RegisterClip("walk", &walk));
    CHECK_EQ(controller.ClipCount(), 2u);
    CHECK(controller.FindClip("idle") == &idle);
    CHECK(controller.FindClip("nope") == nullptr);
    CHECK(!controller.RegisterClip("", &idle)); // 空名
    CHECK(!controller.RegisterClip("x", nullptr)); // 空 clip

    CHECK(controller.Play("walk", true, 1.0f));
    CHECK(controller.Playing());
    CHECK(controller.FindClip(controller.Current()) == &walk);
    CHECK(!controller.Play("nope")); // 未知名字不动当前状态
    CHECK(controller.FindClip(controller.Current()) == &walk);

    controller.Stop();
    CHECK(!controller.Playing());
    CHECK(controller.Current()[0] == '\0');
}

TEST_CASE(entityAnimControllerSamplesAndIgnoresBadDt) {
    cANIM_CONTROLLER controller;
    const sANIM_CLIP clip = makeSpinClip(1);
    CHECK(controller.RegisterClip("spin", &clip));
    CHECK(controller.Play("spin", true, 1.0f));

    controller.Update(0.0f);
    controller.Update(-1.0f);
    controller.Update(std::nanf(""));
    CHECK_NEAR(controller.Time(), 0.0f, kEps);

    controller.Update(0.5f);
    CHECK_NEAR(controller.Time(), 0.5f, 1e-4);

    sANIM_TRANSFORM transform;
    CHECK(controller.Sample(1, transform));
    CHECK_NEAR(transform.aRotation.z, std::sin(0.785398163f * 0.5f), 1e-3); // 半程 ≈ 45°
    CHECK(!controller.Sample(7, transform)); // 该节点没通道

    CHECK(controller.CrossFade("spin", 0.2f, true)); // 同名切换也不崩
    CHECK(controller.Playing());
}

// ── 骨骼控制器 ──────────────────────────────────────────
TEST_CASE(entitySkeletonControllerBindPoseIsIdentity) {
    const sSKELETON skeleton = fixture::buildBoneChain(3);
    CHECK_EQ(skeleton.jointCount(), 3u);

    cSKELETON_CONTROLLER controller;
    controller.SetSkeleton(&skeleton);
    CHECK_EQ(controller.JointCount(), 3u);
    CHECK(!controller.PaletteValid());
    CHECK(!controller.Palette().size());

    const std::vector<sANIM_TRANSFORM> local = fixture::bindPoseLocal(skeleton);
    CHECK(controller.ComputePalette(local));
    CHECK(controller.PaletteValid());
    CHECK_EQ(controller.Palette().size(), 3u);
    for (size_t i = 0; i < controller.Palette().size(); ++i) {
        checkIdentityMatrix(controller.Palette()[i], 1e-4f);
    }

    // 绑定姿势下关节世界位置 = (0, (i+1)L, 0)
    for (uint32_t i = 0; i < 3; ++i) {
        const GNg::sVEC3 position = controller.JointWorldPosition(i);
        CHECK_NEAR(position.x, 0.0f, 1e-4);
        CHECK_NEAR(position.y, (float)(i + 1) * fixture::BONE_LENGTH, 1e-4);
        CHECK_NEAR(position.z, 0.0f, 1e-4);
    }

    // 姿势长度不符：返回 false，且不清掉上一次成功的缓存
    std::vector<sANIM_TRANSFORM> wrong(2);
    CHECK(!controller.ComputePalette(wrong));
    CHECK(controller.PaletteValid());
    CHECK_EQ(controller.Palette().size(), 3u);
}

TEST_CASE(entitySkeletonControllerPackPalette) {
    const sSKELETON skeleton = fixture::buildBoneChain(3);
    cSKELETON_CONTROLLER controller;
    controller.SetSkeleton(&skeleton);
    CHECK(controller.ComputePalette(fixture::bindPoseLocal(skeleton)));

    sSKIN_UBO ubo;
    CHECK_EQ(controller.PackPalette(ubo), 3u);
    // 前 3 个槽是单位阵，其余清零
    for (int i = 0; i < 16; ++i) {
        const float expected = (i % 5 == 0) ? 1.0f : 0.0f;
        CHECK_NEAR(ubo.aJointMatrices[0][i], expected, 1e-4);
        CHECK_NEAR(ubo.aJointMatrices[2][i], expected, 1e-4);
    }
    for (int i = 0; i < 16; ++i) {
        CHECK_NEAR(ubo.aJointMatrices[3][i], 0.0f, 1e-6);
        CHECK_NEAR(ubo.aJointMatrices[SKIN_MAX_JOINTS - 1][i], 0.0f, 1e-6);
    }

    // 129 关节：打包截断到 128
    const sSKELETON big = fixture::buildBoneChain(129);
    cSKELETON_CONTROLLER bigController;
    bigController.SetSkeleton(&big);
    CHECK(bigController.ComputePalette(fixture::bindPoseLocal(big)));
    CHECK_EQ(bigController.PackPalette(ubo), SKIN_MAX_JOINTS);
}

// 转 mid 关节：下半（只绑 root）不动，上半（绑 mid）跟着转 —— 这条把 
// fixtures → palette → LBS 整条链串起来验。
TEST_CASE(entitySkeletonDrivesSkinning) {
    const sSKELETON skeleton = fixture::buildBoneChain(3);
    cSKELETON_CONTROLLER controller;
    controller.SetSkeleton(&skeleton);

    const std::vector<GNg::sMAT44> identityPalette(skeleton.jointCount(),
                                                   GNg::sMAT44(true));
    const std::vector<sSKINNED_VERTEX> vertices = fixture::buildSkinnedCube(1.0f);
    CHECK_EQ(vertices.size(), 8u);

    std::vector<GNg::sVEC3> basePositions(vertices.size());
    std::vector<GNg::sVEC3> baseNormals(vertices.size());
    CHECK(skinVertices(vertices.data(), (uint32_t)vertices.size(), identityPalette.data(),
                       (uint32_t)identityPalette.size(), basePositions.data(),
                       baseNormals.data()));
    for (size_t i = 0; i < basePositions.size(); ++i) {
        CHECK_NEAR(basePositions[i].x, vertices[i].aPosition.x, 1e-5);
        CHECK_NEAR(basePositions[i].y, vertices[i].aPosition.y, 1e-5);
    }

    CHECK(controller.ComputePalette(fixture::poseWithJointRotation(skeleton, 1, 1.5707963f)));
    std::vector<GNg::sVEC3> rotatedPositions(vertices.size());
    std::vector<GNg::sVEC3> rotatedNormals(vertices.size());
    CHECK(skinVertices(vertices.data(), (uint32_t)vertices.size(), controller.Palette().data(),
                       (uint32_t)controller.Palette().size(), rotatedPositions.data(),
                       rotatedNormals.data()));

    for (size_t i = 0; i < vertices.size(); ++i) {
        const bool upper = vertices[i].aPosition.y > 0.0f;
        const float dx = rotatedPositions[i].x - basePositions[i].x;
        const float dy = rotatedPositions[i].y - basePositions[i].y;
        const float dz = rotatedPositions[i].z - basePositions[i].z;
        const float moved = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (upper) {
            CHECK(moved > 0.1f); // 上半跟着 mid 关节转
        } else {
            CHECK_NEAR(moved, 0.0f, 1e-5); // 下半只绑 root（root 没动）
        }
    }
}

// ── 物理控制器 ──────────────────────────────────────────
TEST_CASE(entityPhysicsBindingsAndStep) {
    cPHYSICS_WORLD world;
    cPHYSICS_CONTROLLER controller;
    controller.SetWorld(&world);
    CHECK(controller.World() == &world);
    CHECK(!controller.AttachBody(1u, cPHYSICS_WORLD::INVALID_HANDLE)); // 非法句柄
    CHECK(!controller.AttachBody(0u, 0u)); // entityId 0 保留

    const uint32_t handle = world.AddBody(makeSphereBody(0.0f, 5.0f, 0.0f));
    CHECK(handle != cPHYSICS_WORLD::INVALID_HANDLE);
    CHECK(controller.AttachBody(7u, handle));
    CHECK_EQ(controller.BodyOf(7u), handle);
    CHECK_EQ(controller.EntityOf(handle), 7);
    CHECK_EQ(controller.BindingCount(), 1u);

    // 重复登记同一 entity：覆盖而不是新增
    CHECK(controller.AttachBody(7u, handle));
    CHECK_EQ(controller.BindingCount(), 1u);
    CHECK(!controller.AttachBody(8u, cPHYSICS_WORLD::INVALID_HANDLE));
    CHECK_EQ(controller.BindingCount(), 1u);

    // Step 转发：球应该落下去
    const sRIGID_BODY *before = world.GetBody(handle);
    CHECK(before != nullptr);
    const float startY = before->aPos.y;
    for (int i = 0; i < 60; ++i) {
        controller.Step(1.0f / 60.0f);
    }
    const sRIGID_BODY *after = world.GetBody(handle);
    CHECK(after != nullptr);
    CHECK(after->aPos.y < startY - 0.5f);

    // 刚体 → 模型 单向同步
    cMODEL model;
    CHECK(controller.ApplyTransform(7u, model));
    CHECK_NEAR(model.aPosition.y, after->aPos.y, 1e-4);

    CHECK(controller.DetachBody(7u));
    CHECK_EQ(controller.BodyOf(7u), cPHYSICS_WORLD::INVALID_HANDLE);
    CHECK_EQ(controller.EntityOf(handle), -1);
    CHECK(!controller.ApplyTransform(7u, model)); // 解绑后不再写模型
    CHECK(!controller.DetachBody(7u));
}

TEST_CASE(entityPhysicsSetWorldClearsBindings) {
    cPHYSICS_WORLD worldA;
    cPHYSICS_WORLD worldB;
    cPHYSICS_CONTROLLER controller;
    controller.SetWorld(&worldA);
    const uint32_t handle = worldA.AddBody(makeSphereBody(0.0f, 3.0f, 0.0f));
    CHECK(controller.AttachBody(1u, handle));
    CHECK_EQ(controller.BindingCount(), 1u);

    controller.SetWorld(&worldB); // 换世界：旧句柄一律失效
    CHECK_EQ(controller.BindingCount(), 0u);
    CHECK_EQ(controller.BodyOf(1u), cPHYSICS_WORLD::INVALID_HANDLE);
}

// ── cENTITY：固定更新顺序 ───────────────────────────────
TEST_CASE(entityUpdateRunsPhysicsAnimSkeleton) {
    cPHYSICS_WORLD world;
    cENTITY entity;
    entity.aId = 42u;
    entity.aPhysics.SetWorld(&world);

    const uint32_t handle = world.AddBody(makeSphereBody(0.0f, 4.0f, 0.0f));
    CHECK(entity.aPhysics.AttachBody(entity.aId, handle));

    const sSKELETON skeleton = fixture::buildBoneChain(3);
    entity.aSkeleton.SetSkeleton(&skeleton);
    const sANIM_CLIP clip = makeSpinClip(1);
    CHECK(entity.aAnim.RegisterClip("spin", &clip));
    CHECK(entity.aAnim.Play("spin", true, 1.0f));

    for (int i = 0; i < 30; ++i) {
        entity.Update(1.0f / 60.0f);
    }

    // 物理先跑：球下落了
    const sRIGID_BODY *body = world.GetBody(handle);
    CHECK(body != nullptr);
    CHECK(body->aPos.y < 4.0f - 0.2f);
    // 动画推进了
    CHECK(entity.aAnim.Time() > 0.0f);
    // 骨骼算出了调色板，且 mid 关节已经不是单位阵（转起来了）
    CHECK(entity.aSkeleton.PaletteValid());
    CHECK_EQ(entity.aSkeleton.Palette().size(), 3u);
    const GNg::sMAT44 &mid = entity.aSkeleton.Palette()[1];
    const bool midRotated = std::fabs(mid.m[1]) > 1e-3f || std::fabs(mid.m[4]) > 1e-3f;
    CHECK(midRotated);
    // root 的调色板仍是单位阵（没人转它）
    checkIdentityMatrix(entity.aSkeleton.Palette()[0], 1e-4f);
}

// clip 只驱动部分关节时：**没有通道的关节留在绑定姿势**，而它的子孙仍然跟着父关节动
// （父动子动是骨架语义，不是"子孙也保持绑定姿势"）。这两条必须一起验，
// 否则很容易把"没通道就当没动"写进实现里。
TEST_CASE(entityAnimLeavesOtherJointsAtBindPose) {
    cENTITY entity;
    const sSKELETON skeleton = fixture::buildBoneChain(3);
    entity.aSkeleton.SetSkeleton(&skeleton);
    const sANIM_CLIP clip = makeSpinClip(1); // 只有 mid 有通道
    CHECK(entity.aAnim.RegisterClip("spin", &clip));
    CHECK(entity.aAnim.Play("spin", true, 1.0f));

    for (int i = 0; i < 10; ++i) {
        entity.Update(1.0f / 60.0f);
    }
    CHECK(entity.aSkeleton.PaletteValid());
    CHECK_EQ(entity.aSkeleton.Palette().size(), 3u);
    // root 没有通道也没有父：仍是单位阵（= 绑定姿势）
    checkIdentityMatrix(entity.aSkeleton.Palette()[0], 1e-4f);
    // mid 转起来了
    const GNg::sMAT44 &mid = entity.aSkeleton.Palette()[1];
    CHECK(std::fabs(mid.m[1]) > 1e-3f || std::fabs(mid.m[4]) > 1e-3f);
    // tip 没有通道，但它是 mid 的子：世界矩阵继承 mid 的旋转，所以调色板不是单位阵，
    // 且旋转部分必须与 mid 相同（同一个刚体旋转，只是枢轴不同）。
    const GNg::sMAT44 &tip = entity.aSkeleton.Palette()[2];
    CHECK_NEAR(tip.m[0], mid.m[0], 1e-4);
    CHECK_NEAR(tip.m[1], mid.m[1], 1e-4);
    CHECK_NEAR(tip.m[4], mid.m[4], 1e-4);
    CHECK_NEAR(tip.m[5], mid.m[5], 1e-4);
    // 绕 Z 转时，mid 与 tip 的调色板其实是同一个刚体变换（Z 轴的枢轴落在同一条线上），
    // 但 tip 的**世界位置**必须被父关节带走：绑定姿势在 (0,3,0)，转起来后 x 不再为 0。
    const GNg::sVEC3 tipWorld = entity.aSkeleton.JointWorldPosition(2);
    CHECK(std::fabs(tipWorld.x) > 0.1f);

    // BindPoseLocal 自身来自 sJOINT 的绑定字段
    const std::vector<sANIM_TRANSFORM> bindPose = entity.aSkeleton.BindPoseLocal();
    CHECK_EQ(bindPose.size(), 3u);
    CHECK_NEAR(bindPose[0].aTranslation.y, fixture::BONE_LENGTH, 1e-6);
    CHECK_NEAR(bindPose[0].aScale.y, 1.0f, 1e-6);
}

TEST_CASE(entityUpdateWithoutSkeletonOrModel) {
    cENTITY entity;
    // 什么都没接：Update 不能崩，也不能造出假姿势
    for (int i = 0; i < 5; ++i) {
        entity.Update(1.0f / 60.0f);
    }
    CHECK(!entity.aSkeleton.PaletteValid());
    CHECK(!entity.bHasLocalPose);
    CHECK(!entity.Visible());

    // 只有 clip、没有骨架：不推进到"零姿势"里
    const sANIM_CLIP clip = makeSpinClip(0);
    CHECK(entity.aAnim.RegisterClip("spin", &clip));
    CHECK(entity.aAnim.Play("spin", true, 1.0f));
    entity.Update(0.1f);
    CHECK(!entity.bHasLocalPose);

    // 手动塞姿势也要能算
    const sSKELETON skeleton = fixture::buildBoneChain(2);
    entity.aSkeleton.SetSkeleton(&skeleton);
    entity.SetLocalPose(fixture::bindPoseLocal(skeleton));
    CHECK(entity.bHasLocalPose);
    entity.Update(0.0f);
    CHECK(entity.aSkeleton.PaletteValid());
}

// ── 阴影状态（P1：示例里的那 7 个全局量收进结构体）──────────
TEST_CASE(entityShadowStateClearAndFit) {
    GN3d::sSHADOW_STATE state;
    CHECK(!state.aValid);
    CHECK_EQ(state.aCount, 0u);
    for (uint32_t i = 0; i < GN3d::SHADOW_CASCADE_MAX; ++i) {
        CHECK_NEAR(state.aSplits[i], 0.0f, 1e-6); // Clear 后不留脏数据
        CHECK_NEAR(state.aTexelWorld[i], 0.0f, 1e-6);
        CHECK_NEAR(state.aLightViewProj[i].m[0], 1.0f, 1e-6); // 单位阵（不会被当作有效矩阵用）
    }
    // 无效 caster 盒：拟合失败，状态必须被清干净（不保留上一帧的光矩阵）
    GN3d::sSHADOW_FIT_DESC desc;
    desc.aCameraPos = GNg::sVEC3{0.0f, 2.0f, 8.0f};
    desc.aLightDirection = GNg::sVEC3{0.0f, -1.0f, 0.0f};
    CHECK(!GN3d::fitShadowState(desc, nullptr, &state));
    CHECK(!state.aValid);
    CHECK_EQ(state.aCount, 0u);
}

TEST_CASE(entityShadowCasterBoxExpansion) {
    GN3d::sAABB box;
    box.aMin = GNg::sVEC3{-1.0f, -0.5f, -1.0f};
    box.aMax = GNg::sVEC3{1.0f, 1.5f, 1.0f};

    const GN3d::sAABB expanded = GN3d::expandShadowCasterBox(box, 2.0f, 0.0f);
    CHECK_NEAR(expanded.aMin.x, -3.0f, 1e-5); // x/z 各外扩 margin
    CHECK_NEAR(expanded.aMax.z, 3.0f, 1e-5);
    CHECK_NEAR(expanded.aMin.y, 0.0f, 1e-5); // 下沿抬到地面
    CHECK_NEAR(expanded.aMax.y, 1.5f, 1e-5); // 上沿不动（竖直方向不外扩）

    // 悬空模型：下沿不动（只抬不压）
    GN3d::sAABB floating;
    floating.aMin = GNg::sVEC3{-1.0f, 3.0f, -1.0f};
    floating.aMax = GNg::sVEC3{1.0f, 5.0f, 1.0f};
    const GN3d::sAABB keptFloating = GN3d::expandShadowCasterBox(floating, 0.0f, 0.0f);
    CHECK_NEAR(keptFloating.aMin.y, 3.0f, 1e-5);

    // 无效盒原样返回
    GN3d::sAABB invalid;
    CHECK(!GN3d::expandShadowCasterBox(invalid, 1.0f, 0.0f).IsValid());
}

// ── fixtures 自身 ───────────────────────────────────────
TEST_CASE(entityFixturesAreConsistent) {
    const sSKELETON three = fixture::buildBoneChain(3);
    CHECK_EQ(three.jointCount(), 3u);
    CHECK_EQ(three.aJoints[0].aParent, -1);
    CHECK_EQ(three.aJoints[2].aParent, 1);
    CHECK_NEAR(three.aJoints[1].aBindTranslation.y, fixture::BONE_LENGTH, 1e-6);

    const sSKELETON single = fixture::buildBoneChain(0); // 至少 1 节
    CHECK_EQ(single.jointCount(), 1u);

    const std::vector<sSKINNED_VERTEX> cube = fixture::buildSkinnedCube(2.0f);
    CHECK_EQ(cube.size(), 8u);
    CHECK_EQ(fixture::cubeIndices().size(), 36u);
    for (size_t i = 0; i < cube.size(); ++i) {
        // 权重和为 1（skinVertices 内部还会再归一化，但源头就该是干净的）
        CHECK_NEAR(cube[i].aWeights[0] + cube[i].aWeights[1] + cube[i].aWeights[2] +
                       cube[i].aWeights[3],
                   1.0f, 1e-6);
        CHECK(std::fabs(cube[i].aPosition.y) <= 1.0f + 1e-6);
    }

    CHECK(fixture::writeFixtures(".local/fixtures")); // make fixtures 的落盘路径也就是它
}

// ── P1 的重构等价性：fitShadowState 与旧的两步路径必须逐字段一致 ──────
// 旧路径（两个示例各写一份）：fitShadowCascades → 逐字段搬进全局量 → computeShadowFar 回填。
// 新路径：fitShadowState 一次做完。这条用例保证"收进去"没有改变任何数值。
TEST_CASE(entityShadowStateMatchesLegacyPath) {
    GN3d::cLIGHT light; // 只做容器：不接 GPU，SetShadow* 只写 CPU 侧数据
    GN3d::sSHADOW_FIT_DESC desc;
    desc.aCameraPos = GNg::sVEC3{0.0f, 1.5f, 6.0f};
    desc.aCameraView.Init(true);
    desc.aCameraView.m[14] = -6.0f;
    desc.aFovYRad = 0.785398163f;
    desc.aAspect = 1.6f;
    desc.aLightDirection = GNg::sVEC3{-0.35f, -0.85f, 0.4f};
    desc.aCasterBox.aMin = GNg::sVEC3{-1.0f, 0.0f, -1.0f};
    desc.aCasterBox.aMax = GNg::sVEC3{1.0f, 2.0f, 1.0f};
    desc.aCatchMargin = 0.0f;
    desc.aClampToGround = false;
    desc.aCascadeCount = GN3d::SHADOW_CASCADE_MAX;
    desc.aShadowMapSize = 2048;
    desc.aSplitLambda = 0.5f;
    desc.aTexelSnapping = true;
    desc.aEngineCascadeLayers = GN3d::SHADOW_CASCADE_MAX;

    // 旧路径
    GN3d::sSHADOW_CASCADE_RESULT legacy;
    CHECK(GN3d::fitShadowCascades(desc, &light, &legacy));
    const float legacyFar = GN3d::computeShadowFar(desc.aCasterBox, desc.aCameraPos);

    // 新路径
    GN3d::sSHADOW_STATE state;
    CHECK(GN3d::fitShadowState(desc, &light, &state));
    CHECK(state.aValid);
    CHECK_EQ(state.aCount, legacy.aCount);
    CHECK_EQ(state.aMapSize, legacy.aShadowMapSize);
    CHECK_NEAR(state.aFar, legacyFar, 1e-6);
    for (uint32_t i = 0; i < GN3d::SHADOW_CASCADE_MAX; ++i) {
        CHECK_NEAR(state.aSplits[i], legacy.aSplits[i], 1e-6);
        CHECK_NEAR(state.aTexelWorld[i], legacy.aTexelWorld[i], 1e-6);
        for (int k = 0; k < 16; ++k) {
            CHECK_NEAR(state.aLightViewProj[i].m[k], legacy.aLightViewProj[i].m[k], 1e-6);
        }
    }
}

// ── cRENDER_FRAME：没有设备时的行为必须是"明确失败 + 不崩" ──────────
// 单测进程不建 Vulkan 设备，所以这里走的就是"引擎未初始化"分支。
// 这条覆盖的正是 cRENDER_FRAME 最容易写错的地方：Begin 失败时析构不能去 End 一个
// 没开始的 pass（那会把 s_in3DPass 状态搞乱，重则画面永久冻结）。
TEST_CASE(entityRenderFrameWithoutDevice) {
    GNg::cRENDER_FRAME frame;
    CHECK(!frame.Active());

    GNg::sRENDER_FRAME_DESC desc;
    CHECK(!frame.Begin(desc)); // aViewProj 为空
    CHECK(!frame.Active());

    GNg::sMAT44 viewProj(true);
    desc.aViewProj = &viewProj;
    CHECK(!frame.Begin(desc)); // 没有设备：进不去 3D pass
    CHECK(!frame.Active());
    frame.End(); // 收尾幂等
    frame.End();
    CHECK(!frame.Active());

    // 阴影：没有设备 + 没有 caster 都是"什么都没做"
    CHECK(!frame.ShadowPass(static_cast<const GN3d::cMODEL *>(nullptr), nullptr, 4u));
    const GN3d::cMODEL *casters[2] = {nullptr, nullptr};
    CHECK(!frame.ShadowPass(casters, 2u, nullptr, 4u));
    CHECK(!frame.ShadowPass(casters, 0u, nullptr, 4u)); // 0 个 caster
}

// ── C1：相机跟随目标（BuildCameraTarget）─────────────
// 这个 helper 存在的意义就是"别让每个调用点自己挑位置来源"。三条优先级各一个用例。
TEST_CASE(entityBuildCameraTargetFromModel) {
    cENTITY entity;
    cMODEL *model = CreateCubeModel(2.0f);
    CHECK(model != nullptr);
    model->SetPosition(3.0f, 5.0f, -2.0f);
    model->computeWorldAABB();
    entity.aMesh.SetModel(model);

    sCAMERA_TARGET target;
    // 无高度偏移 ⇒ AABB 中心
    CHECK(entity.BuildCameraTarget(&target, 0.0f));
    CHECK(target.aValid);
    CHECK_NEAR(target.aPosition.x, 3.0f, 1e-4);
    CHECK_NEAR(target.aPosition.y, 5.0f, 1e-4);
    CHECK_NEAR(target.aPosition.z, -2.0f, 1e-4);
    CHECK(target.aBounds.IsValid());
    CHECK_NEAR(target.aBounds.aMin.y, 4.0f, 1e-4);

    // 带高度偏移 ⇒ 在中心之上（第三人称看向头部）
    CHECK(entity.BuildCameraTarget(&target, 0.8f));
    CHECK_NEAR(target.aPosition.y, 5.8f, 1e-4);

    entity.aMesh.SetModel(nullptr);
    delete model;
}

// 角色优先于网格：胶囊中心比模型 AABB 中心更适合当相机目标。
TEST_CASE(entityBuildCameraTargetPrefersCharacter) {
    cENTITY entity;
    cMODEL *model = CreateCubeModel(2.0f);
    model->SetPosition(0.0f, 5.0f, 0.0f);
    model->computeWorldAABB();
    entity.aMesh.SetModel(model);

    sCAMERA_TARGET target;
    CHECK(entity.BuildCameraTarget(&target, 0.0f));
    const float modelY = target.aPosition.y;

    // 初始化角色在别处：目标应该切到胶囊中心
    sCHARACTER_DESC desc;
    desc.aHeight = 1.7f;
    desc.aRadius = 0.3f;
    entity.aCharacter.Init(desc);
    entity.aCharacter.SetPosition(GNg::sVEC3{10.0f, 1.0f, 0.0f});

    CHECK(entity.BuildCameraTarget(&target, 0.0f));
    CHECK_NEAR(target.aPosition.x, 10.0f, 1e-4); // 用胶囊的位置
    CHECK_NEAR(target.aPosition.y, 1.0f, 1e-4);
    CHECK(std::fabs(target.aPosition.y - modelY) > 1.0f); // 确实不是模型那个

    entity.aMesh.SetModel(nullptr);
    delete model;
}

// 什么都没有：返回 false，**不要**给一个"看起来像原点"的假目标。
TEST_CASE(entityBuildCameraTargetWithoutVisual) {
    cENTITY entity;
    sCAMERA_TARGET target;
    target.aPosition = GNg::sVEC3{7.0f, 7.0f, 7.0f}; // 预填一个可识别的值
    CHECK(!entity.BuildCameraTarget(&target, 0.0f));
    CHECK(!target.aValid);
    // 位置不该被写成原点（虽然 aValid=false 时调用方应当忽略它）
    CHECK_NEAR(target.aPosition.x, 7.0f, 1e-6);

    // 空指针安全
    CHECK(!entity.BuildCameraTarget(nullptr, 0.0f));
}

// 端到端：把目标喂给真实相机，相机确实朝它移动。
TEST_CASE(entityCameraFollowsEntity) {
    cENTITY entity;
    cMODEL *model = CreateCubeModel(1.0f);
    model->SetPosition(0.0f, 0.0f, 0.0f);
    model->computeWorldAABB();
    entity.aMesh.SetModel(model);

    cCAMERA camera;
    camera.Init();
    camera.SetOrbit(GNg::sVEC3{0.0f, 0.0f, 0.0f}, 8.0f, 0.0f, 0.25f);

    sCAMERA_TARGET target;
    CHECK(entity.BuildCameraTarget(&target, 0.0f));
    camera.SetTarget(target, 0.25f);
    CHECK(camera.HasTarget());
    for (int i = 0; i < 30; ++i) camera.Update(1.0f / 60.0f);
    // 焦点是私有的（只暴露 SetOrbitFocus），所以断言"相机世界位置"——
    // 轨道模式下距离固定，焦点往 +X 走会带着相机一起平移。
    const float beforeX = camera.GetCoordV().x;

    // 实体跑到 +X 6 m 处，相机焦点应当追过去
    model->SetPosition(6.0f, 0.0f, 0.0f);
    model->computeWorldAABB();
    CHECK(entity.BuildCameraTarget(&target, 0.0f));
    camera.SetTarget(target, 0.25f);
    for (int i = 0; i < 90; ++i) camera.Update(1.0f / 60.0f);
    CHECK(camera.GetCoordV().x > beforeX + 3.0f);

    entity.aMesh.SetModel(nullptr);
    delete model;
}
