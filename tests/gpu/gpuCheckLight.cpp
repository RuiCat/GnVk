// gpuCheckLight.cpp — 光照系统端到端接入验收。
//
// 思路：同一块正对相机、法线 +Z 的全屏白 quad，只改 sLIGHT_UBO 的字段渲染多次，
// 用中心像素亮度做**相对**断言（不依赖 IBL 底噪绝对值）：
//   * 关掉所有光源 → 基线（只剩天空盒 IBL 兜底）
//   * 正对法线的方向光 → 明显比基线亮
//   * 强度翻倍 → 更亮（证明 color.a 被当作强度读到了）
//   * 背对法线的方向光 → 回到基线（证明 N·L 真的参与计算，不是无脑加亮）
//   * 点光源近/远（1/d² 衰减）→ 近的明显亮
//   * 只给环境光 → 比基线亮
//
// 之所以用相对断言：PBR 的「GI 倍率=0」分支仍会叠一层 skybox IBL，绝对亮度含底噪；
// 但这些底噪在每组对比里是同一个值，做差就能干净地反映光源贡献。
#include "gpuCheckLight.h"
#include "gpuTestHooks.h"

#include <GN.h>
#include "GN_Vk.h"
#include "giParams.h"
#include "postParams.h"

#include "gpuCheckReport.h"
#include "gpuPixelRead.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

