// testSkin.cpp — 蒙皮：identity 调色板不变、单关节平移、0.5/0.5 双关节混合、越界关节/零权重降级、
// 调色板 UBO 打包 128 截断。
#include "testFramework.h"

#include <GN.h>
#include "GN_3DSkin.h"

#include <cmath>
#include <vector>

using namespace GN3d;

namespace {

GNg::sMAT44 identityMatrix() {
    GNg::sMAT44 matrix;
    matrix.Init(true);
    return matrix;
}

GNg::sMAT44 translationMatrix(float x, float y, float z) {
    GNg::sMAT44 matrix = identityMatrix();
    matrix.m[12] = x;
    matrix.m[13] = y;
    matrix.m[14] = z;
    return matrix;
}

GNg::sMAT44 rotationZMatrix(float radians) {
    GNg::sMAT44 matrix = identityMatrix();
    const float c = std::cos(radians);
    const float s = std::sin(radians);
    matrix.m[0] = c;
    matrix.m[1] = s;
    matrix.m[4] = -s;
    matrix.m[5] = c;
    return matrix;
}

sSKINNED_VERTEX makeVertex() {
    sSKINNED_VERTEX vertex;
    vertex.aPosition = GNg::sVEC3{1.0f, 2.0f, 3.0f};
    vertex.aNormal = GNg::sVEC3{0.0f, 1.0f, 0.0f};
    vertex.aUv[0] = 0.25f;
    vertex.aUv[1] = 0.75f;
    vertex.aTangent = GNg::sVEC3{1.0f, 0.0f, 0.0f};
    vertex.aJoints[0] = 0;
    vertex.aJoints[1] = 0;
    vertex.aJoints[2] = 0;
    vertex.aJoints[3] = 0;
    vertex.aWeights[0] = 1.0f;
    vertex.aWeights[1] = 0.0f;
    vertex.aWeights[2] = 0.0f;
    vertex.aWeights[3] = 0.0f;
    return vertex;
}

bool isFinite(const GNg::sVEC3 &vector) {
    return std::isfinite(vector.x) && std::isfinite(vector.y) && std::isfinite(vector.z);
}

} // namespace

// identity 调色板：位置与法线原样输出。
TEST_CASE(skinIdentityPaletteKeepsVertex) {
    GNg::sMAT44 palette[1] = {identityMatrix()};
    const sSKINNED_VERTEX vertex = makeVertex();
    GNg::sVEC3 position;
    GNg::sVEC3 normal;
    CHECK(skinVertices(&vertex, 1, palette, 1, &position, &normal));
    CHECK_NEAR(position.x, 1.0f, 1e-5f);
    CHECK_NEAR(position.y, 2.0f, 1e-5f);
    CHECK_NEAR(position.z, 3.0f, 1e-5f);
    CHECK_NEAR(normal.x, 0.0f, 1e-5f);
    CHECK_NEAR(normal.y, 1.0f, 1e-5f);
    CHECK_NEAR(normal.z, 0.0f, 1e-5f);
}

// 单关节平移：位置跟随平移，法线方向不被平移改变。
TEST_CASE(skinSingleJointTranslation) {
    GNg::sMAT44 palette[1] = {translationMatrix(10.0f, -2.0f, 0.5f)};
    sSKINNED_VERTEX vertex = makeVertex();
    vertex.aJoints[0] = 0;
    vertex.aWeights[0] = 1.0f;
    GNg::sVEC3 position;
    GNg::sVEC3 normal;
    CHECK(skinVertices(&vertex, 1, palette, 1, &position, &normal));
    CHECK_NEAR(position.x, 11.0f, 1e-5f);
    CHECK_NEAR(position.y, 0.0f, 1e-5f);
    CHECK_NEAR(position.z, 3.5f, 1e-5f);
    CHECK_NEAR(normal.x, 0.0f, 1e-5f);
    CHECK_NEAR(normal.y, 1.0f, 1e-5f);
    CHECK_NEAR(normal.z, 0.0f, 1e-5f);
}

// 两个 0.5/0.5 关节：位置取两个平移结果的中间值。
TEST_CASE(skinTwoJointBlendIsAverage) {
    GNg::sMAT44 palette[2] = {translationMatrix(0.0f, 0.0f, 0.0f),
                              translationMatrix(2.0f, 0.0f, 0.0f)};
    sSKINNED_VERTEX vertex = makeVertex();
    vertex.aJoints[0] = 0;
    vertex.aJoints[1] = 1;
    vertex.aWeights[0] = 0.5f;
    vertex.aWeights[1] = 0.5f;
    GNg::sVEC3 position;
    GNg::sVEC3 normal;
    CHECK(skinVertices(&vertex, 1, palette, 2, &position, &normal));
    CHECK_NEAR(position.x, 2.0f, 1e-5f); // 1 + (0 + 2) * 0.5
    CHECK_NEAR(position.y, 2.0f, 1e-5f);
    CHECK_NEAR(position.z, 3.0f, 1e-5f);
    CHECK_NEAR(normal.y, 1.0f, 1e-5f);
}

