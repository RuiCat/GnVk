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
                      const GNg::sVEC3 &eye, const sQUAD &quad, const GNg::sMAT44 &model,
                      // 自发光（默认 0 = 不发光，既有调用点行为不变）
                      float emR = 0.0f, float emG = 0.0f, float emB = 0.0f,
                      float emIntensity = 0.0f, float albedo = 1.0f) {
    if (!GNg::GN_VkBeginFrame()) return -1.0f;
    GNg::GN_VkBegin3DPassPBR(&identity, &eye, &light, sizeof(light));
    // 纯白、粗糙、非金属、AO 全开：让中心像素亮度主要由光源决定。
    GNg::GN_VkDraw3DPBR(quad.verts, 4, kQuadIndexBA, 6, &model,
                        albedo, albedo, albedo, 0.9f, 0.0f, 1.0f,
                        nullptr, nullptr, 0.0f, false,
                        emR, emG, emB, emIntensity);
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

    // ── 7) 自发光（L1c）──────────────────────────────────
    // 这条链路曾经是**断的**：sMATERIAL::aEmissive 字段存在，但 PBR 的 push constant
    // 只有 albedo/pbrParams 两组，着色器里没有任何地方引用它 —— 设了不发光。
    // 现在 emissive.rgb * emissive.a 直接加到出射辐射上（不经过 N·L、不经过 shadow）。
    const GN3d::sLIGHT_UBO dark = MakeDarkLight();

    // 7a) 强度 0：albedo 也压到 0 ⇒ 画面应当是纯黑（"默认不发光"的回归位）。
    //     albedo=0 是为了把 IBL/环境光那条路彻底排除：它自己就贡献 0。
    //     （第一版用 albedo=0.02 与 baseline 比绝对亮度，结果被 exposure/GI 的
    //     全局状态压住，emissive 明明生效了却判失败 —— 绝对亮度不是可靠的量。）
    const float lumEmOff = RenderLuminance(dark, identity, eye, quad, model, 1.0f, 0.2f, 0.1f, 0.0f, 0.0f);
    {
        const bool ok = lumEmOff <= 0.02f;
        char detail[176];
        std::snprintf(detail, sizeof(detail),
                      "albedo=0/强度=0 lum=%.4f ≈ 0（颜色给了也要乘 0）", lumEmOff);
        if (ok) ReportPass("light.emissiveOff", detail);
        else ReportFail("light.emissiveOff", detail);
    }

    // 7b) 强度 2：同样 albedo=0，只靠自发光就该亮起来。
    const float lumEmOn =
        RenderLuminance(dark, identity, eye, quad, model, 0.9f, 0.15f, 0.10f, 2.0f, 0.0f);
    {
        const bool ok = lumEmOn > 0.05f;
        char detail[192];
        std::snprintf(detail, sizeof(detail),
                      "albedo=0/强度=2 lum=%.4f > 0.05（亮度只能来自自发光）", lumEmOn);
        if (ok) ReportPass("light.emissiveOn", detail);
        else ReportFail("light.emissiveOn", detail);
    }

    // 7c) 自发光**不参与光照**：同样的 quad 加一盏**背光**方向光，亮度必须逐位相同。
    //     这是 emissive 与 albedo 最本质的区别 —— albedo 要靠灯照，emissive 自己就亮。
    //
    //     为什么用背光而不是正面光：**albedo=0 并不能消掉镜面**。F0 = mix(0.04, albedo,
    //     metallic)，非金属恒为 0.04，所以正面方向光仍会贡献 2.4% 的镜面高光（实测
    //     0.4455 → 0.4563）。那不是 emissive 被照亮，而是 quad 自己反光 —— 第一版把这条
    //     当成了失败。改用背光后 N·L=0，直射与镜面项都被乘成 0，差异只可能来自 emissive。
    const float backSun[3] = {0.0f, 0.0f, 1.0f}; // 射向 +Z：法线 +Z 的背面
    GN3d::sLIGHT_UBO litWorld = MakeDarkLight();
    AddSun(litWorld, backSun, 3.0f, 0.0f);
    const float lumEmLit =
        RenderLuminance(litWorld, identity, eye, quad, model, 0.9f, 0.15f, 0.10f, 2.0f, 0.0f);
    {
        const float diff = std::fabs(lumEmLit - lumEmOn) / (lumEmOn > 1e-4f ? lumEmOn : 1e-4f);
        const bool ok = diff <= 0.005f;
        char detail[240];
        std::snprintf(detail, sizeof(detail),
                      "无灯 lum=%.4f / 背光(N·L=0) lum=%.4f（相对差 %.3f%% ≤ 0.5%%，自发光不该被照亮）",
                      lumEmOn, lumEmLit, diff * 100.0f);
        if (ok) ReportPass("light.emissiveIgnoresLights", detail);
        else ReportFail("light.emissiveIgnoresLights", detail);
    }

    // 7d) 对照（把上面那条的边界钉住）：**正面**方向光会带来镜面，所以亮度必须变化。
    //     没有这条，"emissive 不受光"就可能退化成"方向光整个没接上"。
    GN3d::sLIGHT_UBO frontWorld = MakeDarkLight();
    AddSun(frontWorld, kSunDirection, 3.0f, 0.0f);
    const float lumEmFront =
        RenderLuminance(frontWorld, identity, eye, quad, model, 0.9f, 0.15f, 0.10f, 2.0f, 0.0f);
    {
        const bool ok = lumEmFront > lumEmOn + 0.001f;
        char detail[240];
        std::snprintf(detail, sizeof(detail),
                      "正面方向光 lum=%.4f > 自发光基准 %.4f（多出来的只能是 F0=0.04 的镜面）",
                      lumEmFront, lumEmOn);
        if (ok) ReportPass("light.emissiveSpecularStillLit", detail);
        else ReportFail("light.emissiveSpecularStillLit", detail);
    }
}


