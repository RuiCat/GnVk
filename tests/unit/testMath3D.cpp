// 3D 数学单测：sVEC3 点积/长度/归一化、sMAT44 单位矩阵与变换。
#include "testFramework.h"
#include <GN.h>

TEST_CASE(math3dDot) {
    GNg::sVEC3 a = {1.0f, 2.0f, 3.0f};
    GNg::sVEC3 b = {4.0f, 5.0f, 6.0f};
    CHECK_NEAR(GNg::Vec3Dot(&a, &b), 32.0f, 1e-6f);
}

TEST_CASE(math3dLength) {
    GNg::sVEC3 a = {1.0f, 2.0f, 3.0f};
    CHECK_NEAR(GNg::Vec3Length(&a), std::sqrt(14.0f), 1e-6f);
}

TEST_CASE(math3dNormalize) {
    GNg::sVEC3 a = {0.0f, 3.0f, 0.0f};
    GNg::sVEC3 out = {0.0f, 0.0f, 0.0f};
    GNg::Vec3Normalize(&out, &a);
    CHECK_NEAR(out.x, 0.0f, 1e-6f);
    CHECK_NEAR(out.y, 1.0f, 1e-6f);
    CHECK_NEAR(out.z, 0.0f, 1e-6f);
}

TEST_CASE(math3dIdentityMatrix) {
    GNg::sMAT44 identity(true);
    CHECK_NEAR(identity.Elem(0, 0), 1.0f, 1e-6f);
    CHECK_NEAR(identity.Elem(1, 1), 1.0f, 1e-6f);
    CHECK_NEAR(identity.Elem(2, 2), 1.0f, 1e-6f);
    CHECK_NEAR(identity.Elem(3, 3), 1.0f, 1e-6f);
    CHECK_NEAR(identity.Elem(0, 1), 0.0f, 1e-6f);

    GNg::sMAT44 identity2;
    GNg::Mat44Identity(&identity2);
    CHECK_NEAR(identity2.Elem(3, 3), 1.0f, 1e-6f);
}

TEST_CASE(math3dTranslate) {
    GNg::sMAT44 translate;
    GNg::Mat44Translate(&translate, 2.0f, 3.0f, 4.0f);
    CHECK_NEAR(translate.Elem(3, 0), 2.0f, 1e-6f);
    CHECK_NEAR(translate.Elem(3, 1), 3.0f, 1e-6f);
    CHECK_NEAR(translate.Elem(3, 2), 4.0f, 1e-6f);
}

TEST_CASE(math3dTransformCoord) {
    GNg::sMAT44 translate;
    GNg::Mat44Translate(&translate, 2.0f, 3.0f, 4.0f);
    GNg::sVEC3 in = {1.0f, 1.0f, 1.0f};
    GNg::sVEC3 out = {0.0f, 0.0f, 0.0f};
    GNg::Mat44TransformCoord(&out, &in, &translate);
    CHECK_NEAR(out.x, 3.0f, 1e-6f);
    CHECK_NEAR(out.y, 4.0f, 1e-6f);
    CHECK_NEAR(out.z, 5.0f, 1e-6f);
}

TEST_CASE(math3dMulIdentity) {
    GNg::sMAT44 identity(true);
    GNg::sMAT44 translate;
    GNg::Mat44Translate(&translate, 2.0f, 3.0f, 4.0f);

    GNg::sMAT44 product;
    GNg::Mat44Mul(&product, &identity, &translate);
    GNg::sVEC3 in = {1.0f, 1.0f, 1.0f};
    GNg::sVEC3 out = {0.0f, 0.0f, 0.0f};
    GNg::Mat44TransformCoord(&out, &in, &product);
    CHECK_NEAR(out.x, 3.0f, 1e-6f);
    CHECK_NEAR(out.y, 4.0f, 1e-6f);
    CHECK_NEAR(out.z, 5.0f, 1e-6f);
}

TEST_CASE(math3dLengthSq) {
    GNg::sVEC3 a = {1.0f, 2.0f, 3.0f};
    CHECK_NEAR(GNg::Vec3LengthSq(&a), 14.0f, 1e-6f);
}

