// vkRayQuery.cpp — 引擎场景加速结构（BLAS/TLAS）构建，供 GI 的 ray query 使用。
//
// 职责：收集世界空间三角形（BeginScene / AddMesh / EndScene），建一层 BLAS + 单实例 TLAS，
// 供 SSGI/SSR 等 GI pass 在 shader 里做 rayQueryEXT。设备扩展已在 GN_VkInit 里按能力启用。
// v1 只支持静态场景：场景变化时 DestroyScene 后重建；AS 与分辨率无关，resize 不需要重建。
//
// 布局契约（与 cornellBox 的 HW RT 一致，避免两套求交漂移）：
//   顶点位置 vec3、顶点法线 vec3、三角形材质 id（每三角形 1 个）。
#include <GN.h>
#include "GN_Vk.h"
#include "vkInternal.h"

#include <cstdio>
#include <cstring>
#include <vector>

namespace GNg {

namespace {

struct sAsFns {
    PFN_vkCreateAccelerationStructureKHR createAs;
    PFN_vkGetAccelerationStructureBuildSizesKHR getSizes;
    PFN_vkCmdBuildAccelerationStructuresKHR cmdBuild;
    PFN_vkGetAccelerationStructureDeviceAddressKHR getAsAddress;
    PFN_vkGetBufferDeviceAddress getBufferAddress;
};

struct sAsBuffer {
    VkBuffer aBuffer;
    VkDeviceMemory aMemory;
};

struct sAsScene {
    bool aCollecting;
    std::vector<float> aPositions; // xyz / 顶点
    std::vector<float> aNormals;   // xyz / 顶点
    std::vector<uint32_t> aIndices;
    std::vector<uint32_t> aMaterials; // / 三角形
    bool aReady;
    VkAccelerationStructureKHR aTlas;
    VkAccelerationStructureKHR aBlas;
    sAsBuffer aVertexBuffer;
    sAsBuffer aNormalBuffer;
    sAsBuffer aIndexBuffer;
    sAsBuffer aMaterialBuffer;
    sAsBuffer aBlasBuffer;
    sAsBuffer aTlasBuffer;
    uint32_t aVertexCount;
    uint32_t aTriangleCount;
};

sAsScene s_scene = {};

bool loadAsFns(sAsFns *fns) {
    fns->createAs = reinterpret_cast<PFN_vkCreateAccelerationStructureKHR>(
        vkGetDeviceProcAddr(GN_Vk.device, "vkCreateAccelerationStructureKHR"));
    fns->getSizes = reinterpret_cast<PFN_vkGetAccelerationStructureBuildSizesKHR>(
        vkGetDeviceProcAddr(GN_Vk.device, "vkGetAccelerationStructureBuildSizesKHR"));
    fns->cmdBuild = reinterpret_cast<PFN_vkCmdBuildAccelerationStructuresKHR>(
        vkGetDeviceProcAddr(GN_Vk.device, "vkCmdBuildAccelerationStructuresKHR"));
    fns->getAsAddress = reinterpret_cast<PFN_vkGetAccelerationStructureDeviceAddressKHR>(
        vkGetDeviceProcAddr(GN_Vk.device, "vkGetAccelerationStructureDeviceAddressKHR"));
    fns->getBufferAddress = reinterpret_cast<PFN_vkGetBufferDeviceAddress>(
        vkGetDeviceProcAddr(GN_Vk.device, "vkGetBufferDeviceAddress"));
    if (!fns->getBufferAddress) {
        fns->getBufferAddress = reinterpret_cast<PFN_vkGetBufferDeviceAddress>(
            vkGetDeviceProcAddr(GN_Vk.device, "vkGetBufferDeviceAddressKHR"));
    }
    return fns->createAs && fns->getSizes && fns->cmdBuild && fns->getAsAddress &&
           fns->getBufferAddress;
}

bool createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props,
                  sAsBuffer *out) {
    VkBufferCreateInfo bci = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bci.size = size;
    bci.usage = usage;
    bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(GN_Vk.device, &bci, nullptr, &out->aBuffer) != VK_SUCCESS) return false;
    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements(GN_Vk.device, out->aBuffer, &req);
    const uint32_t type = FindMemType(req.memoryTypeBits, props);
    VkMemoryAllocateInfo mai = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    mai.allocationSize = req.size;
    mai.memoryTypeIndex = type;
    if (vkAllocateMemory(GN_Vk.device, &mai, nullptr, &out->aMemory) != VK_SUCCESS) return false;
    return vkBindBufferMemory(GN_Vk.device, out->aBuffer, out->aMemory, 0) == VK_SUCCESS;
}

