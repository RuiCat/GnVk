// vkOnceCmd.cpp — 一次性命令缓冲：分配/开始与结束/提交/等待/释放。
//
// 用途：初始化期的 buffer→image 上传（探针、大气 LUT 等）。调用方必须保证在帧外使用。
#include <GN.h>
#include "GN_Vk.h"
#include "vkInternal.h"

#include <cstdio>

namespace GNg {

VkCommandBuffer beginOnceCommand() {
    VkCommandBuffer cb = VK_NULL_HANDLE;
    VkCommandBufferAllocateInfo ai = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandPool = GN_Vk.commandPool;
    ai.commandBufferCount = 1;
    vkAllocateCommandBuffers(GN_Vk.device, &ai, &cb);
    VkCommandBufferBeginInfo bi = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cb, &bi);
    return cb;
}

bool submitOnceCommand(VkCommandBuffer cb) {
    const VkResult endResult = vkEndCommandBuffer(cb);
    VkFence fence = VK_NULL_HANDLE;
    VkFenceCreateInfo fci = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    const VkResult fenceResult = vkCreateFence(GN_Vk.device, &fci, 0, &fence);
    VkResult submitResult = VK_ERROR_UNKNOWN;
    if(fenceResult == VK_SUCCESS) {
        VkSubmitInfo si = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
        si.commandBufferCount = 1;
        si.pCommandBuffers = &cb;
        submitResult = vkQueueSubmit(GN_Vk.graphicsQueue, 1, &si, fence);
        if(submitResult == VK_SUCCESS) {
            vkWaitForFences(GN_Vk.device, 1, &fence, VK_TRUE, UINT64_MAX);
        }
        vkDestroyFence(GN_Vk.device, fence, 0);
    }
    vkFreeCommandBuffers(GN_Vk.device, GN_Vk.commandPool, 1, &cb);
    if(endResult != VK_SUCCESS || fenceResult != VK_SUCCESS || submitResult != VK_SUCCESS) {
        fprintf(stderr, "[GN_Vk] 一次性命令失败（end=%d fence=%d submit=%d）\n", (int)endResult,
                (int)fenceResult, (int)submitResult);
        return false;
    }
    return true;
}

} // namespace GNg