TEST_CASE(math3dAddSubScale) {
    GNg::sVEC3 a = {1.0f, 2.0f, 3.0f};
    GNg::sVEC3 b = {4.0f, 5.0f, 6.0f};
    GNg::sVEC3 out = {0.0f, 0.0f, 0.0f};

    GNg::Vec3Add(&out, &a, &b);
    CHECK_NEAR(out.x, 5.0f, 1e-6f);
    CHECK_NEAR(out.y, 7.0f, 1e-6f);
    CHECK_NEAR(out.z, 9.0f, 1e-6f);

    GNg::Vec3Sub(&out, &b, &a);
    CHECK_NEAR(out.x, 3.0f, 1e-6f);
    CHECK_NEAR(out.y, 3.0f, 1e-6f);
    CHECK_NEAR(out.z, 3.0f, 1e-6f);

    GNg::Vec3Scale(&out, &a, 2.0f);
    CHECK_NEAR(out.x, 2.0f, 1e-6f);
    CHECK_NEAR(out.y, 4.0f, 1e-6f);
    CHECK_NEAR(out.z, 6.0f, 1e-6f);
}

// ── Mat44Inverse 回归（列主序）：旧实现对纯平移矩阵返回错误；
//    tests/unit/testSkeleton.cpp:46 与 src/graphics/_Vulkan/vkCloud.cpp:11 均有登记。
namespace {

void checkMatrixIdentity(const GNg::sMAT44 &m, float tolerance) {
    for(int col = 0; col < 4; ++col) {
        for(int row = 0; row < 4; ++row) {
            const float expected = (col == row) ? 1.0f : 0.0f;
            CHECK_NEAR(m.Elem(col, row), expected, tolerance);
        }
    }
}

} // namespace

TEST_CASE(math3dInverseTranslation) {
    GNg::sMAT44 translate;
    GNg::Mat44Translate(&translate, 2.0f, -3.0f, 4.0f);
    GNg::sMAT44 inverse;
    GNg::Mat44Inverse(&inverse, &translate);
    CHECK_NEAR(inverse.Elem(3, 0), -2.0f, 1e-6f);
    CHECK_NEAR(inverse.Elem(3, 1), 3.0f, 1e-6f);
    CHECK_NEAR(inverse.Elem(3, 2), -4.0f, 1e-6f);
    GNg::sMAT44 product;
    GNg::Mat44Mul(&product, &translate, &inverse);
    checkMatrixIdentity(product, 1e-5f);
}

TEST_CASE(math3dInverseTrs) {
    GNg::sMAT44 translate;
    GNg::sMAT44 rotate;
    GNg::sMAT44 scale;
    GNg::Mat44Translate(&translate, 1.0f, 2.0f, 3.0f);
    GNg::Mat44RotateY(&rotate, 0.7f);
    GNg::Mat44Scale(&scale, 2.0f, 0.5f, 1.5f);
    GNg::sMAT44 trs;
    GNg::sMAT44 tmp;
    GNg::Mat44Mul(&tmp, &translate, &rotate);
    GNg::Mat44Mul(&trs, &tmp, &scale);
    GNg::sMAT44 inverse;
    GNg::Mat44Inverse(&inverse, &trs);
    GNg::sMAT44 product;
    GNg::Mat44Mul(&product, &trs, &inverse);
    checkMatrixIdentity(product, 1e-5f);
}

TEST_CASE(math3dInversePerspective) {
    GNg::sMAT44 perspective;
    GNg::Mat44PerspectiveVK(&perspective, 1.0471976f, 16.0f / 9.0f, 0.1f, 100.0f);
    GNg::sMAT44 inverse;
    GNg::Mat44Inverse(&inverse, &perspective);
    GNg::sMAT44 product;
    GNg::Mat44Mul(&product, &perspective, &inverse);
    checkMatrixIdentity(product, 1e-5f);
}

TEST_CASE(math3dInverseSingularFallsBackToIdentity) {
    GNg::sMAT44 zero; // sMAT44 有用户构造函数，不能用 ={} 指望清零
    for(int i = 0; i < 16; ++i) zero.m[i] = 0.0f;
    GNg::sMAT44 inverse;
    GNg::Mat44Inverse(&inverse, &zero);
    checkMatrixIdentity(inverse, 1e-6f);
}
