#pragma once
#ifndef _GN_VK_SKINNED_DISPATCH_H_
#define _GN_VK_SKINNED_DISPATCH_H_
// vkSkinnedDispatch.h — P2：按关节数自动分流到 UBO(≤128) / SSBO(≤4096) 蒙皮路径。
//
// 为什么单独一个头（不并进 GN_Vk.h）：本头需要 cSKELETON_CONTROLLER 的**前置声明**即可
// （完整类型只在 .cpp 里用），而 GN_Vk.h 若反过来包含 GN_Entity.h 会形成
// `GN_Entity.h → GN_3Dmodel.h → GN_Vk.h` 的循环包含。分开之后 GN_Vk.h 保持干净，
// 只有真正要画蒙皮网格的调用方才包含本头。
//
// 依赖方向：本头在 L3（_Vulkan）→ 引用 L2 的 GN_3DSkin/GN_3Dmath（合法向下依赖）。
#include <GN.h> // GN_API / sMAT44
#include "GN_3DSkin.h" // eSKIN_PALETTE_PATH / SKIN_SSBO_MAX_JOINTS

namespace GN3d {
class cSKELETON_CONTROLLER; // 只取 palette，前置声明足够
}

namespace GNg {

struct sSKINNED_DRAW_DESC {
    const void *aVertices; // GN3d::sSKINNED_VERTEX 数组（68B/顶点）
    uint32_t aVertexCount;
    const uint32_t *aIndices; // 可空（空 = 非索引绘制）
    uint32_t aIndexCount;
    const GN3d::cSKELETON_CONTROLLER *aPose; // 提供 palette（须 PaletteValid()）
    const sMAT44 *aModelMatrix; // 蒙皮在模型空间完成后乘它
    float aColor[3]; // albedo（frag 只做 albedo × Lambert）

    sSKINNED_DRAW_DESC();
};

// 按 palette 大小自动选路径并提交。返回 false：无调色板 / 参数非法 /
// 关节数超出 SSBO 容量（>4096，明确失败而不是静默截断）/ 后端拒绝提交。
bool GN_VkDrawSkinnedAuto(const sSKINNED_DRAW_DESC &desc);

// 上一次真正走的路径（诊断/验收用；包含"被拒绝"的情形 = eSKIN_PATH_NONE）。
GN3d::eSKIN_PALETTE_PATH GN_VkLastSkinnedPath();

} // namespace GNg
#endif // _GN_VK_SKINNED_DISPATCH_H_
