// testLightPick.cpp — 光源侧的两块纯 CPU 能力（光照这一轮新增）：
//   * B2b：光源物理半径（PCSS 半影宽度）的存取与钳制；
//   * B2c：多光源 cube shadow 的**选灯**（引擎只有一张 cube map，必须挑一盏）。
#include "testFramework.h"

#include <GN.h>
#include "GN_3Dlight.h"

#include <cmath>
#include <cstring>

using namespace GN3d;

namespace {

// 直接构造一个 sLIGHT_UBO（不经过 cLIGHT），便于精确控制字段
GN3d::sLIGHT_UBO makeEmpty() {
    GN3d::sLIGHT_UBO light;
    std::memset(&light, 0, sizeof(light));
    return light;
}

// 追加一盏点光。shadowType: 0=无影, 2=cube shadow
void addPoint(GN3d::sLIGHT_UBO &light, float x, float y, float z, float intensity,
              float shadowType) {
    const int index = (int)light.lightCount;
    if (index >= GN_MAX_LIGHTS) return;
    GN3d::sLIGHT_GPU &L = light.lights[index];
    L.color[0] = L.color[1] = L.color[2] = 1.0f;
    L.color[3] = intensity;
    L.position[0] = x;
    L.position[1] = y;
    L.position[2] = z;
    L.position[3] = 1.0f; // POINT
    L.direction[3] = shadowType;
    light.lightCount = (float)(index + 1);
}

} // namespace

// ── B2b：光源物理半径 ─────────────────────────────────
TEST_CASE(lightRadiusDefaultsAndClamps) {
    cLIGHT light;
    // 默认 0.1（"细灯丝"尺度）
    const int point = light.AddPoint(GNg::sVEC3{0.0f, 1.0f, 0.0f}, 8.0f,
                                     GNg::sVEC3{1.0f, 1.0f, 1.0f}, 1.0f, true);
    CHECK(point >= 0);
    CHECK_NEAR(light.GetLightRadius(point), 0.1f, 1e-5);

    // 显式给半径
    const int big = light.AddPoint(GNg::sVEC3{2.0f, 1.0f, 0.0f}, 8.0f,
                                    GNg::sVEC3{1.0f, 1.0f, 1.0f}, 1.0f, true, 0.35f);
    CHECK_NEAR(light.GetLightRadius(big), 0.35f, 1e-5);

    // 非法值（0/负）回落到默认，而不是留 0（0 会让 PCSS 完全没有半影）
    const int zero = light.AddPoint(GNg::sVEC3{4.0f, 1.0f, 0.0f}, 8.0f,
                                     GNg::sVEC3{1.0f, 1.0f, 1.0f}, 1.0f, true, 0.0f);
    CHECK_NEAR(light.GetLightRadius(zero), 0.1f, 1e-5);

    // 聚光同样支持
    const int spot = light.AddSpot(GNg::sVEC3{0.0f, 5.0f, 0.0f}, GNg::sVEC3{0.0f, -1.0f, 0.0f},
                                    12.0f, 0.2f, 0.4f, GNg::sVEC3{1.0f, 1.0f, 1.0f}, 1.0f, true,
                                    0.22f);
    CHECK_NEAR(light.GetLightRadius(spot), 0.22f, 1e-5);

    // SetLightRadius 钳到 >= 0；越界索引安全
    light.SetLightRadius(point, -3.0f);
    CHECK_NEAR(light.GetLightRadius(point), 0.0f, 1e-6);
    CHECK_NEAR(light.GetLightRadius(-1), 0.0f, 1e-6);
    CHECK_NEAR(light.GetLightRadius(999), 0.0f, 1e-6);
}

// 半径真的写进了 UBO（shader 读的是 coneAngles.z）
TEST_CASE(lightRadiusWrittenToUbo) {
    cLIGHT light;
    const int index = light.AddPoint(GNg::sVEC3{0.0f, 1.0f, 0.0f}, 8.0f,
                                      GNg::sVEC3{1.0f, 1.0f, 1.0f}, 1.0f, true, 0.42f);
    const sLIGHT_GPU &gpu = light.GetUBO()->lights[index];
    CHECK_NEAR(gpu.coneAngles[2], 0.42f, 1e-5); // [2] = 光源半径
    // [0]/[1] 是锥角，点光必须是 0（否则被当成聚光）
    CHECK_NEAR(gpu.coneAngles[0], 0.0f, 1e-6);
    CHECK_NEAR(gpu.coneAngles[1], 0.0f, 1e-6);
    // shadowType 写在 direction.w
    CHECK_NEAR(gpu.direction[3], 2.0f, 1e-6); // 2 = cube shadow（不是 1！）
}

