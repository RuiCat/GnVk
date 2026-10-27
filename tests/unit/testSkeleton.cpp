// testSkeleton.cpp — 骨骼：世界矩阵（父链）、调色板（world*inverseBind）、绑定姿势恒等、输入校验。
#include "testFramework.h"

#include <GN.h>
#include "GN_3Dskeleton.h"

#include <cmath>
#include <vector>

using namespace GN3d;

namespace {

GNg::sMAT44 identityMatrix() {
    GNg::sMAT44 matrix;
    matrix.Init(true);
    return matrix;
}

sANIM_TRANSFORM makeTranslation(float x, float y, float z) {
    sANIM_TRANSFORM transform;
    transform.aTranslation = GNg::sVEC3{x, y, z};
    transform.aRotation = GNg::sQUAT();
    transform.aScale = GNg::sVEC3{1.0f, 1.0f, 1.0f};
    return transform;
}

sJOINT makeJoint(int32_t parent) {
    sJOINT joint;
    joint.aParent = parent;
    joint.aInverseBind = identityMatrix();
    joint.aBindTranslation = GNg::sVEC3{0.0f, 0.0f, 0.0f};
    joint.aBindRotation = GNg::sQUAT();
    joint.aBindScale = GNg::sVEC3{1.0f, 1.0f, 1.0f};
    return joint;
}

GNg::sMAT44 translationMatrix(float x, float y, float z) {
    GNg::sMAT44 matrix = identityMatrix();
    matrix.Elem(3, 0) = x;
    matrix.Elem(3, 1) = y;
    matrix.Elem(3, 2) = z;
    return matrix;
}

// 平移矩阵的逆（本测试不依赖 Mat44Inverse：该函数对纯平移矩阵返回错误结果，已另行报告）。
GNg::sMAT44 inverseTranslationMatrix(float x, float y, float z) {
    return translationMatrix(-x, -y, -z);
}

} // namespace

// 空骨骼：等长输入（都为空）返回 true，palette 为空。
TEST_CASE(skeletonEmpty) {
    sSKELETON skeleton;
    std::vector<sANIM_TRANSFORM> local;
    std::vector<GNg::sMAT44> palette;
    CHECK(skeleton.computePalette(local, palette));
    CHECK_EQ(skeleton.jointCount(), 0u);
    CHECK(palette.empty());
}

// 单根：世界矩阵平移 = 局部平移；inverseBind 为 identity 时 palette = world。
TEST_CASE(skeletonSingleRoot) {
    sSKELETON skeleton;
    skeleton.aJoints.push_back(makeJoint(-1));
    std::vector<sANIM_TRANSFORM> local;
    local.push_back(makeTranslation(1.0f, 2.0f, 3.0f));
    std::vector<GNg::sMAT44> palette;
    CHECK(skeleton.computePalette(local, palette));
    CHECK_EQ(palette.size(), 1u);
    CHECK_NEAR(palette[0].Elem(3, 0), 1.0f, 1e-5f);
    CHECK_NEAR(palette[0].Elem(3, 1), 2.0f, 1e-5f);
    CHECK_NEAR(palette[0].Elem(3, 2), 3.0f, 1e-5f);
}

// 两节骨骼：子世界平移 = 父 + 子局部平移（父链叠加）。
TEST_CASE(skeletonParentChain) {
    sSKELETON skeleton;
    skeleton.aJoints.push_back(makeJoint(-1));
    skeleton.aJoints.push_back(makeJoint(0));
    std::vector<sANIM_TRANSFORM> local;
    local.push_back(makeTranslation(0.0f, 1.0f, 0.0f));
    local.push_back(makeTranslation(0.0f, 2.0f, 0.0f));
    std::vector<GNg::sMAT44> palette;
    CHECK(skeleton.computePalette(local, palette));
    CHECK_EQ(palette.size(), 2u);
    CHECK_NEAR(palette[1].Elem(3, 1), 3.0f, 1e-5f);
}

// 绑定姿势恒等：local 等于绑定姿势时 palette ≈ identity。
TEST_CASE(skeletonBindPoseGivesIdentity) {
    sSKELETON skeleton;
    sJOINT joint = makeJoint(-1);
    joint.aBindTranslation = GNg::sVEC3{0.0f, 1.0f, 0.0f};
    joint.aInverseBind = inverseTranslationMatrix(0.0f, 1.0f, 0.0f);
    skeleton.aJoints.push_back(joint);

    std::vector<sANIM_TRANSFORM> local;
    local.push_back(makeTranslation(0.0f, 1.0f, 0.0f));
    std::vector<GNg::sMAT44> palette;
    CHECK(skeleton.computePalette(local, palette));
    CHECK_EQ(palette.size(), 1u);
    CHECK_NEAR(palette[0].Elem(0, 0), 1.0f, 1e-5f);
    CHECK_NEAR(palette[0].Elem(1, 1), 1.0f, 1e-5f);
    CHECK_NEAR(palette[0].Elem(2, 2), 1.0f, 1e-5f);
    CHECK_NEAR(palette[0].Elem(3, 0), 0.0f, 1e-5f);
    CHECK_NEAR(palette[0].Elem(3, 1), 0.0f, 1e-5f);
}

// 坏父索引（父 >= 自身）：按根降级，不崩溃。
TEST_CASE(skeletonBadParentDegradesToRoot) {
    sSKELETON skeleton;
    skeleton.aJoints.push_back(makeJoint(1)); // 父索引大于自身
    skeleton.aJoints.push_back(makeJoint(-1));
    std::vector<sANIM_TRANSFORM> local;
    local.push_back(makeTranslation(1.0f, 0.0f, 0.0f));
    local.push_back(makeTranslation(0.0f, 2.0f, 0.0f));
    std::vector<GNg::sMAT44> palette;
    CHECK(skeleton.computePalette(local, palette));
    CHECK_NEAR(palette[0].Elem(3, 0), 1.0f, 1e-5f); // 未被父级叠加
    CHECK_NEAR(palette[1].Elem(3, 1), 2.0f, 1e-5f);
}

// 输入长度不匹配：返回 false。
TEST_CASE(skeletonMismatchedInput) {
    sSKELETON skeleton;
    skeleton.aJoints.push_back(makeJoint(-1));
    std::vector<sANIM_TRANSFORM> local; // 空
    std::vector<GNg::sMAT44> palette;
    CHECK(!skeleton.computePalette(local, palette));
}
