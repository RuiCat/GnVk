// vkAtmosphere.cpp — P11-S3 雾：独立 Weather/Fog UBO（不扩 SceneUBO）。
//
// 契约：
//   * `sFOG_UBO` 是三条 3D 片元管线共用的只读 UBO（std140，32 字节）：
//     color.rgb = 雾颜色，params = (density 1/m, heightFalloff 1/m, baseHeight m, maxOpacity)。
//   * 描述符绑定点由各管线自己声明：前向 binding 3、PBR binding 6、延迟光照 binding 8
//     （与 §3.6/§3.7 冻结的槽位表一致）。
//   * `density == 0` 时 fogAmount 恒为 0 ⇒ 不改变既有画面（默认关闭）。
//   * 每帧/每次天气变化调用 GN_VkSetFog 写映射内存；缓冲在 GN_VkInit3D 最先创建。
#include <GN.h>
#include "GN_Vk.h"
#include "vkInternal.h"

#include <cstring>

namespace GNg {

namespace {

VkBuffer s_fogBuffer = VK_NULL_HANDLE;
VkDeviceMemory s_fogMemory = VK_NULL_HANDLE;
void *s_fogMapped = 0;

} // namespace

bool init3DAtmosphere() {
    if(s_fogBuffer != VK_NULL_HANDLE) return true;
    if(!GN_Vk.device) return false; // 早于 GN_Vk.initialized 置位
    CreateBuf(sizeof(sFOG_UBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
              s_fogBuffer, s_fogMemory);
    if(s_fogBuffer == VK_NULL_HANDLE || s_fogMemory == VK_NULL_HANDLE) {
        destroyAtmosphere();
        return false;
    }
    if(vkMapMemory(GN_Vk.device, s_fogMemory, 0, VK_WHOLE_SIZE, 0, &s_fogMapped) != VK_SUCCESS) {
        s_fogMapped = 0;
        destroyAtmosphere();
        return false;
    }
    memset(s_fogMapped, 0, sizeof(sFOG_UBO)); // 默认 density 0 = 无雾
    return true;
}

void destroyAtmosphere() {
    if(GN_Vk.device) {
        if(s_fogBuffer != VK_NULL_HANDLE) vkDestroyBuffer(GN_Vk.device, s_fogBuffer, 0);
        if(s_fogMemory != VK_NULL_HANDLE) vkFreeMemory(GN_Vk.device, s_fogMemory, 0);
    }
    s_fogBuffer = VK_NULL_HANDLE;
    s_fogMemory = VK_NULL_HANDLE;
    s_fogMapped = 0;
}

VkBuffer atmosphereFogBuffer() { return s_fogBuffer; }

VkDeviceSize atmosphereFogRange() { return (VkDeviceSize)sizeof(sFOG_UBO); }

void GN_VkSetFog(const sFOG_UBO &fog) {
    if(!s_fogMapped) return;
    sFOG_UBO clamped = fog;
    if(clamped.aParams[0] < 0.0f) clamped.aParams[0] = 0.0f; // density
    if(clamped.aParams[1] < 0.0f) clamped.aParams[1] = 0.0f; // heightFalloff
    if(clamped.aParams[3] < 0.0f) clamped.aParams[3] = 0.0f;
    if(clamped.aParams[3] > 1.0f) clamped.aParams[3] = 1.0f; // maxOpacity
    memcpy(s_fogMapped, &clamped, sizeof(clamped));
}

} // namespace GNg