// ── B2c：选灯 ─────────────────────────────────────────
TEST_CASE(lightPickChoosesNearestToCamera) {
    GN3d::sLIGHT_UBO light = makeEmpty();
    addPoint(light, 0.0f, 1.0f, 0.0f, 2.0f, 2.0f);   // index 0：离相机 5
    addPoint(light, 0.0f, 1.0f, 20.0f, 2.0f, 2.0f);  // index 1：离相机 15
    const GNg::sVEC3 camera = {0.0f, 1.0f, 5.0f};

    const sCUBE_SHADOW_PICK pick = pickCubeShadowLight(light, camera);
    CHECK_EQ(pick.aLightIndex, 0); // 近的赢
    CHECK(pick.aScore > 0.0f);

    // 把相机移到另一盏旁边：应该换灯
    const GNg::sVEC3 camera2 = {0.0f, 1.0f, 18.0f};
    const sCUBE_SHADOW_PICK pick2 = pickCubeShadowLight(light, camera2);
    CHECK_EQ(pick2.aLightIndex, 1);
}

TEST_CASE(lightPickSkipsNonShadowLights) {
    GN3d::sLIGHT_UBO light = makeEmpty();
    addPoint(light, 0.0f, 1.0f, 0.0f, 2.0f, 0.0f);   // index 0：**没开阴影**
    addPoint(light, 0.0f, 1.0f, 20.0f, 2.0f, 2.0f);  // index 1：开了阴影（但远）
    const GNg::sVEC3 camera = {0.0f, 1.0f, 5.0f};

    // 近的那盏没有 cube shadow，只能选远的
    const sCUBE_SHADOW_PICK pick = pickCubeShadowLight(light, camera);
    CHECK_EQ(pick.aLightIndex, 1);

    // shadowType = 1（方向光的 2D 级联）也不算候选
    GN3d::sLIGHT_UBO light2 = makeEmpty();
    addPoint(light2, 0.0f, 1.0f, 0.0f, 2.0f, 1.0f);
    CHECK_EQ(pickCubeShadowLight(light2, camera).aLightIndex, -1);

    // 方向光（position.w < 0.5）不参与
    GN3d::sLIGHT_UBO light3 = makeEmpty();
    light3.lights[0].position[3] = 0.0f; // DIRECTIONAL
    light3.lights[0].direction[3] = 2.0f; // 声称有 cube shadow（数据异常）
    light3.lightCount = 1.0f;
    CHECK_EQ(pickCubeShadowLight(light3, camera).aLightIndex, -1);
}

TEST_CASE(lightPickRespectsMaxDistance) {
    GN3d::sLIGHT_UBO light = makeEmpty();
    addPoint(light, 0.0f, 1.0f, 100.0f, 2.0f, 2.0f); // 离相机 ~95
    const GNg::sVEC3 camera = {0.0f, 1.0f, 5.0f};

    // 默认上限 40 m：太远，不选
    CHECK_EQ(pickCubeShadowLight(light, camera).aLightIndex, -1);
    // 放宽到 200 m：可选
    CHECK_EQ(pickCubeShadowLight(light, camera, 200.0f).aLightIndex, 0);
    // 非法上限（<=0）回落到默认 40
    CHECK_EQ(pickCubeShadowLight(light, camera, 0.0f).aLightIndex, -1);
    CHECK_EQ(pickCubeShadowLight(light, camera, -5.0f).aLightIndex, -1);
}

TEST_CASE(lightPickPrefersBrighterOnTie) {
    GN3d::sLIGHT_UBO light = makeEmpty();
    // 同一位置、不同强度：亮的赢
    addPoint(light, 0.0f, 1.0f, 0.0f, 1.0f, 2.0f);
    addPoint(light, 0.0f, 1.0f, 0.0f, 9.0f, 2.0f);
    const sCUBE_SHADOW_PICK pick = pickCubeShadowLight(light, GNg::sVEC3{0.0f, 1.0f, 5.0f});
    CHECK_EQ(pick.aLightIndex, 1);
}

TEST_CASE(lightPickEmptyAndDegenerate) {
    const GNg::sVEC3 camera = {0.0f, 0.0f, 0.0f};
    // 没有光源
    GN3d::sLIGHT_UBO empty = makeEmpty();
    CHECK_EQ(pickCubeShadowLight(empty, camera).aLightIndex, -1);

    // lightCount 越界（数据损坏）：不选，也不读脏内存
    GN3d::sLIGHT_UBO broken = makeEmpty();
    broken.lightCount = 999.0f;
    CHECK_EQ(pickCubeShadowLight(broken, camera).aLightIndex, -1);

    // 强度为 0 的光：score 为 0，不入选（不能因为"只有它一个"就选它 ——
    // 一盏不发光的光源投出的阴影毫无意义，还会白白花掉 6 遍场景渲染）
    GN3d::sLIGHT_UBO dark = makeEmpty();
    addPoint(dark, 0.0f, 1.0f, 0.0f, 0.0f, 2.0f);
    CHECK_EQ(pickCubeShadowLight(dark, camera).aLightIndex, -1);
}
