#ifndef _GN_RENDER_PIPELINE_H_
#define _GN_RENDER_PIPELINE_H_
// GN_RenderPipeline.h — 多层渲染的 CPU 侧契约：层枚举、着色器程序、材质实例、
// 渲染项、层掩码解析、程序合法性校验与层内排序。纯 CPU，不 include Vulkan。
// 契约冻结见 docs/MULTILAYER_RENDER_PIPELINE_PLAN.md §7。

#include "GN_3Dmath.h"

#include <cstddef>
#include <cstdint>

namespace GNg {
struct sIMAGE; // 前向声明；Vulkan 句柄只在 _Vulkan 侧解释
}

namespace GN3d {

// 层顺序固定（docs/MULTILAYER_RENDER_PIPELINE_PLAN.md §3.2）。
enum eRENDER_LAYER {
    eLAYER_SHADOW = 0,
    eLAYER_GBUFFER,
    eLAYER_FORWARD_OPAQUE,
    eLAYER_OUTLINE,
    eLAYER_FORWARD_ALPHA,
    eLAYER_EFFECT,
    eLAYER_OVERLAY,
    eLAYER_COUNT
};

enum eVERTEX_FORMAT {
    eVF_STATIC = 0, // VkVertex3D（11 float）
    eVF_SKINNED,    // sSKINNED_VERTEX（68 B）
    eVF_CUSTOM      // v1 保留桩，校验拒绝
};

enum eSKIN_DATA {
    eSKIN_NONE = 0,
    eSKIN_UBO,  // set2 binding0 = 8 KB palette UBO
    eSKIN_SSBO  // set2 binding0 = storage buffer
};

enum ePROGRAM_FLAGS {
    ePROG_IMPLICIT_SHADOW = 1u << 0 // 无 SHADOW 入口时允许内置 depth 代打
};

const uint32_t kBindingLayoutVersion = 1;
const uint32_t kMaxRenderItems = 4096;
const uint32_t kMaxShaderPrograms = 64;
const uint32_t kMaxMaterialInstances = 1024;
const uint32_t kMaxPushConstantBytes = 128;
const uint32_t kMaxSpecConstants = 8;

struct sSPEC_CONST {
    uint32_t aId;
    uint32_t aValue; // v1 只支持 32 位标量；float 用位模式
};

// 某一层的入口；aBlend/aDepthTest/... 为 -1 时使用层默认。
struct sSHADER_ENTRY {
    const void *aVertSpv;
    uint32_t aVertSize;
    const void *aFragSpv;
    uint32_t aFragSize;
    uint32_t aPushConstantSize;
    int32_t aBlend;
    int32_t aDepthTest;
    int32_t aDepthWrite;
    int32_t aCullBack;
    int32_t aTopology;
    uint32_t aSpecCount;
    sSPEC_CONST aSpec[kMaxSpecConstants];
};

struct sSHADER_PROGRAM_DESC {
    const char *aName;
    uint32_t aLayerMask;
    uint32_t aFlags;
    eVERTEX_FORMAT aVertexFormat;
    eSKIN_DATA aSkinData;
    uint32_t aLayoutVersion;
    sSHADER_ENTRY aEntries[eLAYER_COUNT];
};

// 注册后的程序对象；aRuntime 由 Vulkan 侧解释，CPU 层不碰。
struct sSHADER_PROGRAM {
    sSHADER_PROGRAM_DESC aDesc;
    uint32_t aId;
    void *aRuntime;
};

// set1 binding0 的 CPU 镜像（256 B，std140）。
struct sMATERIAL_UBO {
    float aAlbedo[4];    // rgb + alphaCutoff
    float aPbr[4];       // roughness, metallic, ao, reserved
    float aEmissive[4];  // rgb + reserved
    float aReserved[4];
    float aCustom[12][4]; // 程序自定义参数
};
static_assert(sizeof(sMATERIAL_UBO) == 256, "sMATERIAL_UBO 必须为 256 字节");

struct sMATERIAL_INSTANCE {
    const sSHADER_PROGRAM *aProgram; // 非空
    uint32_t aLayerMask;             // 0 = 用程序默认
    sMATERIAL_UBO aUbo;
    const GNg::sIMAGE *aTextures[7]; // 对应 set1 binding 1..7
};

struct sRENDER_ITEM {
    const void *aVertices;  // 借用，活到队列 reset
    uint32_t aVertexCount;
    const uint32_t *aIndices; // 借用，活到队列 reset
    uint32_t aIndexCount;
    GNg::sMAT44 aModelMatrix; // 按值复制
    const sMATERIAL_INSTANCE *aMaterial; // 非空
    uint32_t aLayerMask;                 // 0 = 材质/程序默认
    const sSHADER_PROGRAM *aLayerProgram[eLAYER_COUNT]; // 可空
    uint32_t aSortKey;
};

// 层名（调试/日志用；层号非法返回 "invalid"）。
const char *RenderLayerName(eRENDER_LAYER layer);

// 解析顺序：item.layerMask → material.aLayerMask → program.aLayerMask。
uint32_t ResolveLayerMask(const sRENDER_ITEM &item);

// 解析某层的程序入口；implicitShadowProgram 用于 ePROG_IMPLICIT_SHADOW 的代打。
const sSHADER_ENTRY *ResolveEntry(const sRENDER_ITEM &item, eRENDER_LAYER layer,
                                  const sSHADER_PROGRAM *implicitShadowProgram = 0);

// 校验程序描述；失败返回 false 并写 reason（可为 null）。
bool ValidateShaderProgramDesc(const sSHADER_PROGRAM_DESC &desc, char *reason, size_t reasonSize);

// 校验渲染项；失败返回 false 并写 reason（可为 null）。
bool ValidateRenderItem(const sRENDER_ITEM &item, char *reason, size_t reasonSize);

// 层内稳定排序：programId 升序 → 视空间深度（ALPHA 后→前，其余前→后）→ aSortKey。
void SortRenderItems(sRENDER_ITEM *items, uint32_t count, eRENDER_LAYER layer,
                     const GNg::sMAT44 &view);

} // namespace GN3d
#endif // _GN_RENDER_PIPELINE_H_
