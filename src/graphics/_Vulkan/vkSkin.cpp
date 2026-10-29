// vkSkin.cpp — P2 GPU 蒙皮的关节调色板 UBO 环形槽位（创建/销毁/写入）。
//
// 边界：
//   * 缓冲为 3 个 8KB host-visible/coherent UBO（128 × mat4），按 GN_Vk.currentFrame
//     选槽（frame % 3），避免 CPU 写入覆盖仍在 GPU 上执行的帧。
//   * createSkinPaletteBuffer 在 GN_VkInit3D 末尾调用，此时 GN_Vk.initialized 尚未置真，
//     因此只以 device 有效为判据；未初始化/创建失败返回 false，不崩溃。
//   * createSkinPaletteBuffer / destroySkinPaletteBuffer 保持 vkInternal.h 冻结声明可用；
//     槽位读取接口只供 vkSkinDraw.cpp 在同一模块内使用（GN_VK_INTERNAL，不导出）。
#include <GN.h>
#include "GN_3DSkin.h"
#include "GN_Vk.h"
#include "vkInternal.h"

#include <cstring>

namespace GNg {
namespace {

// 环形槽位数：覆盖本机 swapchain 的在飞帧数上限（2~3），供 currentFrame 取模选槽。
const uint32_t SKIN_PALETTE_SLOT_COUNT = 3;

struct sPALETTE_SLOT {
    VkBuffer aBuffer;
    VkDeviceMemory aMemory;
};

sPALETTE_SLOT s_paletteSlots[SKIN_PALETTE_SLOT_COUNT] = {};

} // namespace

// 销毁全部调色板环形缓冲；device 已失效时只清句柄（不能对无效 device 调 vkDestroyBuffer）。
void destroySkinPaletteBuffer() {
    if(GN_Vk.device != VK_NULL_HANDLE) {
        for(uint32_t slot = 0; slot < SKIN_PALETTE_SLOT_COUNT; ++slot) {
            if(s_paletteSlots[slot].aBuffer != VK_NULL_HANDLE) {
                vkDestroyBuffer(GN_Vk.device, s_paletteSlots[slot].aBuffer, nullptr);
            }
            if(s_paletteSlots[slot].aMemory != VK_NULL_HANDLE) {
                vkFreeMemory(GN_Vk.device, s_paletteSlots[slot].aMemory, nullptr);
            }
        }
    }
    for(uint32_t slot = 0; slot < SKIN_PALETTE_SLOT_COUNT; ++slot) {
        s_paletteSlots[slot].aBuffer = VK_NULL_HANDLE;
        s_paletteSlots[slot].aMemory = VK_NULL_HANDLE;
    }
}

// 创建并清零 3 个调色板槽位。device 无效/创建失败返回 false；已存在视为成功（幂等）。
bool createSkinPaletteBuffer() {
    static_assert(sizeof(GN3d::sSKIN_UBO) == GN3d::SKIN_MAX_JOINTS * 16u * 4u,
                  "sSKIN_UBO 必须为 128 x mat4 = 8KB");
    if(GN_Vk.device == VK_NULL_HANDLE) return false;
    if(s_paletteSlots[0].aBuffer != VK_NULL_HANDLE) return true;

    for(uint32_t slot = 0; slot < SKIN_PALETTE_SLOT_COUNT; ++slot) {
        const VkDeviceSize byteSize = (VkDeviceSize)sizeof(GN3d::sSKIN_UBO);
        CreateBuf(byteSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                  s_paletteSlots[slot].aBuffer, s_paletteSlots[slot].aMemory);
        if(s_paletteSlots[slot].aBuffer == VK_NULL_HANDLE ||
           s_paletteSlots[slot].aMemory == VK_NULL_HANDLE) {
            destroySkinPaletteBuffer();
            return false;
        }

        void *mapped = nullptr;
        if(vkMapMemory(GN_Vk.device, s_paletteSlots[slot].aMemory, 0, byteSize, 0, &mapped) !=
               VK_SUCCESS ||
           mapped == nullptr) {
            destroySkinPaletteBuffer();
            return false;
        }
        std::memset(mapped, 0, (size_t)byteSize);
        vkUnmapMemory(GN_Vk.device, s_paletteSlots[slot].aMemory);
    }
    return true;
}

// 环形槽位数（vkSkinDraw.cpp 用它分配描述符集，避免两处各写一份常量）。
GN_VK_INTERNAL uint32_t skinPaletteSlotCount() { return SKIN_PALETTE_SLOT_COUNT; }

// 按当前帧号选槽：同一帧内多次绘制共用同一槽位，跨帧轮转避免覆盖在飞数据。
GN_VK_INTERNAL uint32_t skinPaletteSlotForFrame(uint32_t frame) {
    return frame % SKIN_PALETTE_SLOT_COUNT;
}

// 单个槽位的 UBO 是否已创建。
GN_VK_INTERNAL bool skinPaletteSlotReady() {
    return s_paletteSlots[0].aBuffer != VK_NULL_HANDLE;
}

// 单个槽位的 VkBuffer；越界/未创建返回 VK_NULL_HANDLE。
GN_VK_INTERNAL VkBuffer skinPaletteSlotBuffer(uint32_t slot) {
    if(slot >= SKIN_PALETTE_SLOT_COUNT) return VK_NULL_HANDLE;
    return s_paletteSlots[slot].aBuffer;
}

// 单个槽位的字节数（= sizeof(GN3d::sSKIN_UBO) = 8192）。
GN_VK_INTERNAL VkDeviceSize skinPaletteSlotBytes() {
    return (VkDeviceSize)sizeof(GN3d::sSKIN_UBO);
}

// 把整块调色板写入指定槽位；槽位/device 无效返回 false。
GN_VK_INTERNAL bool writeSkinPalette(uint32_t slot, const GN3d::sSKIN_UBO &palette) {
    if(slot >= SKIN_PALETTE_SLOT_COUNT || GN_Vk.device == VK_NULL_HANDLE) return false;
    if(s_paletteSlots[slot].aBuffer == VK_NULL_HANDLE || s_paletteSlots[slot].aMemory == VK_NULL_HANDLE) {
        return false;
    }

    const VkDeviceSize byteSize = (VkDeviceSize)sizeof(GN3d::sSKIN_UBO);
    void *mapped = nullptr;
    if(vkMapMemory(GN_Vk.device, s_paletteSlots[slot].aMemory, 0, byteSize, 0, &mapped) != VK_SUCCESS ||
       mapped == nullptr) {
        return false;
    }
    std::memcpy(mapped, &palette, (size_t)byteSize);
    vkUnmapMemory(GN_Vk.device, s_paletteSlots[slot].aMemory);
    return true;
}

} // namespace GNg