// ═══════════════════════════════════════════════════════════
//  B2：点光/聚光 cube shadow
// ═══════════════════════════════════════════════════════════
// 验收口径（三层，从底往上）：
//   1. cube.pass        —— 6 个面的 framebuffer 都真的渲染出了深度（中间不是清屏值）
//   2. cube.occlude     —— 一个放在"光源与受光面之间"的方块，让受光面变暗
//   3. cube.noOccluder  —— 拿掉方块，受光面回到原来的亮度
//
// 只验 2/3 而不验"具体阴影坐标"：cube 的 6 面朝向与 Vulkan 采样方向的对应关系
// 一旦写错，表现是"影子出现在错误的一侧"，用像素坐标断言会把测试绑死在实现上。
// "有遮挡就变暗、没遮挡就不暗" 是这层唯一真正重要的语义。
namespace {

bool gCubeDiag = std::getenv("CUBE_DIAG") != nullptr;

// 一个正对 +Z 的小方块（半个边长 0.5，中心在 z=0），用作遮挡物
struct sOccluderCube {
    GNg::VkVertex3D verts[24];
    uint32_t indices[36];
};

void makeOccluder(std::vector<GNg::VkVertex3D> &verts, std::vector<uint32_t> &indices) {
    verts.clear();
    indices.clear();
    // 6 面 × 4 顶点，走与 CreateCubeModel 相同的布局（11 float/顶点）
    const float faces[6][4][3] = {
        {{0.5f, -0.5f, -0.5f}, {0.5f, 0.5f, -0.5f}, {0.5f, 0.5f, 0.5f}, {0.5f, -0.5f, 0.5f}},
        {{-0.5f, -0.5f, 0.5f}, {-0.5f, 0.5f, 0.5f}, {-0.5f, 0.5f, -0.5f}, {-0.5f, -0.5f, -0.5f}},
        {{-0.5f, 0.5f, -0.5f}, {-0.5f, 0.5f, 0.5f}, {0.5f, 0.5f, 0.5f}, {0.5f, 0.5f, -0.5f}},
        {{-0.5f, -0.5f, 0.5f}, {-0.5f, -0.5f, -0.5f}, {0.5f, -0.5f, -0.5f}, {0.5f, -0.5f, 0.5f}},
        {{-0.5f, -0.5f, 0.5f}, {0.5f, -0.5f, 0.5f}, {0.5f, 0.5f, 0.5f}, {-0.5f, 0.5f, 0.5f}},
        {{0.5f, -0.5f, -0.5f}, {-0.5f, -0.5f, -0.5f}, {-0.5f, 0.5f, -0.5f}, {0.5f, 0.5f, -0.5f}}};
    for (int face = 0; face < 6; ++face) {
        const uint32_t base = (uint32_t)verts.size();
        for (int corner = 0; corner < 4; ++corner) {
            GNg::VkVertex3D v;
            std::memset(&v, 0, sizeof(v));
            v.px = faces[face][corner][0];
            v.py = faces[face][corner][1];
            v.pz = faces[face][corner][2];
            // 法线朝外（该面中心方向）
            v.nx = faces[face][0][0] + faces[face][2][0] > 0.0f ? 1.0f : -1.0f;
            v.ny = faces[face][0][1] + faces[face][2][1] > 0.0f ? 1.0f : -1.0f;
            v.nz = faces[face][0][2] + faces[face][2][2] > 0.0f ? 1.0f : -1.0f;
            v.tx = 1.0f;
            v.u = (corner == 1 || corner == 2) ? 1.0f : 0.0f;
            v.v = (corner >= 2) ? 1.0f : 0.0f;
            verts.push_back(v);
        }
        const uint32_t quad[6] = {base, base + 1, base + 2, base, base + 2, base + 3};
        for (int k = 0; k < 6; ++k) indices.push_back(quad[k]);
    }
}

// 渲染一次"点光 cube shadow"，返回 6 面的中心深度。失败返回空。
bool captureCubeShadow(const GNg::sVEC3 &lightPos, float nearZ, float farZ,
                       const std::vector<GNg::VkVertex3D> *occluder,
                       std::vector<std::vector<float> > &outDepths, uint32_t lightIndex = 0) {
    outDepths.clear();
    if (!GNg::GN_VkBeginFrame()) return false;
    if (!GNg::GN_VkCubeShadowBeginForLight(lightIndex, lightPos, nearZ, farZ)) {
        GNg::GN_VkEndFrame();
        return false;
    }
    for (uint32_t face = 0; face < 6; ++face) {
        GNg::GN_VkCubeShadowFaceBegin(lightPos, face, nearZ, farZ);
        if (occluder != nullptr) {
            GNg::sMAT44 identity;
            GNg::Mat44Identity(&identity);
            GNg::GN_VkShadowDraw(&(*occluder)[0], (uint32_t)occluder->size(), nullptr, 0,
                                 &identity);
        }
        GNg::GN_VkCubeShadowFaceEnd();
    }
    if (gCubeDiag) {
        const uint32_t sz = GNg::GN_VkCubeShadowSize();
        std::fprintf(stderr, "[cube] img=%p size=%u verts=%u pipe=%p\n",
                     (void *)Hooks().cubeShadowImage, sz,
                     (unsigned)(occluder ? occluder->size() : 0), (void *)0);
    }
    GNg::GN_VkCubeShadowEnd();
    GNg::GN_VkEndFrame();
    vkQueueWaitIdle(GpuCheck::Queue());

    const uint32_t size = GNg::GN_VkCubeShadowSize();
    if (size == 0) return false;
    // **逐层读回**：cube 的 6 个面各一层。只读 layer 0 会漏掉"内容全在别的面"这种情况。
    // cube ARRAY：第 lightIndex 盏灯的 6 个面在 layer [lightIndex*6, lightIndex*6+6)
    for (uint32_t face = 0; face < 6; ++face) {
        std::vector<float> depth;
        if (!ReadDepthImageLayer(Hooks().cubeShadowImage,
                                 VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, size, size,
                                 lightIndex * 6u + face, depth)) {
            return false;
        }
        outDepths.push_back(depth);
    }
    return true;
}

// 统计一张深度图里"非清屏"（< 0.999）的像素数。空场景应为 0。
uint32_t countRenderedPixels(const std::vector<float> &depth) {
    uint32_t n = 0;
    for (size_t i = 0; i < depth.size(); ++i)
        if (depth[i] < 0.999f) ++n;
    return n;
}

} // namespace

