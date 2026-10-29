// testSpace.cpp — 坐标空间约定的 CPU 契约测试（世界轴/正面规则/视口/绕序/UV/光照符号）。
#include "testFramework.h"

#include <GN.h>
#include "GN_Space.h"
#include "mmd/mmdMeshBuilder.h"
#include "mmd/pmxModel.h"

#include <cmath>
#include <cstdint>

using namespace GN3d;

namespace {

const float kEps = 1e-6f;

bool nearlyEqual(float a, float b) {
    return std::fabs(a - b) <= kEps;
}

} // namespace

TEST_CASE(spaceConstantsOrthonormal) {
    CHECK_NEAR(SPACE_WORLD_RIGHT.x, 1.0f, kEps);
    CHECK_NEAR(SPACE_WORLD_RIGHT.y, 0.0f, kEps);
    CHECK_NEAR(SPACE_WORLD_RIGHT.z, 0.0f, kEps);
    CHECK_NEAR(SPACE_WORLD_UP.x, 0.0f, kEps);
    CHECK_NEAR(SPACE_WORLD_UP.y, 1.0f, kEps);
    CHECK_NEAR(SPACE_WORLD_UP.z, 0.0f, kEps);
    CHECK_NEAR(SPACE_CAMERA_FORWARD.x, 0.0f, kEps);
    CHECK_NEAR(SPACE_CAMERA_FORWARD.y, 0.0f, kEps);
    CHECK_NEAR(SPACE_CAMERA_FORWARD.z, -1.0f, kEps);
    CHECK_NEAR(SPACE_ASSET_FRONT.z, 1.0f, kEps);
    CHECK_NEAR(SPACE_MMD_UNIT_SCALE, 1.0f, kEps);

    // 右手系：X × Y = Z（= 资产正面 = -相机前向）
    GNg::sVEC3 cross;
    GNg::Vec3Cross(&cross, &SPACE_WORLD_RIGHT, &SPACE_WORLD_UP);
    CHECK(nearlyEqual(cross.x, 0.0f));
    CHECK(nearlyEqual(cross.y, 0.0f));
    CHECK(nearlyEqual(cross.z, 1.0f));
}

TEST_CASE(spaceAssetFrontIsOppositeCameraForward) {
    CHECK(nearlyEqual(SPACE_ASSET_FRONT.x, -SPACE_CAMERA_FORWARD.x));
    CHECK(nearlyEqual(SPACE_ASSET_FRONT.y, -SPACE_CAMERA_FORWARD.y));
    CHECK(nearlyEqual(SPACE_ASSET_FRONT.z, -SPACE_CAMERA_FORWARD.z));
}

TEST_CASE(spaceFrontFaceWorldRule) {
    // 世界 CCW；正高度视口 + 投影 Y 翻转下 CCW 仍是帧缓冲 CCW ⇒ 世界 pass 用 CCW；
    // 不翻 Y（identity / 未翻转正交）会把 CCW 内容镜像成 CW ⇒ 用 CW。
    CHECK(frontFaceForWorldPass(true) == eSPACE_FACE_CCW);
    CHECK(frontFaceForWorldPass(false) == eSPACE_FACE_CW);
}

TEST_CASE(spaceFrontFaceScreenRule) {
    CHECK(frontFaceForScreenPass() == eSPACE_FACE_CW);
}

TEST_CASE(spaceViewportValidate) {
    CHECK(isValidViewportSize(800, 600));
    CHECK(isValidViewportSize(1, 1));
    CHECK(!isValidViewportSize(0, 600));
    CHECK(!isValidViewportSize(800, 0));
    CHECK(!isValidViewportSize(800, -600));
    CHECK(!isValidViewportSize(-1, -1));
}

TEST_CASE(spaceReverseWinding) {
    uint32_t indices[6] = {0, 1, 2, 3, 4, 5};
    CHECK(reverseWinding(indices, 6));
    CHECK_EQ(indices[0], 0u);
    CHECK_EQ(indices[1], 2u);
    CHECK_EQ(indices[2], 1u);
    CHECK_EQ(indices[3], 3u);
    CHECK_EQ(indices[4], 5u);
    CHECK_EQ(indices[5], 4u);
}

TEST_CASE(spaceReverseWindingTwiceRestores) {
    uint32_t indices[9] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
    CHECK(reverseWinding(indices, 9));
    CHECK(reverseWinding(indices, 9));
    const uint32_t expected[9] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
    for (int i = 0; i < 9; ++i) {
        CHECK_EQ(indices[i], expected[i]);
    }
}

TEST_CASE(spaceReverseWindingInvalid) {
    uint32_t indices[4] = {0, 1, 2, 3};
    CHECK(!reverseWinding(indices, 4)); // 不是 3 的倍数
    CHECK(!reverseWinding(0, 3)); // 空指针
    CHECK(reverseWinding(0, 0)); // 空且数量 0 = 无事可做
}

TEST_CASE(spaceMirrorVectorsX) {
    float values[6] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f};
    CHECK(mirrorVectorsX(values, 2, 3));
    CHECK_NEAR(values[0], -1.0f, kEps);
    CHECK_NEAR(values[1], 2.0f, kEps);
    CHECK_NEAR(values[2], 3.0f, kEps);
    CHECK_NEAR(values[3], -4.0f, kEps);
    CHECK_NEAR(values[4], 5.0f, kEps);
    CHECK_NEAR(values[5], 6.0f, kEps);

    CHECK(mirrorVectorsX(0, 0, 3)); // 空数据 = 成功
    CHECK(!mirrorVectorsX(0, 1, 3)); // 非空但空指针
    CHECK(!mirrorVectorsX(values, 1, 2)); // stride 不足 3
}

