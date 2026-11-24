#pragma once
#ifndef GN_TESTS_FIXTURES_H
#define GN_TESTS_FIXTURES_H
// tests/fixtures/fixtures.h — 程序化测试资产（P1）
//
// 为什么要有它（docs/ENTITY_CONTROLLERS_PLAN.md §5.1）：仓库里**没有任何模型资产**
// （mmdViewer 的 PMX 与 imoScene 的 GLB 都在 .gitignore 里、不入库），所以
// P2/P3/P4 的验收不能依赖外部文件。这里用代码造出最小可用的骨架与蒙皮网格：
//
//   * 骨链：N 节的直线链（默认 3 节），每节沿 +Y 长 1 m；N 可设 129 用来验 UBO/SSBO 分流；
//   * 蒙皮立方体：8 个顶点 × 68 字节 sSKINNED_VERTEX，权重按高度在两节间线性混合
//     （下半绑 root、上半绑 mid），这样"转 mid 关节，立方体上半跟着歪"是可断言的行为。
//
// 用法：
//   GN3d::sSKELETON skeleton = fixture::buildBoneChain(3);
//   std::vector<GN3d::sANIM_TRANSFORM> local = fixture::bindPoseLocal(skeleton);
//   std::vector<GN3d::sSKINNED_VERTEX> verts = fixture::buildSkinnedCube(1.0f);
//
// 也可以 `make fixtures` 把它们落盘到 .local/fixtures/（给 P2 的 GPU 用例与截图用）。
#include <GN.h>

#include <cstdint>
#include <vector>

namespace fixture {

// 每节骨长（米）。绑定姿势下第 i 个关节的世界 Y = (i+1) * BONE_LENGTH。
const float BONE_LENGTH = 1.0f;

// 直线骨链：joints[0] 为根，joints[i] 的父为 i-1；绑定姿势为"沿 +Y 逐节 1 m"。
// jointCount 至少 1（更小的值按 1 处理）。
GN3d::sSKELETON buildBoneChain(uint32_t jointCount);

// 骨链的绑定姿势局部变换（每节只有 Y 方向平移）。
std::vector<GN3d::sANIM_TRANSFORM> bindPoseLocal(const GN3d::sSKELETON &skeleton);

// 只改第 joint 个关节绕 Z 轴转 radians 的局部姿势副本（其余保持绑定姿势）。
// 用途：断言"转 mid 关节后立方体上半跟着转、下半不动"。
std::vector<GN3d::sANIM_TRANSFORM> poseWithJointRotation(
    const GN3d::sSKELETON &skeleton, uint32_t joint, float radians);

// 8 顶点立方体（边长 size，中心在原点），按高度在两节骨之间线性混合权重。
std::vector<GN3d::sSKINNED_VERTEX> buildSkinnedCube(float size = 1.0f);

// 立方体的 12 个三角形索引（6 面 × 2），与 buildSkinnedCube 的顶点顺序对应。
std::vector<uint32_t> cubeIndices();

// 把 fixture 落盘到 dir（不存在会创建）：
//   bone<N>.txt    每行 "joint parent bindY"，供外部工具核对
//   cube_skin.txt  每行 "x y z nx ny nz u v j0 w0 j1 w1"
// 返回是否全部写成功。
bool writeFixtures(const char *dir);

} // namespace fixture
#endif // GN_TESTS_FIXTURES_H
