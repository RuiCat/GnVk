// 2D 数学单测：sVEC2 运算、点积、长度与分量比较。
#include "testFramework.h"
#include <GN.h>

TEST_CASE(math2dAddSub) {
    GNg::sVEC2 a(1.0f, 2.0f);
    GNg::sVEC2 b(3.0f, 4.0f);

    GNg::sVEC2 sum = a + b;
    CHECK_NEAR(sum.x, 4.0f, 1e-6f);
    CHECK_NEAR(sum.y, 6.0f, 1e-6f);

    GNg::sVEC2 diff = b - a;
    CHECK_NEAR(diff.x, 2.0f, 1e-6f);
    CHECK_NEAR(diff.y, 2.0f, 1e-6f);
}

TEST_CASE(math2dScale) {
    GNg::sVEC2 a(1.0f, -2.0f);
    GNg::sVEC2 scaled = a * 3.0f;
    CHECK_NEAR(scaled.x, 3.0f, 1e-6f);
    CHECK_NEAR(scaled.y, -6.0f, 1e-6f);
}

TEST_CASE(math2dDot) {
    GNg::sVEC2 a(1.0f, 2.0f);
    GNg::sVEC2 b(3.0f, 4.0f);
    CHECK_NEAR(GNg::Vec2Dot(&a, &b), 11.0f, 1e-6f);
}

TEST_CASE(math2dLength) {
    GNg::sVEC2 a(3.0f, 4.0f);
    CHECK_NEAR(GNg::Vec2Length(&a), 5.0f, 1e-6f);
    CHECK_NEAR(a.Length(), 5.0f, 1e-6f);
}

TEST_CASE(math2dEqual) {
    GNg::sVEC2 a(1.0f, 2.0f);
    GNg::sVEC2 b(1.0f, 2.0f);
    GNg::sVEC2 c(2.0f, 1.0f);
    CHECK(a == b);
    CHECK(a != c);
}

TEST_CASE(math2dMinMax) {
    GNg::sVEC2 a(1.0f, 5.0f);
    CHECK_NEAR(a.Max(), 5.0f, 1e-6f);
    CHECK_NEAR(a.Min(), 1.0f, 1e-6f);
}

TEST_CASE(math2dCompoundAssign) {
    GNg::sVEC2 a(1.0f, 2.0f);
    GNg::sVEC2 b(3.0f, 4.0f);
    a += b;
    CHECK_NEAR(a.x, 4.0f, 1e-6f);
    CHECK_NEAR(a.y, 6.0f, 1e-6f);
    a *= 2.0f;
    CHECK_NEAR(a.x, 8.0f, 1e-6f);
    CHECK_NEAR(a.y, 12.0f, 1e-6f);
}

TEST_CASE(math2dUnaryNegate) {
    GNg::sVEC2 a(1.0f, -2.0f);
    GNg::sVEC2 negated = -a;
    CHECK_NEAR(negated.x, -1.0f, 1e-6f);
    CHECK_NEAR(negated.y, 2.0f, 1e-6f);
}
