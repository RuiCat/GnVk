// 基础工具单测：半精度打包、数值比较与 Clamp 宏。
#include "testFramework.h"
#include <GN.h>

TEST_CASE(baseHalfRoundTripPositive) {
    CHECK_NEAR(GN_ValueUnPackHalf(GN_ValuePackToHalf(1.0f)), 1.0f, 1e-3f);
    CHECK_NEAR(GN_ValueUnPackHalf(GN_ValuePackToHalf(2.0f)), 2.0f, 1e-3f);
    CHECK_NEAR(GN_ValueUnPackHalf(GN_ValuePackToHalf(0.5f)), 0.5f, 1e-3f);
}

TEST_CASE(baseHalfRoundTripNegative) {
    CHECK_NEAR(GN_ValueUnPackHalf(GN_ValuePackToHalf(-1.0f)), -1.0f, 1e-3f);
    CHECK_NEAR(GN_ValueUnPackHalf(GN_ValuePackToHalf(-2.5f)), -2.5f, 1e-3f);
}

TEST_CASE(baseHalfZero) {
    CHECK_EQ(GN_ValuePackToHalf(0.0f), 0);
    CHECK_NEAR(GN_ValueUnPackHalf(GN_ValuePackToHalf(0.0f)), 0.0f, 1e-6f);
}

TEST_CASE(baseClampMacro) {
    int over = 5;
    GN_ValueClamp(over, 0, 3);
    CHECK_EQ(over, 3);

    int under = -2;
    GN_ValueClamp(under, 0, 3);
    CHECK_EQ(under, 0);

    int inside = 2;
    GN_ValueClamp(inside, 0, 3);
    CHECK_EQ(inside, 2);
}

TEST_CASE(baseFloatCompare) {
    CHECK(GN_ValueEqualFloat(1.0f, 1.0f));
    CHECK(!GN_ValueEqualFloat(1.0f, 1.001f));
}

TEST_CASE(baseFloatToHalfCompatible) {
    CHECK_NEAR(GN_ValueUnPackHalf((short)GN_ValueFloatToHalf(1.0f)), 1.0f, 1e-3f);
    CHECK_NEAR(GN_ValueUnPackHalf((short)GN_ValueFloatToHalf(0.5f)), 0.5f, 1e-3f);
}

TEST_CASE(baseHalfToFloat) {
    CHECK_NEAR(GN_ValueHalfToFloat(0x3C00), 1.0f, 1e-6f);
    CHECK_NEAR(GN_ValueHalfToFloat(0x3800), 0.5f, 1e-6f);
    CHECK_NEAR(GN_ValueHalfToFloat(0x4000), 2.0f, 1e-6f);
    CHECK_NEAR(GN_ValueHalfToFloat(0x0000), 0.0f, 1e-6f);
    CHECK(std::signbit(GN_ValueHalfToFloat(0x8000)));
}
