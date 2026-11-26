#ifndef _GN_VK_H_
#define _GN_VK_H_

// ═══════════════════════════════════════════════════════════
//  多光源 cube shadow（B2 扩展）
// ═══════════════════════════════════════════════════════════
// 一张 cube map 只能服务一盏灯（后渲染的会覆盖前面的），所以这里做成
// **cube array**：每盏灯占 6 层（CUBE_ARRAY 的每一"层"是一个完整立方图）。
// 采样用 `texture(samplerCubeArray, vec4(dir, lightIndex), 0.0)`。
//
// 上限取 4：每盏灯 6 × 1024² × 4 B ≈ 25 MB，4 盏 = 100 MB，已经不小。
// 超过 4 盏灯需要阴影时，超出的按"无阴影"处理（而不是静默复用别人的层 ——
// 那会让阴影出现在错误的灯上，比没有阴影更糟）。
#define GN_MAX_CUBE_SHADOW_LIGHTS 4
// 总层数 = 灯数 × 6
#define GN_CUBE_SHADOW_TOTAL_LAYERS (GN_MAX_CUBE_SHADOW_LIGHTS * 6)
// ╔══════════════════════════════════════════════════════════╗
// ║  GN_mini — Vulkan 后端 (替代 OpenGL/_Graphics)          ║
// ╚══════════════════════════════════════════════════════════╝
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <cstring>
#include <cstdlib>
#include "GN_Pipeline.h" // P1b：管线缓存以值成员持有，需完整类型
#include "GN_Material.h" // P1c：材质实例（CPU 描述 + GPU 句柄槽）
#include "GN_3Dtexture.h" // P1c：纹理解码结果与 glTF sampler 参数
#include "GN_3DparticleGPU.h" // P9：GPU 粒子 SSBO 布局与绘制参数
#include "GN_3DclothGPU.h" // P9：GPU 布料 SSBO 布局与 CPU 回退
#include "GN_3Dparticle.h" // P11-S5：降水绘制直接消费 CPU 粒子池（GN3d::sPARTICLE）
#include "GN_3Ddecal.h" // P11-S6b：贴花渲染条目（GN3d::sDECAL_RENDER_ITEM）
#include "giParams.h" // G1a：sGI_UBO 契约（GN_VkSetGI 参数类型）
#include "GN_Environment.h" // G1b：环境光照预设（eENV_LIGHTING）
#include "postParams.h" // L1：sPOST_UBO 契约（GN_VkSetPost 参数类型）
#include "atmosphereParams.h" // L4：sATMOSPHERE_PARAMS 契约（GN_VkSetAtmosphere 参数类型）

// GN3d 前向声明（放在 GNg 外层，否则会变成 GNg::GN3d）
namespace GN3d {
class cMODEL;
struct sSHADOW_CASCADE_RESULT;
} // namespace GN3d

namespace GNg {
// 2026-11-26 B4 move-only 拆分：片段在 *.inc，包含顺序即原文件顺序，不要调整。
#include "vkImageTypes.inc"
#include "vkContext.inc"
#include "vkApi2d.inc"
#include "vkApi3d.inc"
#include "vkTestHooks.inc"
#endif // _GN_VK_H_
