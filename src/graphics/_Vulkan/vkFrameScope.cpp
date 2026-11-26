// vkFrameScope.cpp — 3D pass 作用域守卫 + 逐级阴影提交（A6 收尾）。
//
// 这两个东西都是把「两个示例里逐字相同的代码」收进引擎，但价值在别处：
//   * drawShadowCascades：消除完全相同的 for 循环；
//   * sFRAME_3D_SCOPE：防住「中途 return 导致 GN_Vk.in3DPass 永久 true ⇒ 画面冻结」。
#include <GN.h>
#include "GN_Vk.h"

#ifdef GN_USING_3D
#include "GN_3Dmodel.h"      // cMODEL::DrawShadow
#include "GN_ShadowCascade.h" // sSHADOW_CASCADE_RESULT
#endif

namespace GNg {

void GN_VkDrawShadowCascades(const GN3d::cMODEL &caster, const sMAT44 *lightVPs,
                             uint32_t count) {
#ifdef GN_USING_3D
    if(!GN_Vk.initialized || lightVPs == nullptr) return;
    const uint32_t layers = GN_VkShadowCascadeCount();
    if(count > layers) count = layers;
    for(uint32_t i = 0; i < count; ++i) {
        GN_VkShadowPassBegin(&lightVPs[i], i);
        caster.DrawShadow();
        GN_VkShadowPassEnd();
    }
#else
    (void)caster;
    (void)lightVPs;
    (void)count;
#endif
}

sFRAME_3D_SCOPE::sFRAME_3D_SCOPE(const sMAT44 *viewProj, const sVEC3 *cameraPos,
                                 const void *lightUBO, uint32_t lightUBOSize, bool drawSkybox)
    : aActive(false) {
#ifdef GN_USING_3D
    GN_VkBegin3DPass(viewProj, cameraPos, lightUBO, lightUBOSize);
    // Begin3DPass 在「设备未初始化」或「已在 3D pass 内」时静默返回，所以必须回查状态，
    // 不能假定成功。否则析构里会去 End 一个从没开始过的 pass。
    aActive = GN_VkIsIn3DPass();
    if(aActive && drawSkybox) {
        GN_VkDrawSkybox(viewProj);
    }
#else
    (void)viewProj;
    (void)cameraPos;
    (void)lightUBO;
    (void)lightUBOSize;
    (void)drawSkybox;
#endif
}

sFRAME_3D_SCOPE::~sFRAME_3D_SCOPE() {
#ifdef GN_USING_3D
    if(!aActive) return;
    // 必须先 End3DPass（内部会做 HDR→交换链合成），再开 2D 叠层。
    GN_VkEnd3DPass();
    GN_VkBegin2DPass();
#endif
    aActive = false;
}

} // namespace GNg