void destroyBuffer(sAsBuffer *b) {
    if (b->aBuffer) vkDestroyBuffer(GN_Vk.device, b->aBuffer, nullptr);
    if (b->aMemory) vkFreeMemory(GN_Vk.device, b->aMemory, nullptr);
    *b = sAsBuffer{VK_NULL_HANDLE, VK_NULL_HANDLE};
}

VkDeviceAddress bufferAddress(const sAsFns &fns, VkBuffer buffer) {
    VkBufferDeviceAddressInfo info = {VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO};
    info.buffer = buffer;
    return fns.getBufferAddress(GN_Vk.device, &info);
}

} // namespace

bool GN_VkRayQueryBeginScene() {
    if (!GN_Vk.rayQuerySupported) return false;
    s_scene.aPositions.clear();
    s_scene.aNormals.clear();
    s_scene.aIndices.clear();
    s_scene.aMaterials.clear();
    s_scene.aReady = false;
    s_scene.aCollecting = true;
    return true;
}

void GN_VkRayQueryAddMesh(const float *positions, const float *normals, const uint32_t *indices,
                          uint32_t vertexCount, uint32_t indexCount, uint32_t materialId) {
    if (!s_scene.aCollecting || !positions || !indices || vertexCount == 0 || indexCount < 3) {
        return;
    }
    const uint32_t base = static_cast<uint32_t>(s_scene.aPositions.size() / 3u);
    s_scene.aPositions.insert(s_scene.aPositions.end(), positions, positions + vertexCount * 3u);
    if (normals) {
        s_scene.aNormals.insert(s_scene.aNormals.end(), normals, normals + vertexCount * 3u);
    } else {
        s_scene.aNormals.insert(s_scene.aNormals.end(), static_cast<size_t>(vertexCount) * 3u, 0.0f);
    }
    for (uint32_t i = 0; i + 2u < indexCount; i += 3u) {
        s_scene.aIndices.push_back(base + indices[i + 0u]);
        s_scene.aIndices.push_back(base + indices[i + 1u]);
        s_scene.aIndices.push_back(base + indices[i + 2u]);
        s_scene.aMaterials.push_back(materialId);
    }
}

