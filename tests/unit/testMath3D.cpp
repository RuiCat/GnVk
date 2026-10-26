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
