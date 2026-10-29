// GPU 像素回读：swapchain 用引擎封装，G-Buffer/shadow 用一次性命令缓冲拷贝到主机内存。
#pragma once
#ifndef _GN_GPU_PIXEL_READ_H_
#define _GN_GPU_PIXEL_READ_H_

#include <vulkan/vulkan.h>

#include <cstdint>
#include <vector>

namespace GpuCheck {

// swapchain 像素统一解码为线性 RGBA 浮点：8-bit sRGB 先解码，RGBA16F 直接读 half。
struct sFLOAT_PIXEL {
    float r;
    float g;
    float b;
    float a;
};

// 当前 swapchain 格式的每像素字节数；0 = 验收程序不支持该格式。
uint32_t SwapchainBytesPerPixel();

// 回读当前 swapchain 图像（须在 vkQueueWaitIdle 之后调用）。
bool ReadSwapchain(int x, int y, int width, int height, std::vector<uint8_t> &out);

// 按当前 swapchain 格式把 raw 回读数据解码为线性 RGBA。
sFLOAT_PIXEL SampleSwapchain(const std::vector<uint8_t> &pixels, int width, int x, int y);

// currentLayout 必须是调用时刻该图像的真实布局；函数会恢复同一布局。
bool ReadColorImage(VkImage image, VkFormat format, VkImageLayout currentLayout,
                    uint32_t width, uint32_t height, std::vector<uint8_t> &out);
bool ReadDepthImage(VkImage image, VkImageLayout currentLayout, uint32_t width, uint32_t height,
                    std::vector<float> &out);

float HalfToFloat(uint16_t half);

} // namespace GpuCheck
#endif // _GN_GPU_PIXEL_READ_H_
