#pragma once
#ifndef GN_GI_PARAMS_H
#define GN_GI_PARAMS_H
// ╔══════════════════════════════════════════════════════════╗
// ║  GN_mini — GI CPU 基座（纯 CPU，可单测）                  ║
// ╚══════════════════════════════════════════════════════════╝
//
// 职责：IBL/SSAO 的 CPU 侧预计算与数据契约——GI UBO 默认值/校验/打包、
// SSAO 16 点半球核（Halton + 随机切向）、SH9 环境积分与求值、BRDF LUT 生成、
// G-Buffer 打包/解包（albedo sRGB、AO/材质位域、octahedral 法线、alpha cutoff）。
// 本层不碰 Vulkan、不分配 GPU 资源、不依赖引擎数学类型；输入输出都用 float 数组，
// 布局与 docs/GI_PLAN.md §4.3/§5.2/§5.4 的 shader 契约逐字段一致。
//
// 类型名 sGI_UBO 沿用 docs/GI_PLAN.md §4.3 的冻结契约名（UBO 布局是跨 CPU/GLSL
// 的外部契约）；其余标识符遵循 camelCase / PascalCase / UPPER_SNAKE。
#include <cstddef>
#include <cstdint>

namespace GN3d {

// SSAO 半球核样本数（§4.2：CPU 生成 16 点半球）。
constexpr int SSAO_KERNEL_SIZE = 16;
// SH9 系数个数与存储 float 数（每个系数 4 float，rgb + padding）。
constexpr int SH9_COEFF_COUNT = 9;
constexpr int SH9_FLOAT_COUNT = 36;
// BRDF LUT 默认边长与每 texel 的 GGX 重要性采样数（§4.1）。
constexpr int BRDF_LUT_SIZE = 128;
constexpr int BRDF_LUT_SAMPLES = 64;

// 与 assets/shaders 里的 GIUBO 逐字段一致；sizeof = 256 B（§4.3）。
struct sGI_UBO {
    float indirect[4];    // x=漫反射倍率 y=镜面倍率 z=AO 强度 w=SSAO 开关(0/1)
    float aoParams[4];    // x=半径 y=偏移 z=幂 w=模糊半径
    float envParams[4];   // x=环境旋转 y=预滤波 mip 数 z=曝光补偿 w=保留
    float sh[36];         // SH9，每系数 4 float（144 B）
    float invViewProj[16]; // 深度 → 世界位置（64 B），替代每像素 inverse(vp)
};

static_assert(sizeof(sGI_UBO) == 256, "sGI_UBO 必须与 shader 契约一致（256 B）");
static_assert(offsetof(sGI_UBO, indirect) == 0, "sGI_UBO.indirect 偏移错误");
static_assert(offsetof(sGI_UBO, aoParams) == 16, "sGI_UBO.aoParams 偏移错误");
static_assert(offsetof(sGI_UBO, envParams) == 32, "sGI_UBO.envParams 偏移错误");
static_assert(offsetof(sGI_UBO, sh) == 48, "sGI_UBO.sh 偏移错误");
static_assert(offsetof(sGI_UBO, invViewProj) == 192, "sGI_UBO.invViewProj 偏移错误");

// 默认 GI 参数：倍率 1、SSAO 关（§4.3）、AO 半径 0.5 m（§4.2）、
// 预滤波 5 级 mip；SH9 为常数环境 radiance=1（evalSh9 返回 π），invViewProj=单位阵。
sGI_UBO makeDefaultGiUbo();

// 校验字段合法性；非法输入返回 false，并把可用的修正结果写入 corrected（可为 nullptr）。
// 判据：全部 float 有限；倍率/AO 半径/偏移/模糊半径/mip 数非负；AO 幂 > 0；
// SSAO 开关只接受 0/1。修正策略为逐字段钳制/置默认，不修改未非法字段。
bool validateGiUbo(const sGI_UBO& ubo, sGI_UBO* corrected = nullptr);

// 把 256 B 的 sGI_UBO 原样打包到调用方缓冲（用于 staging buffer 上传）；空指针返回 false。
bool packGiUbo(const sGI_UBO& ubo, void* outBytes);

// ── G2b：SSAO UBO 冻结契约（400 B；docs/GI_PLAN.md §4.2.1）────────────────
// 偏移：viewProj@0(64) / invViewProj@64(64) / samples[16]@128(256) / params@384(16)。
// 位置与法线均为世界空间；invViewProj 由调用方用 Mat44Inverse 求逆后写入。
struct sSSAO_UBO {
    float viewProj[16];
    float invViewProj[16];
    float samples[SSAO_KERNEL_SIZE][4]; // xyz=半球样本方向，w=padding
    float params[4];                    // x=半径(m) y=偏置 z=幂 w=启用位(0=关)
};

static_assert(sizeof(sSSAO_UBO) == 400, "sSSAO_UBO 必须与 shader 契约一致（400 B）");
static_assert(offsetof(sSSAO_UBO, viewProj) == 0, "sSSAO_UBO.viewProj 偏移错误");
static_assert(offsetof(sSSAO_UBO, invViewProj) == 64, "sSSAO_UBO.invViewProj 偏移错误");
static_assert(offsetof(sSSAO_UBO, samples) == 128, "sSSAO_UBO.samples 偏移错误");
static_assert(offsetof(sSSAO_UBO, params) == 384, "sSSAO_UBO.params 偏移错误");

// 默认 SSAO 参数：viewProj/invViewProj=单位阵、16 点核（seed=0）、
// 半径 0.5 m / 偏置 0.025 / 幂 1.0 / 启用位 0（默认关，观感不变）。
sSSAO_UBO makeDefaultSsaoUbo();

// 把 400 B 的 sSSAO_UBO 原样打包到调用方缓冲；空指针返回 false。
bool packSsaoUbo(const sSSAO_UBO& ubo, void* outBytes);

// ── G4：SSGI UBO 冻结契约（160 B；docs/GI_PLAN.md §6）──────────────────
// 偏移：viewProj@0(64) / invViewProj@64(64) / params@128(16) / screen@144(16)。
// params：x=强度(0=关) y=步数 z=厚度 w=最大距离；screen：x=1/宽 y=1/高 z=半宽 w=半高。
struct sSSGI_UBO {
    float viewProj[16];
    float invViewProj[16];
    float params[4];
    float screen[4];
};

static_assert(sizeof(sSSGI_UBO) == 160, "sSSGI_UBO 必须与 shader 契约一致（160 B）");
static_assert(offsetof(sSSGI_UBO, viewProj) == 0, "sSSGI_UBO.viewProj 偏移错误");
static_assert(offsetof(sSSGI_UBO, invViewProj) == 64, "sSSGI_UBO.invViewProj 偏移错误");
static_assert(offsetof(sSSGI_UBO, params) == 128, "sSSGI_UBO.params 偏移错误");
static_assert(offsetof(sSSGI_UBO, screen) == 144, "sSSGI_UBO.screen 偏移错误");

// 默认 SSGI 参数：viewProj/invViewProj=单位阵，强度 0（默认关）、8 步、厚度 0.5、距离 20；
// screen 由调用方每帧填（默认 0 表示未填，shader 侧不会因此开启）。
sSSGI_UBO makeDefaultSsgiUbo();

// 把 160 B 的 sSSGI_UBO 原样打包到调用方缓冲；空指针返回 false。
bool packSsgiUbo(const sSSGI_UBO& ubo, void* outBytes);

// ── G5b：探针 UBO 冻结契约（48 B；docs/GI_PLAN.md §7.1）─────────────────
// 偏移：origin xyz+pad @0(16) / spacing+counts xyz @16(16) / weight+reserved @32(16)。
// spacingCounts：x=spacing, y=countX, z=countY, w=countZ；weight[0]=0 表示关闭。
struct sPROBE_UBO {
    float origin[4];
    float spacingCounts[4];
    float weight[4];
};

static_assert(sizeof(sPROBE_UBO) == 48, "sPROBE_UBO 必须与 shader 契约一致（48 B）");
static_assert(offsetof(sPROBE_UBO, origin) == 0, "sPROBE_UBO.origin 偏移错误");
static_assert(offsetof(sPROBE_UBO, spacingCounts) == 16, "sPROBE_UBO.spacingCounts 偏移错误");
static_assert(offsetof(sPROBE_UBO, weight) == 32, "sPROBE_UBO.weight 偏移错误");

// 默认探针参数：8×4×8、间距 2 m、origin 0、weight 0（默认关，观感不变）。
sPROBE_UBO makeDefaultProbeUbo();

// 把 48 B 的 sPROBE_UBO 原样打包到调用方缓冲；空指针返回 false。
bool packProbeUbo(const sPROBE_UBO& ubo, void* outBytes);

// 生成 16 点半球核；kernel 为 [SSAO_KERNEL_SIZE][4]（xyz + padding）。
// 方向由 Halton(2,3) 余弦加权半球给出，长度按 0.1..1 递增，再按 seed 做固定切向旋转；
// 同 seed 逐位可重复，不同 seed 方向不同、长度不变。
void generateSsaoKernel(float kernel[SSAO_KERNEL_SIZE][4], std::uint32_t seed);

// 从 equirect 环境图（width×height×3，线性 radiance，v=0 为北极、方向 (x,y,z) 中 y 向上）
// 积分出 SH9 radiance 系数；sh 为 36 float。空指针/尺寸 < 2 返回 false 并把 sh 清零。
bool shProjectEquirect(const float* envRgb, int width, int height, float sh[SH9_FLOAT_COUNT]);

// 用 SH9 系数求某方向（世界空间、y 向上）的 irradiance（已含余弦卷积 A_l=π,2π/3,π/4）。
// 空指针/零长度/非有限法线时输出 0。
void evalSh9(const float sh[SH9_FLOAT_COUNT], const float normal[3], float outIrradiance[3]);

// 生成 width×height 的 BRDF LUT（RG 两通道，行=roughness、列=NdotV；64 样本 split-sum 积分）。
// 空指针或尺寸 < 1 返回 false。
bool generateBrdfLut(float* rgOut, int width, int height);

// 双线性采样 BRDF LUT，输入钳制到 [0,1]；空指针/尺寸 < 1 返回 false。
bool evalBrdfLut(const float* lut, int width, int height, float ndotV, float roughness,
                 float outRG[2]);

// ── 环境 cubemap 预滤波与 SH9（G1 CPU 增量）────────────────────────────
// 面序固定 +X,-X,+Y,-Y,+Z,-Z；输入为 6 面连续数据，每面 faceSize²×channels
// （channels=3 为 RGB、4 为 RGBA），行主序。像素 (x,y) 的局部坐标
// u=2(x+0.5)/faceSize-1、v=2(y+0.5)/faceSize-1，方向（y 向上、未归一化）：
//   +X=(1,-v,-u) -X=(-1,-v,u) +Y=(u,1,v)
//   -Y=(u,-1,-v) +Z=(u,-v,1) -Z=(-u,-v,-1)
// 预滤波输出为 mip 主序：mip0 的 6 面在前，然后 mip1…；每级每面
// levelSize²×4（RGBA float，levelSize=max(1,faceSize>>mip)）。
constexpr int CUBEMAP_PREFILTER_SAMPLES = 64;

// 合法 mip 级数上限（size 逐级减半到 1）；faceSize<1 返回 0。
int cubemapMaxMipCount(int faceSize);

// 预滤波输出总 float 数（mipCount 非法返回 0）。
std::size_t prefilterCubemapFloatCount(int faceSize, int mipCount);

// 第 mip 级在输出中的起始 float 偏移（参数非法返回 0）。
std::size_t prefilterCubemapMipOffset(int faceSize, int mipCount, int mip);

// 第 face 面像素局部坐标 (u,v)（-1..1）→ 归一化方向；face 越界按 +Z。
void cubemapFaceDirection(int face, float u, float v, float outDir[3]);

// GGX 预滤波（64 样本，r=mip/(mipCount-1)）：mip0 原样拷贝输入（RGB + alpha，
// 输入无 alpha 时 A=1），mip>0 输出预滤波 RGB、A=1。参数非法返回 false 且不写 outRgba。
bool prefilterCubemap(const float* envFaces, int faceSize, int inputChannels, int mipCount,
                      float* outRgba);

// 从 6 面 cubemap 积分 SH9 radiance 系数（与 shProjectEquirect 同一基与顺序）。
// 参数非法返回 false 并把 sh 清零。
bool shProjectCubemap(const float* envFaces, int faceSize, int inputChannels,
                      float sh[SH9_FLOAT_COUNT]);

// 线性 ↔ sRGB 传递函数（§5.2：albedo 用 sRGB 编码避免 8-bit 线性暗部带状）。
float srgbEncode(float linearValue);
float srgbDecode(float encodedValue);

// albedo RGBA8：RGB 走 sRGB 编码，A 线性；输入钳制到 [0,1]，NaN 视为 0。
void packAlbedoRgba8(const float rgb[3], float alpha, std::uint8_t outRgba[4]);
void unpackAlbedoRgba8(const std::uint8_t rgba[4], float outRgb[3], float* outAlpha);

// G-Buffer 材质位域（§5.2）：R=AO、G=反射率、B=材质 ID、A=alpha，各 8 bit（低字节在前）；
// 非有限/越界输入钳制，materialId 钳制到 0..255。
std::uint32_t packGbufferMaterial(float ao, float reflectance, std::uint32_t materialId,
                                  float alpha);
void unpackGbufferMaterial(std::uint32_t packed, float* outAo, float* outReflectance,
                           std::uint32_t* outMaterialId, float* outAlpha);

// octahedral 法线编码/解码（§5.4：编码/解码函数进 giParams，CPU 单测 roundtrip）。
// 编码输出两分量落在 [0,1]，便于存 RG16F；零/非有限法线编码为 (0.5,0.5)（解码得 +Z）。
void octEncodeNormal(const float normal[3], float outOct[2]);
bool octDecodeNormal(const float oct[2], float outNormal[3]);
// snorm16 量化版本（RG16 存储）；量化误差应 ≤ 1e-3。
void octEncodeSnorm16(const float normal[3], std::int16_t outOct[2]);
bool octDecodeSnorm16(const std::int16_t oct[2], float outNormal[3]);

// alpha cutoff 判定（§5.1-4）：cutoff<=0 表示不裁剪；否则 alpha>=cutoff 通过。
// alpha 为 NaN 判失败；cutoff 为 NaN 视为不裁剪。
bool alphaTestPasses(float alpha, float cutoff);

} // namespace GN3d

#endif // GN_GI_PARAMS_H