TEST_CASE(spaceFlipTextureV) {
    CHECK_NEAR(flipTextureV(0.0f), 1.0f, kEps);
    CHECK_NEAR(flipTextureV(0.25f), 0.75f, kEps);
    CHECK_NEAR(flipTextureV(0.5f), 0.5f, kEps);
    CHECK_NEAR(flipTextureV(1.0f), 0.0f, kEps);
    CHECK_NEAR(flipTextureV(1.25f), -0.25f, kEps); // 平铺 UV 也按 1-v
}

TEST_CASE(spaceFlipTextureVTwiceRestores) {
    const float samples[4] = {0.0f, 0.1f, 0.75f, 1.0f};
    for (int i = 0; i < 4; ++i) {
        CHECK_NEAR(flipTextureV(flipTextureV(samples[i])), samples[i], kEps);
    }
    const float nan = std::nanf("");
    CHECK(std::isnan(flipTextureV(nan)));
}

TEST_CASE(spaceSunLightSignRoundTrip) {
    const GNg::sVEC3 sun = {0.45f, 0.78f, 0.44f};
    const GNg::sVEC3 light = sunDirectionToLightDirection(sun);
    CHECK_NEAR(light.x, -sun.x, kEps);
    CHECK_NEAR(light.y, -sun.y, kEps);
    CHECK_NEAR(light.z, -sun.z, kEps);

    const GNg::sVEC3 back = lightDirectionToSunDirection(light);
    CHECK_NEAR(back.x, sun.x, kEps);
    CHECK_NEAR(back.y, sun.y, kEps);
    CHECK_NEAR(back.z, sun.z, kEps);
}

TEST_CASE(spaceSunLightDotIsNegative) {
    const GNg::sVEC3 sun = {0.45f, 0.78f, 0.44f};
    const GNg::sVEC3 light = sunDirectionToLightDirection(sun);
    const float dot = sun.x * light.x + sun.y * light.y + sun.z * light.z;
    CHECK(dot < 0.0f);
}

TEST_CASE(spacePlaneWindingIsCcw) {
    GN3d::cMODEL *plane = GN3d::CreatePlaneModel(2.0f, 2.0f, 1.0f, 1.0f, 1.0f);
    CHECK(plane != 0);
    if (!plane) {
        return;
    }
    CHECK_EQ(plane->aMeshCount, 1u);
    const GN3d::sMESH &mesh = plane->aMeshes[0];
    CHECK_EQ(mesh.aIndexCount, 6u);
    const float *v = mesh.aVertices;
    auto position = [v](uint32_t index, GNg::sVEC3 *out) {
        const float *p = v + index * 11u;
        out->x = p[0];
        out->y = p[1];
        out->z = p[2];
    };
    GNg::sVEC3 p0;
    GNg::sVEC3 p1;
    GNg::sVEC3 p2;
    position(mesh.aIndices[0], &p0);
    position(mesh.aIndices[1], &p1);
    position(mesh.aIndices[2], &p2);
    GNg::sVEC3 e1;
    GNg::sVEC3 e2;
    GNg::sVEC3 normal;
    e1.x = p1.x - p0.x;
    e1.y = p1.y - p0.y;
    e1.z = p1.z - p0.z;
    e2.x = p2.x - p0.x;
    e2.y = p2.y - p0.y;
    e2.z = p2.z - p0.z;
    GNg::Vec3Cross(&normal, &e1, &e2);
    CHECK(normal.y > 0.0f); // 平面法线 +Y ⇒ 绕序 CCW
    delete plane;
}

TEST_CASE(spaceMmdMirrorX) {
    mmd::PmxModel source;
    source.vertices.resize(3);
    source.vertices[0].position[0] = 1.0f;
    source.vertices[0].position[1] = 2.0f;
    source.vertices[0].position[2] = 3.0f;
    source.vertices[0].normal[0] = 1.0f;
    source.vertices[1].position[0] = 4.0f;
    source.vertices[1].position[1] = 5.0f;
    source.vertices[1].position[2] = 6.0f;
    source.vertices[2].position[0] = 7.0f;
    source.vertices[2].position[1] = 8.0f;
    source.vertices[2].position[2] = 9.0f;
    source.materials.resize(1);
    source.materials[0].indexCount = 3;
    source.indices = {0, 1, 2};

    GN3d::cMODEL model;
    std::vector<mmd::MeshTextureBinding> textures;
    CHECK(mmd::buildPmxModel(source, model, textures, 0));
    CHECK_EQ(model.aMeshCount, 1u);
    if (model.aMeshCount != 1u) {
        return;
    }
    const float *v = model.aMeshes[0].aVertices;
    CHECK_NEAR(v[0], -1.0f, kEps); // 位置 X 镜像
    CHECK_NEAR(v[1], 2.0f, kEps);
    CHECK_NEAR(v[2], 3.0f, kEps);
    CHECK_NEAR(v[3], -1.0f, kEps); // 法线 X 镜像
    CHECK_NEAR(v[4], 0.0f, kEps);
    CHECK_NEAR(v[5], 0.0f, kEps);
    // 绕序保持文件顺序；镜像后由 CW 变 CCW（世界约定）
    CHECK_EQ(model.aMeshes[0].aIndices[0], 0u);
    CHECK_EQ(model.aMeshes[0].aIndices[1], 1u);
    CHECK_EQ(model.aMeshes[0].aIndices[2], 2u);
}