bool GN_VkRayQueryEndScene() {
    s_scene.aCollecting = false;
    if (!GN_Vk.rayQuerySupported) return false;
    sAsFns fns = {};
    if (!loadAsFns(&fns)) return false;
    const uint32_t vertexCount = static_cast<uint32_t>(s_scene.aPositions.size() / 3u);
    const uint32_t triangleCount = static_cast<uint32_t>(s_scene.aIndices.size() / 3u);
    if (vertexCount == 0 || triangleCount == 0) return false;

    // 上传顶点/法线/索引/材质
    if (!createBuffer(static_cast<VkDeviceSize>(vertexCount) * 3u * sizeof(float),
                      VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
                          VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                          VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                      &s_scene.aVertexBuffer)) {
        return false;
    }
    if (!createBuffer(static_cast<VkDeviceSize>(vertexCount) * 3u * sizeof(float),
                      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                      &s_scene.aNormalBuffer)) {
        return false;
    }
    if (!createBuffer(static_cast<VkDeviceSize>(triangleCount) * 3u * sizeof(uint32_t),
                      VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
                          VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                      &s_scene.aIndexBuffer)) {
        return false;
    }
    if (!createBuffer(static_cast<VkDeviceSize>(triangleCount) * sizeof(uint32_t),
                      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                      &s_scene.aMaterialBuffer)) {
        return false;
    }
    auto upload = [](sAsBuffer &b, const void *data, size_t bytes) {
        void *mapped = nullptr;
        if (vkMapMemory(GN_Vk.device, b.aMemory, 0, bytes, 0, &mapped) != VK_SUCCESS) return false;
        std::memcpy(mapped, data, bytes);
        vkUnmapMemory(GN_Vk.device, b.aMemory);
        return true;
    };
    if (!upload(s_scene.aVertexBuffer, s_scene.aPositions.data(),
                s_scene.aPositions.size() * sizeof(float)) ||
        !upload(s_scene.aNormalBuffer, s_scene.aNormals.data(),
                s_scene.aNormals.size() * sizeof(float)) ||
        !upload(s_scene.aIndexBuffer, s_scene.aIndices.data(),
                s_scene.aIndices.size() * sizeof(uint32_t)) ||
        !upload(s_scene.aMaterialBuffer, s_scene.aMaterials.data(),
                s_scene.aMaterials.size() * sizeof(uint32_t))) {
        return false;
    }

    // 一次性命令池：建 BLAS → 屏障 → 建 TLAS → 提交等待
    VkCommandPoolCreateInfo pci = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    pci.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    pci.queueFamilyIndex = GN_Vk.graphicsFamily;
    VkCommandPool pool = VK_NULL_HANDLE;
    if (vkCreateCommandPool(GN_Vk.device, &pci, nullptr, &pool) != VK_SUCCESS) return false;
    VkCommandBufferAllocateInfo cai = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    cai.commandPool = pool;
    cai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cai.commandBufferCount = 1;
    VkCommandBuffer cb = VK_NULL_HANDLE;
    vkAllocateCommandBuffers(GN_Vk.device, &cai, &cb);
    VkCommandBufferBeginInfo bi = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cb, &bi);

    // BLAS
    VkAccelerationStructureGeometryKHR geometry = {
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR};
    geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
    geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
    geometry.geometry.triangles.sType =
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
    geometry.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
    geometry.geometry.triangles.vertexData.deviceAddress =
        bufferAddress(fns, s_scene.aVertexBuffer.aBuffer);
    geometry.geometry.triangles.vertexStride = 12;
    geometry.geometry.triangles.maxVertex = vertexCount - 1u;
    geometry.geometry.triangles.indexType = VK_INDEX_TYPE_UINT32;
    geometry.geometry.triangles.indexData.deviceAddress =
        bufferAddress(fns, s_scene.aIndexBuffer.aBuffer);
    VkAccelerationStructureBuildGeometryInfoKHR build = {
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR};
    build.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    build.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    build.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    build.geometryCount = 1;
    build.pGeometries = &geometry;
    VkAccelerationStructureBuildSizesInfoKHR sizes = {
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
    fns.getSizes(GN_Vk.device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &build,
                 &triangleCount, &sizes);
    if (!createBuffer(sizes.accelerationStructureSize,
                      VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
                          VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &s_scene.aBlasBuffer)) {
        return false;
    }
    VkAccelerationStructureCreateInfoKHR ci = {
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR};
    ci.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    ci.size = sizes.accelerationStructureSize;
    ci.buffer = s_scene.aBlasBuffer.aBuffer;
    if (fns.createAs(GN_Vk.device, &ci, nullptr, &s_scene.aBlas) != VK_SUCCESS) return false;
    sAsBuffer blasScratch = {};
    if (!createBuffer(sizes.buildScratchSize,
                      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                          VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &blasScratch)) {
        return false;
    }
    build.dstAccelerationStructure = s_scene.aBlas;
    build.scratchData.deviceAddress = bufferAddress(fns, blasScratch.aBuffer);
    VkAccelerationStructureBuildRangeInfoKHR range = {triangleCount, 0, 0, 0};
    const VkAccelerationStructureBuildRangeInfoKHR *ranges = &range;
    fns.cmdBuild(cb, 1, &build, &ranges);
    VkMemoryBarrier toBuild = {VK_STRUCTURE_TYPE_MEMORY_BARRIER};
    toBuild.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
    toBuild.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
    vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                         VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0, 1, &toBuild,
                         0, nullptr, 0, nullptr);

    // TLAS（单实例）
    sAsBuffer instances = {};
    if (!createBuffer(sizeof(VkAccelerationStructureInstanceKHR),
                      VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
                          VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                      &instances)) {
        return false;
    }
    VkAccelerationStructureInstanceKHR instance = {};
    for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 4; ++c) instance.transform.matrix[r][c] = (r == c) ? 1.0f : 0.0f;
    }
    instance.mask = 0xFF;
    VkAccelerationStructureDeviceAddressInfoKHR ai = {
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR};
    ai.accelerationStructure = s_scene.aBlas;
    instance.accelerationStructureReference = fns.getAsAddress(GN_Vk.device, &ai);
    {
        void *mapped = nullptr;
        vkMapMemory(GN_Vk.device, instances.aMemory, 0, sizeof(instance), 0, &mapped);
        std::memcpy(mapped, &instance, sizeof(instance));
        vkUnmapMemory(GN_Vk.device, instances.aMemory);
    }
    VkAccelerationStructureGeometryKHR instGeom = {
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR};
    instGeom.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    instGeom.geometry.instances.sType =
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
    instGeom.geometry.instances.arrayOfPointers = VK_FALSE;
    instGeom.geometry.instances.data.deviceAddress =
        bufferAddress(fns, instances.aBuffer);
    VkAccelerationStructureBuildGeometryInfoKHR tbuild = {
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR};
    tbuild.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    tbuild.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    tbuild.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    tbuild.geometryCount = 1;
    tbuild.pGeometries = &instGeom;
    uint32_t instanceCount = 1;
    VkAccelerationStructureBuildSizesInfoKHR tsizes = {
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
    fns.getSizes(GN_Vk.device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &tbuild,
                 &instanceCount, &tsizes);
    if (!createBuffer(tsizes.accelerationStructureSize,
                      VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
                          VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &s_scene.aTlasBuffer)) {
        return false;
    }
    VkAccelerationStructureCreateInfoKHR tci = {
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR};
    tci.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    tci.size = tsizes.accelerationStructureSize;
    tci.buffer = s_scene.aTlasBuffer.aBuffer;
    if (fns.createAs(GN_Vk.device, &tci, nullptr, &s_scene.aTlas) != VK_SUCCESS) return false;
    sAsBuffer tlasScratch = {};
    if (!createBuffer(tsizes.buildScratchSize,
                      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                          VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &tlasScratch)) {
        return false;
    }
    tbuild.dstAccelerationStructure = s_scene.aTlas;
    tbuild.scratchData.deviceAddress = bufferAddress(fns, tlasScratch.aBuffer);
    VkAccelerationStructureBuildRangeInfoKHR trange = {instanceCount, 0, 0, 0};
    const VkAccelerationStructureBuildRangeInfoKHR *tranges = &trange;
    fns.cmdBuild(cb, 1, &tbuild, &tranges);
    VkMemoryBarrier toShader = {VK_STRUCTURE_TYPE_MEMORY_BARRIER};
    toShader.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
    toShader.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
    vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
                             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         0, 1, &toShader, 0, nullptr, 0, nullptr);
    vkEndCommandBuffer(cb);

    VkFenceCreateInfo fci = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    VkFence fence = VK_NULL_HANDLE;
    vkCreateFence(GN_Vk.device, &fci, nullptr, &fence);
    VkSubmitInfo si = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
    si.commandBufferCount = 1;
    si.pCommandBuffers = &cb;
    const bool ok = vkQueueSubmit(GN_Vk.graphicsQueue, 1, &si, fence) == VK_SUCCESS &&
                    vkWaitForFences(GN_Vk.device, 1, &fence, VK_TRUE, UINT64_MAX) == VK_SUCCESS;
    vkDestroyFence(GN_Vk.device, fence, nullptr);
    vkDestroyCommandPool(GN_Vk.device, pool, nullptr);
    destroyBuffer(&blasScratch);
    destroyBuffer(&tlasScratch);
    destroyBuffer(&instances);
    destroyBuffer(&s_scene.aIndexBuffer); // 索引只在建 AS 时用
    s_scene.aVertexCount = vertexCount;
    s_scene.aTriangleCount = triangleCount;
    s_scene.aReady = ok && s_scene.aTlas != VK_NULL_HANDLE;
    return s_scene.aReady;
}

