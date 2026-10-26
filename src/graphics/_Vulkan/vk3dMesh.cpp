// 3D 网格上传与释放：通过 staging buffer 拷贝到 device-local 顶点/索引缓冲。
#include <GN.h>
#include "GN_Vk.h"
#include "vkInternal.h"

#include <cstring>

namespace GNg {

void GN_VkUpload3DMesh(VkBuffer *outVB, VkDeviceMemory *outVM,
                       VkBuffer *outIB, VkDeviceMemory *outIM,
                       const void *vertices, uint32_t vertexCount, uint32_t vertexStride,
                       const uint32_t *indices, uint32_t indexCount) {
    if(!GN_Vk.initialized) return;

    if(vertices && vertexCount > 0) {
        VkDeviceSize sz = (VkDeviceSize)vertexCount * vertexStride;
        VkBuffer staging;
        VkDeviceMemory stagingMem;
        CreateBuf(sz, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                  staging, stagingMem);
        void *d;
        vkMapMemory(GN_Vk.device, stagingMem, 0, sz, 0, &d);
        memcpy(d, vertices, sz);
        vkUnmapMemory(GN_Vk.device, stagingMem);

        CreateBuf(sz, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, *outVB, *outVM);

        VkCommandBuffer cb;
        {
            VkCommandBufferAllocateInfo ai = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
            ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            ai.commandPool = GN_Vk.commandPool;
            ai.commandBufferCount = 1;
            vkAllocateCommandBuffers(GN_Vk.device, &ai, &cb);
        }
        {
            VkCommandBufferBeginInfo bi = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
            bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            vkBeginCommandBuffer(cb, &bi);
        }
        {
            VkBufferCopy rgn = {};
            rgn.size = sz;
            vkCmdCopyBuffer(cb, staging, *outVB, 1, &rgn);
        }
        vkEndCommandBuffer(cb);
        {
            VkSubmitInfo si = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
            si.commandBufferCount = 1;
            si.pCommandBuffers = &cb;
            vkQueueSubmit(GN_Vk.graphicsQueue, 1, &si, VK_NULL_HANDLE);
            vkQueueWaitIdle(GN_Vk.graphicsQueue);
        }
        vkFreeCommandBuffers(GN_Vk.device, GN_Vk.commandPool, 1, &cb);
        vkDestroyBuffer(GN_Vk.device, staging, 0);
        vkFreeMemory(GN_Vk.device, stagingMem, 0);
    }

    if(indices && indexCount > 0) {
        VkDeviceSize sz = indexCount * sizeof(uint32_t);
        VkBuffer staging;
        VkDeviceMemory stagingMem;
        CreateBuf(sz, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                  staging, stagingMem);
        void *d;
        vkMapMemory(GN_Vk.device, stagingMem, 0, sz, 0, &d);
        memcpy(d, indices, sz);
        vkUnmapMemory(GN_Vk.device, stagingMem);

        CreateBuf(sz, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, *outIB, *outIM);

        VkCommandBuffer cb;
        {
            VkCommandBufferAllocateInfo ai = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
            ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            ai.commandPool = GN_Vk.commandPool;
            ai.commandBufferCount = 1;
            vkAllocateCommandBuffers(GN_Vk.device, &ai, &cb);
        }
        {
            VkCommandBufferBeginInfo bi = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
            bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            vkBeginCommandBuffer(cb, &bi);
        }
        {
            VkBufferCopy rgn = {};
            rgn.size = sz;
            vkCmdCopyBuffer(cb, staging, *outIB, 1, &rgn);
        }
        vkEndCommandBuffer(cb);
        {
            VkSubmitInfo si = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
            si.commandBufferCount = 1;
            si.pCommandBuffers = &cb;
            vkQueueSubmit(GN_Vk.graphicsQueue, 1, &si, VK_NULL_HANDLE);
            vkQueueWaitIdle(GN_Vk.graphicsQueue);
        }
        vkFreeCommandBuffers(GN_Vk.device, GN_Vk.commandPool, 1, &cb);
        vkDestroyBuffer(GN_Vk.device, staging, 0);
        vkFreeMemory(GN_Vk.device, stagingMem, 0);
    }
}

void GN_VkRelease3DMesh(VkBuffer vb, VkDeviceMemory vm,
                        VkBuffer ib, VkDeviceMemory im) {
    if(!GN_Vk.initialized) return;
    if(vb) { vkDestroyBuffer(GN_Vk.device, vb, 0); }
    if(vm) { vkFreeMemory(GN_Vk.device, vm, 0); }
    if(ib) { vkDestroyBuffer(GN_Vk.device, ib, 0); }
    if(im) { vkFreeMemory(GN_Vk.device, im, 0); }
}

} // namespace GNg
