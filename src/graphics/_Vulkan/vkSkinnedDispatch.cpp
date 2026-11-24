// vkSkinnedDispatch.cpp — P2：按关节数自动选 UBO / SSBO 蒙皮路径并提交绘制。
//
// 为什么这一层必须存在：两套绘制入口（GN_VkDraw3DSkinned / GN_VkDraw3DSkinnedSsbo）
// 早就都有，但"该用哪一套"从来没人判断——调用方要么写死 UBO（MMD 的 200+ 关节静默
// 截断成缺胳膊少腿），要么自己复制一段 if。这里把判断收成一处。
//
// 分层：本文件在 L3（_Vulkan），可以向下依赖 GN_Entity（L2）拿调色板；反向不行，
// 所以 cSKELETON_CONTROLLER 保持零 Vulkan 依赖，绘制入口留在这里。
#include <GN.h>
#include "GN_Vk.h"
#include "GN_Entity.h" // 完整类型：cSKELETON_CONTROLLER::Palette()
#include "GN_3DSkin.h"
#include "vkSkinnedDispatch.h"

namespace GNg {

// 上一次 GN_VkDrawSkinnedAuto 实际走的路径（诊断/验收用；未绘制过为 eSKIN_PATH_NONE）。
static GN3d::eSKIN_PALETTE_PATH s_lastSkinnedPath = GN3d::eSKIN_PATH_NONE;

sSKINNED_DRAW_DESC::sSKINNED_DRAW_DESC()
    : aVertices(nullptr), aVertexCount(0), aIndices(nullptr), aIndexCount(0), aPose(nullptr),
      aModelMatrix(nullptr) {
    aColor[0] = 0.8f;
    aColor[1] = 0.8f;
    aColor[2] = 0.8f;
}

GN3d::eSKIN_PALETTE_PATH GN_VkLastSkinnedPath() { return s_lastSkinnedPath; }

bool GN_VkDrawSkinnedAuto(const sSKINNED_DRAW_DESC &desc) {
    s_lastSkinnedPath = GN3d::eSKIN_PATH_NONE;

    if(desc.aVertices == nullptr || desc.aVertexCount == 0) return false;
    if(desc.aPose == nullptr || desc.aModelMatrix == nullptr) return false;
    if(!desc.aPose->PaletteValid()) return false; // 没算过调色板：画出来是零矩阵，不如不画

    const std::vector<sMAT44> &palette = desc.aPose->Palette();
    if(palette.empty()) return false;

    const uint32_t jointCount = (uint32_t)palette.size();
    const GN3d::eSKIN_PALETTE_PATH path = GN3d::selectSkinPalettePath(jointCount);
    s_lastSkinnedPath = path;
    if(path == GN3d::eSKIN_PATH_NONE) {
        // 超容量：明确失败而不是静默截断（截断=角色缺肢体且看不出原因）。
        return false;
    }

    if(path == GN3d::eSKIN_PATH_UBO) {
        // UBO 路径内部会 packPalette 截断到 128；本分支的 jointCount 已保证 ≤128。
        return GN_VkDraw3DSkinned(desc.aVertices, desc.aVertexCount, desc.aIndices,
                                  desc.aIndexCount, palette.data(), jointCount,
                                  desc.aModelMatrix, desc.aColor[0], desc.aColor[1],
                                  desc.aColor[2]);
    }
    return GN_VkDraw3DSkinnedSsbo(desc.aVertices, desc.aVertexCount, desc.aIndices,
                                  desc.aIndexCount, palette.data(), jointCount, desc.aModelMatrix,
                                  desc.aColor[0], desc.aColor[1], desc.aColor[2]);
}

} // namespace GNg
