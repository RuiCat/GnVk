// vkRenderFrame.cpp — 一帧的渲染编排（阴影 → 3D pass → 2D 叠层起点）。
#include <GN.h>
#include "vkRenderFrame.h"

#ifdef GN_USING_3D
#include "GN_3Dmodel.h"
#endif

namespace GNg {

sRENDER_FRAME_DESC::sRENDER_FRAME_DESC()
    : aViewProj(nullptr), aCameraPos(nullptr), aLightUBO(nullptr), aLightUBOSize(0),
      aDrawSkybox(true) {}

cRENDER_FRAME::cRENDER_FRAME() {}

cRENDER_FRAME::~cRENDER_FRAME() { End(); }

bool cRENDER_FRAME::ShadowPass(const GN3d::cMODEL *caster, const sMAT44 *lightVPs,
                               uint32_t cascadeCount) {
    if(caster == nullptr) return false;
    const GN3d::cMODEL *casters[1] = {caster};
    return ShadowPass(casters, 1u, lightVPs, cascadeCount);
}

bool cRENDER_FRAME::ShadowPass(const GN3d::cMODEL *const *casters, uint32_t casterCount,
                               const sMAT44 *lightVPs, uint32_t cascadeCount) {
#ifdef GN_USING_3D
    if(casters == nullptr || casterCount == 0u || lightVPs == nullptr) return false;
    uint32_t submitted = 0;
    for(uint32_t i = 0; i < casterCount; ++i) {
        if(casters[i] == nullptr) continue;
        // GN_VkDrawShadowCascades 内部已把级数钳到 GN_VkShadowCascadeCount()，
        // 并逐级做 Begin/DrawShadow/End。
        GN_VkDrawShadowCascades(*casters[i], lightVPs, cascadeCount);
        ++submitted;
    }
    return submitted > 0u;
#else
    (void)casters;
    (void)casterCount;
    (void)lightVPs;
    (void)cascadeCount;
    return false;
#endif
}

bool cRENDER_FRAME::Begin(const sRENDER_FRAME_DESC &desc) {
    if(aScope) return aScope->Valid(); // 已在 pass 内：幂等（不重复 Begin）
    if(desc.aViewProj == nullptr) return false;
    aScope.reset(new sFRAME_3D_SCOPE(desc.aViewProj, desc.aCameraPos, desc.aLightUBO,
                                     desc.aLightUBOSize, desc.aDrawSkybox));
    if(!aScope->Valid()) {
        aScope.reset(); // 没进去就别留着（析构里不能去 End 一个没开始的 pass）
        return false;
    }
    return true;
}

bool cRENDER_FRAME::Active() const { return aScope != nullptr && aScope->Valid(); }

void cRENDER_FRAME::End() {
    // reset 会触发 sFRAME_3D_SCOPE 的析构：End3DPass + Begin2DPass。
    aScope.reset();
}

} // namespace GNg
