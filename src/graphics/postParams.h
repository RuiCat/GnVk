#pragma once
#ifndef GN_POST_PARAMS_H
#define GN_POST_PARAMS_H
// ╔══════════════════════════════════════════════════════════╗
// ║  GN_mini — 后处理 CPU 基座（曝光 / 色调映射 / Bloom 参数）║
// ╚══════════════════════════════════════════════════════════╝
//
// 职责：L1 后处理链的 CPU 侧契约与参考实现——POST UBO 的默认值/校验/打包，
// 以及曝光倍率、Reinhard/ACES 色调映射的 CPU 版本（与 gn3d_composite.frag 逐公式一致，
// 单测用 CPU 版本锁公式，shader 只做同式求值）。
// 本层不碰 Vulkan、不分配资源；布局见 docs/LIGHTING_PLAN.md §4.3。
//
// 类型名 sPOST_UBO 是跨 CPU/GLSL 的 UBO 契约名；其余标识符遵循
// camelCase / PascalCase / UPPER_SNAKE。
#include <cstddef>
#include <cstdint>

namespace GN3d {

// 色调映射模式：NONE=线性直通（仅 clamp，供回归/对照），REINHARD=全局 Reinhard，
// ACES=Narkowicz 2015 单曲线 fit。
enum eTONEMAP_MODE {
    eTONEMAP_NONE = 0,
    eTONEMAP_REINHARD = 1,
    eTONEMAP_ACES = 2
};

// 与 assets/shaders/gn3d_composite.frag 的 POST_UBO 逐字段一致；sizeof = 32 B。
struct sPOST_UBO {
    float exposure;       // 线性曝光倍率（= 2^EV），默认 1
    float bloomIntensity; // Bloom 叠加强度，默认 0.3
    float bloomThreshold; // Bloom 亮度阈值（线性 HDR），默认 0.8
    float bloomKnee;      // 阈值软膝盖宽度，默认 0.3
    float ev;             // 曝光 EV（仅记录/UI 用，渲染用 exposure）
    int32_t tonemapMode;  // eTONEMAP_MODE
    float fxaa;           // L2a：FXAA 开关（0=关，1=开；合成着色器内单 pass 实现）
    float pad1;           // 对齐保留
};

static_assert(sizeof(sPOST_UBO) == 32, "sPOST_UBO 必须与 shader 契约一致（32 B）");
static_assert(offsetof(sPOST_UBO, exposure) == 0, "sPOST_UBO.exposure 偏移错误");
static_assert(offsetof(sPOST_UBO, bloomIntensity) == 4, "sPOST_UBO.bloomIntensity 偏移错误");
static_assert(offsetof(sPOST_UBO, bloomThreshold) == 8, "sPOST_UBO.bloomThreshold 偏移错误");
static_assert(offsetof(sPOST_UBO, bloomKnee) == 12, "sPOST_UBO.bloomKnee 偏移错误");
static_assert(offsetof(sPOST_UBO, ev) == 16, "sPOST_UBO.ev 偏移错误");
static_assert(offsetof(sPOST_UBO, tonemapMode) == 20, "sPOST_UBO.tonemapMode 偏移错误");
static_assert(offsetof(sPOST_UBO, fxaa) == 24, "sPOST_UBO.fxaa 偏移错误");

// 默认后处理参数：曝光 1、ACES、Bloom 0.3 / 阈值 0.8 / 膝盖 0.3（§4.3）。
sPOST_UBO makeDefaultPostUbo();

// 回归参数：线性直通 + 关闭 Bloom，等价于改造前的 LDR 输出（供 gpuCheck 旧色值用例）。
sPOST_UBO makeLegacyPostUbo();

// 校验字段合法性；非法输入返回 false，并把可用的修正结果写入 corrected（可为 nullptr）。
// 判据：float 全部有限；exposure 落在 [0, 64]；Bloom 强度/阈值/膝盖非负（强度 ≤ 10）；
// tonemapMode 只接受 0..2。修正策略为逐字段钳制/置默认，不修改合法字段。
bool validatePostUbo(const sPOST_UBO& ubo, sPOST_UBO* corrected = nullptr);

// 把 32 B 的 sPOST_UBO 原样打包到调用方缓冲；空指针返回 false。
bool packPostUbo(const sPOST_UBO& ubo, void* outBytes);

// 曝光 EV → 线性倍率（2^EV；EV 钳制到 [-10, 10]）。
float exposureScaleFromEv(float ev);

// 单通道色调映射参考实现（输入为已乘曝光的线性值）；mode 非法时按 NONE 处理。
float tonemapReinhard(float linear);
float tonemapAces(float linear);
float tonemapChannel(int32_t mode, float linear, float exposure);

} // namespace GN3d

#endif // GN_POST_PARAMS_H
