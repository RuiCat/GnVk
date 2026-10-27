// vkSkin.cpp — 蒙皮关节调色板 UBO 的创建/销毁骨架（P2-C；不做描述符集、不做绘制）。
//
// 边界：
//   * 仅在 GN_Vk.initialized 且 device 有效时触碰 Vulkan；未初始化返回 false，不崩溃。
//   * 缓冲区为 8KB host-visible/coherent UBO，创建时清零，供 P2 装配阶段每帧 memcpy 调色板。
//   * 本机无 GPU：本文件只做编译验证，创建/销毁的运行期路径不在本任务验证范围内。
#include <GN.h>
#include "GN_3DSkin.h"
#include "GN_Vk.h"
#include "vkInternal.h"

#include <cstring>

namespace GNg {

namespace {

VkBuffer s_skinPaletteBuffer = VK_NULL_HANDLE;
VkDeviceMemory s_skinPaletteMemory = VK_NULL_HANDLE;

} // namespace

// 销毁调色板 UBO；device 已失效时只清句柄（不能对无效 device 调 vkDestroyBuffer）。
void destroySkinPaletteBuffer() {
    if(GN_Vk.initialized && GN_Vk.device != VK_NULL_HANDLE) {
        if(s_skinPaletteBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(GN_Vk.device, s_skinPaletteBuffer, nullptr);
        }
        if(s_skinPaletteMemory != VK_NULL_HANDLE) {
            vkFreeMemory(GN_Vk.device, s_skinPaletteMemory, nullptr);
        }
    }
    s_skinPaletteBuffer = VK_NULL_HANDLE;
    s_skinPaletteMemory = VK_NULL_HANDLE;
}

// 创建并清零关节调色板 UBO。未初始化/创建失败返回 false；已存在视为成功（幂等）。
bool createSkinPaletteBuffer() {
    static_assert(sizeof(GN3d::sSKIN_UBO) == GN3d::SKIN_MAX_JOINTS * 16u * 4u,
                  "sSKIN_UBO 必须为 128 x mat4 = 8KB");
    if(!GN_Vk.initialized || GN_Vk.device == VK_NULL_HANDLE) return false;
    if(s_skinPaletteBuffer != VK_NULL_HANDLE) return true;

    const VkDeviceSize byteSize = (VkDeviceSize)sizeof(GN3d::sSKIN_UBO);
    CreateBuf(byteSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
              s_skinPaletteBuffer, s_skinPaletteMemory);
    if(s_skinPaletteBuffer == VK_NULL_HANDLE || s_skinPaletteMemory == VK_NULL_HANDLE) {
        destroySkinPaletteBuffer();
        return false;
    }

    void *mapped = nullptr;
    if(vkMapMemory(GN_Vk.device, s_skinPaletteMemory, 0, byteSize, 0, &mapped) != VK_SUCCESS ||
       mapped == nullptr) {
        destroySkinPaletteBuffer();
        return false;
    }
    std::memset(mapped, 0, (size_t)byteSize);
    vkUnmapMemory(GN_Vk.device, s_skinPaletteMemory);
    return true;
}

} // namespace GNg
