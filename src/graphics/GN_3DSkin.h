#pragma once
#ifndef _GN_3DSKIN_H_
#define _GN_3DSKIN_H_
// ╔══════════════════════════════════════════════════════════╗
// ║  GN_mini — 蒙皮顶点布局 / CPU 线性混合蒙皮 / 关节调色板   ║
// ╚══════════════════════════════════════════════════════════╝
//
// 契约（P2-C）：
//   * 顶点布局与 GN_3Dasset.h 的 sASSET_VERTEX 同序（pos/normal/uv/4 关节+4 权重），末尾追加切线；
//     关节为 uint16（skin.joints 下标），权重由导入层给出、由本层再归一化。
//   * skinVertices 只做 CPU LBS：不分配内存、不修改输入；权重和为 0（或调色板为空）时退化为原顶点，
//     关节索引越界钳到 [0, paletteCount-1]，法线按 3x3 变换（不受平移影响）后归一化。
//   * sSKIN_UBO 为 GPU 关节调色板布局（128 × mat4 = 8KB）；超过 128 个关节截断并把未用槽位清零。
//   * GPU 侧只提供创建/销毁骨架（_Vulkan/vkSkin.cpp），不做绑定与绘制。
#include "GN_3Dmath.h"

#include <cstdint>

namespace GN3d {

// GPU 关节调色板上限：128 个关节 × mat4 = 8192 字节。
const uint32_t SKIN_MAX_JOINTS = 128;

// 蒙皮顶点：68 字节，4 字节标量紧凑布局（GPU 侧按相同顺序取属性）。
struct sSKINNED_VERTEX {
    GNg::sVEC3 aPosition;
    GNg::sVEC3 aNormal;
    float aUv[2];
    GNg::sVEC3 aTangent;
    uint16_t aJoints[4]; // 关节索引（sSKELETON 关节 / skin.joints 下标）
    float aWeights[4]; // 导入层给出的权重；skinVertices 内部再归一化
};

static_assert(sizeof(sSKINNED_VERTEX) == 68, "sSKINNED_VERTEX 必须为紧凑 68 字节");

// 关节调色板 UBO：每关节一个列主序 mat4（与 GNg::sMAT44 内存布局一致）。
struct sSKIN_UBO {
    float aJointMatrices[SKIN_MAX_JOINTS][16];
};

// 纯 CPU 线性混合蒙皮：按权重混合调色板变换，写出位置与法线。
// 要求 in/outPositions/outNormals 非空且各覆盖 count 项；只返回 false 当指针为空（count == 0 视为成功）。
bool skinVertices(const sSKINNED_VERTEX *in, uint32_t count, const GNg::sMAT44 *palette,
                  uint32_t paletteCount, GNg::sVEC3 *outPositions, GNg::sVEC3 *outNormals);

// 打包调色板到 UBO：最多 SKIN_MAX_JOINTS 个，超出截断，未用槽位清零。
// 返回实际写入的矩阵数（= min(paletteCount, SKIN_MAX_JOINTS)）。
uint32_t packPalette(const GNg::sMAT44 *palette, uint32_t paletteCount, sSKIN_UBO &out);

// ═══════════════════════════════════════════════════════════
//  调色板路径分流（P2）：UBO ≤128 / SSBO ≤4096 / 拒绝
// ═══════════════════════════════════════════════════════════
// 为什么要有它：MMD 模型常见 200+ 关节，超过 UBO 的 128 就会静默截断（画面坏但不说原因）。
// 引擎两套绘制入口都早已存在（GN_VkDraw3DSkinned / ...Ssbo），缺的只是"按关节数选一套"
// ——这段判断原先谁都不做。放在本头（纯 CPU）而不是 Vulkan 层，是为了能无 GPU 单测。

// SSBO 调色板容量，与 _Vulkan/vkSkinSsbo.cpp 里的缓冲容量是同一条定义（共享常量，
// 两边不可能漂移）。v1 上限 4096。
const uint32_t SKIN_SSBO_MAX_JOINTS = 4096;

enum eSKIN_PALETTE_PATH {
    eSKIN_PATH_NONE = 0, // 无关节 / 超出 SSBO 上限：调用方不应绘制（而不是静默画错）
    eSKIN_PATH_UBO = 1, // 1..SKIN_MAX_JOINTS：8KB UBO，着色器把索引钳到 127
    eSKIN_PATH_SSBO = 2 // SKIN_MAX_JOINTS+1..SKIN_SSBO_MAX_JOINTS：SSBO，索引钳到 4095
};

// 该路径能容纳的关节数上限（NONE = 0）。
uint32_t skinPaletteCapacity(eSKIN_PALETTE_PATH path);

// 按关节数选路径：
//   0                     → NONE
//   1..128                → UBO
//   129..4096             → SSBO
//   >4096                 → NONE（超容量。**不静默截断**：截断会让角色缺肢体，
//                            却看不出是"关节太多"，所以宁可让调用方报错/换资产）
eSKIN_PALETTE_PATH selectSkinPalettePath(uint32_t jointCount);

// 该关节数在这条路径上是否会被截断（仅用于告警文案；NONE 返回 true）。
bool skinPaletteTruncated(uint32_t jointCount);

} // namespace GN3d
#endif // _GN_3DSKIN_H_