bool GN_VkRayQuerySceneReady() {
    return s_scene.aReady;
}

VkAccelerationStructureKHR GN_VkRayQuerySceneTlas() {
    return s_scene.aTlas;
}

void GN_VkRayQueryDestroyScene() {
    if (GN_Vk.device == VK_NULL_HANDLE) {
        s_scene = sAsScene{};
        return;
    }
    const auto destroyAs = reinterpret_cast<PFN_vkDestroyAccelerationStructureKHR>(
        vkGetDeviceProcAddr(GN_Vk.device, "vkDestroyAccelerationStructureKHR"));
    if (destroyAs) {
        if (s_scene.aTlas) destroyAs(GN_Vk.device, s_scene.aTlas, nullptr);
        if (s_scene.aBlas) destroyAs(GN_Vk.device, s_scene.aBlas, nullptr);
    }
    destroyBuffer(&s_scene.aVertexBuffer);
    destroyBuffer(&s_scene.aNormalBuffer);
    destroyBuffer(&s_scene.aIndexBuffer);
    destroyBuffer(&s_scene.aMaterialBuffer);
    destroyBuffer(&s_scene.aBlasBuffer);
    destroyBuffer(&s_scene.aTlasBuffer);
    s_scene = sAsScene{};
}

} // namespace GNg
