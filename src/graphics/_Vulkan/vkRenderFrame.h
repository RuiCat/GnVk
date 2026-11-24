#pragma once
#ifndef _GN_VK_RENDERFRAME_H_
#define _GN_VK_RENDERFRAME_H_
// vkRenderFrame.h — 一帧的渲染编排（P1：从示例上移到引擎的薄编排层）。
//
// 为什么需要它：两个示例的主循环里各有一段「阴影级联 → 3D pass（天空盒 + 几何）→
// 2D 叠层起点」的编排。这段代码本身不长，但它把三件engine-level的事写在了示例里：
//   1. 级联级数必须分给每级一次 shadow pass（for 循环 + 层数钳制）；
//   2. 3D pass 必须成对 Begin/End（漏 End 会静默黑屏，见 sFRAME_3D_SCOPE 注释）；
//   3. 天空盒永远第一个画。
// 换一个场景就要再抄一遍。这里把它收成 cRENDER_FRAME：调用方只管"要画什么"，
// 不再管"什么时候进 pass、什么时候提交阴影"。
//
// 边界：**不创建任何 Vulkan 资源**，只调用既有 pass 函数；不改 pass 实现。
// 属于 L3（_Vulkan），因为它直接组合 pass；上层（tests/、GN_Entity）不需要包含它。
#include <GN.h> // GN_API / sMAT44 / sVEC3 / GN_USING_3D

#include <memory>

namespace GN3d {
class cMODEL;
}

namespace GNg {

// 一帧的公共输入。字段全部直接取自相机与光源，不做任何推导。
struct sRENDER_FRAME_DESC {
    const sMAT44 *aViewProj; // 本帧 view×proj（非空）
    const sVEC3 *aCameraPos; // 相机世界位置（可空 = 不用眼睛位置的 pass）
    const void *aLightUBO; // 光源 UBO 数据（sLIGHT_UBO）
    uint32_t aLightUBOSize; // 字节数
    bool aDrawSkybox; // 默认 true（示例都是"天空盒永远第一"）

    sRENDER_FRAME_DESC();
};

// 一帧的编排对象。典型用法：
//     GNg::cRENDER_FRAME frame;
//     if(!frame.ShadowPass(casters, casterCount, state)) return;   // 可选
//     if(!frame.Begin(desc)) return;                               // 进不去就直接返回
//     ... 画几何（地面/模型/贴花/粒子/线框）...
//     frame.End();                                                 // 也可省略，析构会收
//     drawOverlay();                                               // 2D 叠层在 pass 外
class cRENDER_FRAME {
  public:
    cRENDER_FRAME();
    ~cRENDER_FRAME();

    // 阴影：逐级提交。casters 为空的项会被跳过；级数为 0 / 引擎未初始化时返回 false。
    // 多个 caster 共用同一组光矩阵（同一光照下所有投影体一起进深度图）。
    bool ShadowPass(const GN3d::cMODEL *const *casters, uint32_t casterCount,
                    const sMAT44 *lightVPs, uint32_t cascadeCount);
    // 便捷版：单个 caster 的常见情形。
    bool ShadowPass(const GN3d::cMODEL *caster, const sMAT44 *lightVPs, uint32_t cascadeCount);

    // 进入 3D pass（含天空盒）。返回 false = 没进去（设备未初始化/已在 pass 内），
    // 调用方应直接 return 本帧。
    bool Begin(const sRENDER_FRAME_DESC &desc);
    bool Active() const;
    // 收尾：End3DPass（内含 HDR→交换链合成）+ Begin2DPass。幂等。
    void End();

    // 生命周期唯一：拷贝会导致双重 End3DPass。
    cRENDER_FRAME(const cRENDER_FRAME &) = delete;
    cRENDER_FRAME &operator=(const cRENDER_FRAME &) = delete;

  private:
    // sFRAME_3D_SCOPE 不可默认构造，用 unique_ptr 表达"进 pass 才算存在"。
    std::unique_ptr<sFRAME_3D_SCOPE> aScope;
};

} // namespace GNg
#endif // _GN_VK_RENDERFRAME_H_