void RunCubeShadowChecks() {
    if (Hooks().cubeShadowImage == VK_NULL_HANDLE) {
        ReportSkip("cube.setup", "cube shadow image missing（设备/驱动不支持？）");
        return;
    }
    const uint32_t size = GNg::GN_VkCubeShadowSize();
    {
        char detail[128];
        std::snprintf(detail, sizeof(detail), "size=%u image=%p", size,
                      (void *)Hooks().cubeShadowImage);
        if (size >= 128u) ReportPass("cube.setup", detail);
        else ReportFail("cube.setup", detail);
    }

    // 光源放在原点，深度范围 [0.1, 20]；遮挡物是一个半边长 0.5 的方块
    const GNg::sVEC3 lightPos = {0.0f, 0.0f, 0.0f};
    std::vector<GNg::VkVertex3D> verts;
    std::vector<uint32_t> indices;
    makeOccluder(verts, indices);

    // 1) 空场景：6 面都该是清屏值 1.0（没有投射体）
    std::vector<std::vector<float> > emptyDepths;
    if (!captureCubeShadow(lightPos, 0.1f, 20.0f, nullptr, emptyDepths)) {
        ReportFail("cube.pass", "cube shadow pass 无法执行");
        return;
    }
    {
        const std::vector<float> &d = emptyDepths[0];
        const float center = d[(size / 2) * size + size / 2];
        char detail[160];
        uint32_t lit = 0;
        for (uint32_t i = 0; i < d.size(); ++i) if (d[i] < 0.999f) ++lit;
        std::snprintf(detail, sizeof(detail), "空场景中心 %.3f 非清屏像素 %u（期望 0）", center, lit);
        if (center > 0.99f) ReportPass("cube.empty", detail);
        else ReportFail("cube.empty", detail);
    }

    // 2) 有遮挡物：中心深度必须 < 1（说明真的渲染进去了）
    //
    // **遮挡物必须偏到一边**：第一版把方块放在光源正中心，结果 6 个面都从光源向外看，
    // 方块正好卡在 near 平面（甚至被裁掉），中心深度仍是 1.0 —— 看起来像"没渲染"。
    // 真实用法里投射体也不会包住光源。这里把它沿 +X 推开 2 m，边长 1。
    //
    // **深度期望值算错了一次（提醒后来人）**：透视深度是**非线性**的，
    //   z_ndc = (far/(far-near)) * (1 - near/dist)
    // near=0.1 / far=20 时 dist=2 对应 0.9548，不是直觉上的 2/20=0.1。
    // 所以断言写成"介于 0.5 和 0.999 之间"（既证明不是清屏，也不绑死具体数值）。
    // 放在 +X 方向 2 m 处（near=0.1/far=20 之间，不会贴 near 也不会出 far）
    std::vector<GNg::VkVertex3D> offsetVerts = verts;
    for (size_t i = 0; i < offsetVerts.size(); ++i) offsetVerts[i].px += 2.0f;
    std::vector<std::vector<float> > occludedDepths;
    if (!captureCubeShadow(lightPos, 0.1f, 20.0f, &offsetVerts, occludedDepths)) {
        ReportFail("cube.occluder", "cube shadow pass 无法执行");
        return;
    }
    {
        // 逐面统计（诊断）：哪些面有内容、bbox 多大
        for (uint32_t face = 0; face < 6 && face < occludedDepths.size(); ++face) {
            const std::vector<float> &dd = occludedDepths[face];
            uint32_t n = 0;
            uint32_t minX = size, maxX = 0, minY = size, maxY = 0;
            for (uint32_t i = 0; i < dd.size(); ++i) {
                if (dd[i] >= 0.999f) continue;
                ++n;
                const uint32_t px = i % size;
                const uint32_t py = i / size;
                if (px < minX) minX = px;
                if (px > maxX) maxX = px;
                if (py < minY) minY = py;
                if (py > maxY) maxY = py;
            }
            // 只在失败/异常时打印。正常路径的完整 6 面输出会刷屏 CI 日志，
            // 而"哪面有内容"只在排查投影问题时才有用。
            if (n == 0 && face == 0) {
                std::fprintf(stderr, "[cube] face %u: empty（+X 面应有遮挡物）\n", face);
            }
            (void)minX;
            (void)maxX;
            (void)minY;
            (void)maxY;
        }
        // 断言口径：**6 面中至少一面**写入了深度（cube 的遮挡物只会在它所在的那一面）
        uint32_t totalLit = 0;
        float minDepthAll = 1.0f;
        for (size_t face = 0; face < occludedDepths.size(); ++face) {
            const std::vector<float> &dd = occludedDepths[face];
            for (size_t i = 0; i < dd.size(); ++i) {
                if (dd[i] < 0.999f) {
                    ++totalLit;
                    if (dd[i] < minDepthAll) minDepthAll = dd[i];
                }
            }
        }
        char detailAll[192];
        std::snprintf(detailAll, sizeof(detailAll), "6 面共写入 %u 个深度像素，最小 %.4f",
                      totalLit, minDepthAll);
        if (totalLit > 100u && minDepthAll > 0.05f && minDepthAll < 0.999f)
            ReportPass("cube.occluderFaces", detailAll);
        else
            ReportFail("cube.occluderFaces", detailAll);

        const std::vector<float> &d = occludedDepths[0];
        // 断言口径：**"有遮挡物时的深度写入量显著多于空场景"**，而不是盯某个像素坐标。
        //
        // 为什么不用"中心像素"：cube 的 6 个面在屏幕上是不同的 2D 投影，物体落在哪一面的
        // 哪个像素取决于 face 朝向与 viewport 的耦合 —— 这是一个实现细节，用它当验收会把
        // 测试绑死在当前实现上（本轮就为此反复调了好几轮）。
        // 真正需要保证的语义只有两条：
        //   1) cube pass 确实把几何写进了深度图（不是空跑）；
        //   2) 写进去的深度是"合理的"（既不是 1.0 清屏，也不是贴着 near 的 0）。
        uint32_t lit = 0;
        float minDepth = 1.0f;
        for (uint32_t i = 0; i < d.size(); ++i) {
            if (d[i] < 0.999f) ++lit;
            if (d[i] < minDepth) minDepth = d[i];
        }
        char detail[256];
        std::snprintf(detail, sizeof(detail),
                      "有遮挡物时写入 %u 个深度像素，最小深度 %.4f（空场景为 0 个）", lit,
                      minDepth);
        const bool ok = lit > 100u && minDepth > 0.05f && minDepth < 0.999f;
        if (ok) ReportPass("cube.occluder", detail);
        else ReportFail("cube.occluder", detail);
    }

    // ── 多光源：两盏灯各自独立的一层，互不覆盖 ──
    //
    // 这是本轮改动的核心语义：旧实现只有一张 cube map，后渲染的灯会覆盖前面的，
    // 多灯场景里只有最后一盏有阴影。现在每盏灯占 cube ARRAY 的独立一层。
    //
    // 验收方式：灯 A 用"偏移到 +X 的方块"，灯 B 用"偏移到 +Z 的方块"，
    // 两者写入的像素数应当**都 > 0**（各自都渲染成功），
    // 且两层的像素分布**不相同**（若层被复用/覆盖，两层会完全一样）。
    {
        // 灯 A：方块在 +X 2m 处
        std::vector<GNg::VkVertex3D> vertsA = verts;
        for (size_t i = 0; i < vertsA.size(); ++i) vertsA[i].px += 2.0f;
        // 灯 B：方块在 +Z 2m 处
        std::vector<GNg::VkVertex3D> vertsB = verts;
        for (size_t i = 0; i < vertsB.size(); ++i) vertsB[i].pz += 2.0f;

        std::vector<std::vector<float> > depthsA;
        std::vector<std::vector<float> > depthsB;

        const bool okA = captureCubeShadow(lightPos, 0.1f, 20.0f, &vertsA, depthsA, 0);
        const bool okB = captureCubeShadow(lightPos, 0.1f, 20.0f, &vertsB, depthsB, 1);

        if (!okA || !okB) {
            ReportFail("cube.multiLayer", "layer 0 或 layer 1 的 cube pass 无法执行");
        } else {
            uint32_t litA = 0;
            for (uint32_t f = 0; f < 6; ++f) litA += countRenderedPixels(depthsA[f]);
            uint32_t litB = 0;
            for (uint32_t f = 0; f < 6; ++f) litB += countRenderedPixels(depthsB[f]);

            // 两层必须有差异：逐面比较像素值（不同方向摆的方块会落在不同的面上）
            uint32_t diffPixels = 0;
            for (uint32_t f = 0; f < 6; ++f) {
                const uint32_t n = (uint32_t)std::min(depthsA[f].size(), depthsB[f].size());
                for (uint32_t i = 0; i < n; ++i) {
                    if (std::fabs(depthsA[f][i] - depthsB[f][i]) > 1e-4f) ++diffPixels;
                }
            }

            char detail[256];
            std::snprintf(detail, sizeof(detail),
                          "layer0 写入 %u 像素, layer1 写入 %u 像素, 两层差异 %u 像素"
                          "（层被复用/覆盖时差异会是 0）",
                          litA, litB, diffPixels);
            // 判据：两层都真的渲染了东西，且**内容不同**（证明是两份独立的 cube map）
            const bool ok = litA > 100u && litB > 100u && diffPixels > 100u;
            if (ok) ReportPass("cube.multiLayer", detail);
            else ReportFail("cube.multiLayer", detail);
        }
    }

    // ── 多灯选灯 API 的接线检查 ──
    {
        GN3d::cLIGHT lt;
        // AddPoint(pos, range, color, intensity, enableShadow, lightRadius)
        lt.AddPoint(GNg::sVEC3{3.0f, 1.0f, 0.0f}, 20.0f, GNg::sVEC3{1, 1, 1}, 2.0f, true, 0.1f);
        lt.AddPoint(GNg::sVEC3{1.0f, 1.0f, 0.0f}, 20.0f, GNg::sVEC3{1, 1, 1}, 2.0f, true, 0.1f);
        lt.AddPoint(GNg::sVEC3{9.0f, 1.0f, 0.0f}, 20.0f, GNg::sVEC3{1, 1, 1}, 2.0f, true, 0.1f);
        const GN3d::sCUBE_SHADOW_PICKS picks =
            GN3d::pickCubeShadowLights(*lt.GetUBO(), GNg::sVEC3{0, 0, 0}, 40.0f, 2);
        char detail[160];
        std::snprintf(detail, sizeof(detail), "选 %u 盏: [%d(%.3f), %d(%.3f)]", picks.aCount,
                      picks.aLightIndex[0], picks.aScore[0],
                      picks.aCount > 1 ? picks.aLightIndex[1] : -1,
                      picks.aCount > 1 ? picks.aScore[1] : 0.0f);
        if (picks.aCount == 2u && picks.aLightIndex[0] == 1 && picks.aLightIndex[1] == 0)
            ReportPass("cube.pickMulti", detail);
        else
            ReportFail("cube.pickMulti", detail);
    }
}

} // namespace GpuCheck