// 法线走 3x3：绕 Z 轴 90° 后 +Y 法线变成 -X（不受平移影响）。
TEST_CASE(skinNormalRotatesWithUpper3x3) {
    GNg::sMAT44 palette[1] = {rotationZMatrix(1.5707963f)};
    sSKINNED_VERTEX vertex = makeVertex();
    vertex.aPosition = GNg::sVEC3{0.0f, 0.0f, 0.0f};
    vertex.aNormal = GNg::sVEC3{0.0f, 1.0f, 0.0f};
    GNg::sVEC3 position;
    GNg::sVEC3 normal;
    CHECK(skinVertices(&vertex, 1, palette, 1, &position, &normal));
    CHECK_NEAR(normal.x, -1.0f, 1e-4f);
    CHECK_NEAR(normal.y, 0.0f, 1e-4f);
    CHECK_NEAR(normal.z, 0.0f, 1e-4f);
}

// 越界关节钳到最后一个、零权重退化为原顶点、空指针返回 false，结果均有限。
TEST_CASE(skinDegradesOnBadJointsAndWeights) {
    GNg::sMAT44 palette[2] = {translationMatrix(0.0f, 0.0f, 0.0f),
                              translationMatrix(5.0f, 0.0f, 0.0f)};

    sSKINNED_VERTEX clamped = makeVertex();
    clamped.aJoints[0] = 99; // 越界 → 钳到 1
    clamped.aJoints[1] = 99;
    clamped.aWeights[0] = 1.0f;
    clamped.aWeights[1] = 0.0f;
    GNg::sVEC3 position;
    GNg::sVEC3 normal;
    CHECK(skinVertices(&clamped, 1, palette, 2, &position, &normal));
    CHECK_NEAR(position.x, 6.0f, 1e-5f); // 1 + 5
    CHECK(isFinite(position) && isFinite(normal));

    sSKINNED_VERTEX zeroWeights = makeVertex();
    zeroWeights.aWeights[0] = 0.0f;
    zeroWeights.aWeights[1] = 0.0f;
    zeroWeights.aWeights[2] = 0.0f;
    zeroWeights.aWeights[3] = 0.0f;
    CHECK(skinVertices(&zeroWeights, 1, palette, 2, &position, &normal));
    CHECK_NEAR(position.x, 1.0f, 1e-5f);
    CHECK_NEAR(position.y, 2.0f, 1e-5f);
    CHECK_NEAR(position.z, 3.0f, 1e-5f);
    CHECK_NEAR(normal.y, 1.0f, 1e-5f);

    sSKINNED_VERTEX negativeWeights = makeVertex();
    negativeWeights.aJoints[0] = 1;
    negativeWeights.aWeights[0] = -1.0f; // 负权重按 0 处理 → 退化
    CHECK(skinVertices(&negativeWeights, 1, palette, 2, &position, &normal));
    CHECK_NEAR(position.x, 1.0f, 1e-5f);
    CHECK(isFinite(position) && isFinite(normal));

    CHECK(!skinVertices(nullptr, 1, palette, 2, &position, &normal));
    CHECK(!skinVertices(&clamped, 1, palette, 2, nullptr, &normal));
    CHECK(skinVertices(&clamped, 0, palette, 2, &position, &normal));
}

// 调色板打包：按 count 写入、超过 128 截断、未用槽位清零。
TEST_CASE(packPaletteTruncatesAt128) {
    std::vector<GNg::sMAT44> palette(200);
    for(uint32_t index = 0; index < palette.size(); ++index) {
        palette[index] = translationMatrix((float)index, 0.0f, 0.0f);
    }

    sSKIN_UBO ubo;
    CHECK_EQ(packPalette(palette.data(), 200, ubo), 128u);
    CHECK_NEAR(ubo.aJointMatrices[0][12], 0.0f, 1e-5f);
    CHECK_NEAR(ubo.aJointMatrices[127][12], 127.0f, 1e-5f);

    CHECK_EQ(packPalette(palette.data(), 2, ubo), 2u);
    CHECK_NEAR(ubo.aJointMatrices[1][12], 1.0f, 1e-5f);
    CHECK_NEAR(ubo.aJointMatrices[2][12], 0.0f, 1e-5f); // 未用槽位清零，不能残留上一次打包的 2

    CHECK_EQ(packPalette(nullptr, 4, ubo), 0u);
}