namespace GpuCheck {
namespace {

const uint32_t kQuadIndexBA[6] = {0, 2, 1, 0, 3, 2}; // 正面朝向（与 3D 站点同绕序）
const float kSunDirection[3] = {0.0f, 0.0f, -1.0f};  // 射向 -Z：法线 +Z 的正面被正照

struct sQUAD {
    GNg::VkVertex3D verts[4];
};

void MakeQuad(sQUAD &quad) {
    std::memset(&quad, 0, sizeof(quad));
    const float xs[4] = {-1.0f, 1.0f, 1.0f, -1.0f};
    const float ys[4] = {-1.0f, -1.0f, 1.0f, 1.0f};
    for (int i = 0; i < 4; ++i) {
        quad.verts[i].px = xs[i];
        quad.verts[i].py = ys[i];
        quad.verts[i].pz = 0.5f;
        quad.verts[i].nz = 1.0f; // 法线朝 +Z（朝相机）
        quad.verts[i].tx = 1.0f; // 零切线在顶点着色器里 normalize 会得 NaN
        quad.verts[i].u = (i == 1 || i == 2) ? 1.0f : 0.0f;
        quad.verts[i].v = (i >= 2) ? 1.0f : 0.0f;
    }
}

// 环境光清零的基线：所有光源都关掉，只留 IBL 兜底。
GN3d::sLIGHT_UBO MakeDarkLight() {
    GN3d::sLIGHT_UBO light;
    std::memset(&light, 0, sizeof(light));
    light.shadowParams[0] = 2048.0f;
    light.shadowParams[1] = 1.0f;
    return light; // ambientLight 与 lightCount 都是 0
}

void SetAmbient(GN3d::sLIGHT_UBO &light, float r, float g, float b, float intensity) {
    light.ambientLight[0] = r;
    light.ambientLight[1] = g;
    light.ambientLight[2] = b;
    light.ambientLight[3] = intensity;
}

// 追加一个方向光（position.w=0）；direction 为光传播方向。
void AddSun(GN3d::sLIGHT_UBO &light, const float dir[3], float intensity, float shadowType) {
    const int idx = (int)light.lightCount;
    if (idx >= GN_MAX_LIGHTS) return;
    GN3d::sLIGHT_GPU &L = light.lights[idx];
    L.color[0] = 1.0f;
    L.color[1] = 1.0f;
    L.color[2] = 1.0f;
    L.color[3] = intensity;
    L.position[3] = (float)GN3d::eLIGHT_DIRECTIONAL;
    L.direction[0] = dir[0];
    L.direction[1] = dir[1];
    L.direction[2] = dir[2];
    L.direction[3] = shadowType;
    L.attenuation[0] = 1.0f;
    light.lightCount = (float)(idx + 1);
}

// 追加一个点光源；atten = (constant, linear, quadratic)，range=0 表示不限距。
void AddPoint(GN3d::sLIGHT_UBO &light, float z, float intensity,
              float constant, float linear, float quadratic, float range) {
    const int idx = (int)light.lightCount;
    if (idx >= GN_MAX_LIGHTS) return;
    GN3d::sLIGHT_GPU &L = light.lights[idx];
    L.color[0] = 1.0f;
    L.color[1] = 1.0f;
    L.color[2] = 1.0f;
    L.color[3] = intensity;
    L.position[0] = 0.0f;
    L.position[1] = 0.0f;
    L.position[2] = z;
    L.position[3] = (float)GN3d::eLIGHT_POINT;
    L.attenuation[0] = constant;
    L.attenuation[1] = linear;
    L.attenuation[2] = quadratic;
    L.attenuation[3] = range;
    light.lightCount = (float)(idx + 1);
}

float Luminance(const sFLOAT_PIXEL &p) {
    return 0.2126f * p.r + 0.7152f * p.g + 0.0722f * p.b;
}

// 用给的光照渲染一次白 quad，返回中心像素线性亮度；失败返回 -1。
float RenderLuminance(const GN3d::sLIGHT_UBO &light, const GNg::sMAT44 &identity,
                      const GNg::sVEC3 &eye, const sQUAD &quad, const GNg::sMAT44 &model) {
    if (!GNg::GN_VkBeginFrame()) return -1.0f;
    GNg::GN_VkBegin3DPassPBR(&identity, &eye, &light, sizeof(light));
    // 纯白、粗糙、非金属、AO 全开：让中心像素亮度主要由光源决定。
    GNg::GN_VkDraw3DPBR(quad.verts, 4, kQuadIndexBA, 6, &model,
                        1.0f, 1.0f, 1.0f, 0.9f, 0.0f, 1.0f);
    GNg::GN_VkEnd3DPass();
    GNg::GN_VkEndFrame();
    vkQueueWaitIdle(Queue());

    const int width = (int)Extent().width;
    const int height = (int)Extent().height;
    std::vector<uint8_t> pixels;
    if (!ReadSwapchain(0, 0, width, height, pixels)) return -1.0f;
    return Luminance(SampleSwapchain(pixels, width, width / 2, height / 2));
}

// 对比实际值与参考值，输出统一格式的断言结果。
void ReportCompare(const char *name, float actual, float expectedRef, bool ok) {
    char detail[192];
    std::snprintf(detail, sizeof(detail), "lum=%.4f %s %.4f", actual, ok ? ">" : "<=",
                  expectedRef);
    if (ok) ReportPass(name, detail);
    else ReportFail(name, detail);
}

} // namespace

void RunLightChecks() {
    if (Pipe(GNg::eTEST_PIPE_3D_PBR) == VK_NULL_HANDLE) {
        ReportSkip("light.chain", "pipelinePBR missing (see pipe.3d.pbr)");
        return;
    }

    GNg::sMAT44 identity;
    std::memset(&identity, 0, sizeof(identity));
    identity.m[0] = identity.m[5] = identity.m[10] = identity.m[15] = 1.0f;
    GNg::sMAT44 model = identity;
    const GNg::sVEC3 eye = {0.0f, 0.0f, 1.0f};

    sQUAD quad;
    MakeQuad(quad);

    // 基线：无光源、无环境光（只剩 IBL 兜底）
    const float baseline = RenderLuminance(MakeDarkLight(), identity, eye, quad, model);
    if (baseline < 0.0f) {
        ReportFail("light.chain", "基线渲染/回读失败");
        return;
    }
    {
        char detail[96];
        std::snprintf(detail, sizeof(detail), "IBL 兜底 lum=%.4f（作为后续相对断言的基准）",
                      baseline);
        ReportPass("light.baseline", detail);
    }

    // 1) 方向光正照法线 → 明显更亮
    GN3d::sLIGHT_UBO sun = MakeDarkLight();
    AddSun(sun, kSunDirection, 1.0f, 0.0f); // shadowType=0：本站点不测阴影
    const float lumSun = RenderLuminance(sun, identity, eye, quad, model);
    ReportCompare("light.sun", lumSun, baseline, lumSun > baseline + 0.02f);

    // 2) 强度翻倍 → 更亮（证明 color.a 被当作强度读入）
    GN3d::sLIGHT_UBO sun2 = MakeDarkLight();
    AddSun(sun2, kSunDirection, 2.0f, 0.0f);
    const float lumSun2 = RenderLuminance(sun2, identity, eye, quad, model);
    ReportCompare("light.intensity", lumSun2, lumSun, lumSun2 > lumSun + 0.02f);

    // 3) 背对法线的方向光 → 直射项为 0，回到基线（证明 N·L 真在算）
    const float awaySun[3] = {0.0f, 0.0f, 1.0f};
    GN3d::sLIGHT_UBO back = MakeDarkLight();
    AddSun(back, awaySun, 1.0f, 0.0f);
    const float lumBack = RenderLuminance(back, identity, eye, quad, model);
    {
        const bool ok = std::fabs(lumBack - baseline) <= 0.02f;
        char detail[160];
        std::snprintf(detail, sizeof(detail), "背光 lum=%.4f ≈ 基线 %.4f（N·L=0 时直射项应消失）",
                      lumBack, baseline);
        if (ok) ReportPass("light.backlit", detail);
        else ReportFail("light.backlit", detail);
    }

    // 4) 点光源 1/d² 衰减：近处（d=1）明显出光，远处（d=32，衰减差 1024 倍）应回到基线。
    //    不用「两点亮度比值」做断言：合成阶段有 ACES 色调映射 + sRGB 编码，高亮区被
    //    强烈压缩，2~3 倍的亮度差在像素上只剩一点点。用「远处回到基线」这个绝对判据更稳。
    GN3d::sLIGHT_UBO pointNear = MakeDarkLight();
    AddPoint(pointNear, 1.0f, 40.0f, 0.0f, 0.0f, 1.0f, 0.0f); // att = 1/d²
    const float lumNear = RenderLuminance(pointNear, identity, eye, quad, model);

    GN3d::sLIGHT_UBO pointFar = MakeDarkLight();
    AddPoint(pointFar, 32.0f, 40.0f, 0.0f, 0.0f, 1.0f, 0.0f); // att = 40/1024 ≈ 0.04
    const float lumFar = RenderLuminance(pointFar, identity, eye, quad, model);

    {
        char detail[224];
        std::snprintf(detail, sizeof(detail),
                      "d=1 lum=%.4f（基线%.4f）; d=32 lum=%.4f（衰减 1024 倍后应回到基线）",
                      lumNear, baseline, lumFar);
        const bool ok = (lumNear > baseline + 0.20f) && (std::fabs(lumFar - baseline) <= 0.05f);
        if (ok) ReportPass("light.pointAttenuation", detail);
        else ReportFail("light.pointAttenuation", detail);
    }

    // 5) 点光源 range 早退：超出 range 应完全不出光（等于基线）
    GN3d::sLIGHT_UBO pointOutOfRange = MakeDarkLight();
    AddPoint(pointOutOfRange, 10.0f, 40.0f, 0.0f, 0.0f, 1.0f, 2.0f); // range=2 < d=10
    const float lumOut = RenderLuminance(pointOutOfRange, identity, eye, quad, model);
    {
        const bool ok = std::fabs(lumOut - baseline) <= 0.02f;
        char detail[176];
        std::snprintf(detail, sizeof(detail), "超距 lum=%.4f ≈ 基线 %.4f（attenuation.w 早退）",
                      lumOut, baseline);
        if (ok) ReportPass("light.pointRange", detail);
        else ReportFail("light.pointRange", detail);
    }

    // 6) 只给环境光（无光源）→ 比基线亮（证明 ambientLight 通道也接上了）
    GN3d::sLIGHT_UBO ambientOnly = MakeDarkLight();
    SetAmbient(ambientOnly, 1.0f, 1.0f, 1.0f, 1.0f);
    const float lumAmbient = RenderLuminance(ambientOnly, identity, eye, quad, model);
    ReportCompare("light.ambient", lumAmbient, baseline, lumAmbient > baseline + 0.02f);
}

} // namespace GpuCheck
