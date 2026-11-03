// tests/cornellBox/cornellRtAs.cpp — 三角化 Cornell 场景并构建 BLAS/TLAS（VK_KHR_acceleration_structure）
//
// 职责：把 sCornellGeometry 的 5 个四边形与 2 个球转成三角形网格（球用 UV 球，
// 16×32 段），上传为「位置 / 法线 / 三角形元数据」SSBO，再建一层 BLAS + 单实例 TLAS。
// 着色器按 ray query 命中的 primitive 索引取材质并插值法线；四边形/球与 CPU 几何同源。
#include "cornellRtAs.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <vector>

namespace Cornell {

namespace {

const float kPi = 3.14159265359f;
const int kSphereStacks = 16;
const int kSphereSlices = 32;

struct sTriScene {
    std::vector<float> aPositions;  // 4 floats / 顶点（xyz + 1）
    std::vector<float> aNormals;    // 4 floats / 顶点
    std::vector<unsigned int> aIndices;
    std::vector<unsigned int> aMeta; // 4 uints / 三角形（i0,i1,i2,material）
};

struct sAsFns {
    PFN_vkCreateAccelerationStructureKHR createAs;
    PFN_vkGetAccelerationStructureBuildSizesKHR getSizes;
    PFN_vkCmdBuildAccelerationStructuresKHR cmdBuild;
    PFN_vkGetAccelerationStructureDeviceAddressKHR getAsAddress;
    PFN_vkGetBufferDeviceAddress getBufferAddress;
};

bool loadAsFns(VkDevice device, sAsFns *fns) {
    fns->createAs = reinterpret_cast<PFN_vkCreateAccelerationStructureKHR>(
        vkGetDeviceProcAddr(device, "vkCreateAccelerationStructureKHR"));
    fns->getSizes = reinterpret_cast<PFN_vkGetAccelerationStructureBuildSizesKHR>(
        vkGetDeviceProcAddr(device, "vkGetAccelerationStructureBuildSizesKHR"));
    fns->cmdBuild = reinterpret_cast<PFN_vkCmdBuildAccelerationStructuresKHR>(
        vkGetDeviceProcAddr(device, "vkCmdBuildAccelerationStructuresKHR"));
    fns->getAsAddress = reinterpret_cast<PFN_vkGetAccelerationStructureDeviceAddressKHR>(
        vkGetDeviceProcAddr(device, "vkGetAccelerationStructureDeviceAddressKHR"));
    fns->getBufferAddress = reinterpret_cast<PFN_vkGetBufferDeviceAddress>(
        vkGetDeviceProcAddr(device, "vkGetBufferDeviceAddress"));
    if (!fns->getBufferAddress) {
        fns->getBufferAddress = reinterpret_cast<PFN_vkGetBufferDeviceAddress>(
            vkGetDeviceProcAddr(device, "vkGetBufferDeviceAddressKHR"));
    }
    return fns->createAs && fns->getSizes && fns->cmdBuild && fns->getAsAddress &&
           fns->getBufferAddress;
}

unsigned int addVertex(sTriScene *scene, float x, float y, float z, float nx, float ny, float nz) {
    const unsigned int index = static_cast<unsigned int>(scene->aPositions.size() / 4u);
    const float position[4] = {x, y, z, 1.0f};
    const float normal[4] = {nx, ny, nz, 0.0f};
    scene->aPositions.insert(scene->aPositions.end(), position, position + 4);
    scene->aNormals.insert(scene->aNormals.end(), normal, normal + 4);
    return index;
}

void addTriangle(sTriScene *scene, unsigned int i0, unsigned int i1, unsigned int i2,
                 unsigned int material) {
    const unsigned int meta[4] = {i0, i1, i2, material};
    scene->aIndices.push_back(i0);
    scene->aIndices.push_back(i1);
    scene->aIndices.push_back(i2);
    scene->aMeta.insert(scene->aMeta.end(), meta, meta + 4);
}

void addQuad(sTriScene *scene, const sQuad &q) {
    const sVec3 p0 = q.aP0;
    const sVec3 p1 = {p0.x + q.aU.x, p0.y + q.aU.y, p0.z + q.aU.z};
    const sVec3 p2 = {p1.x + q.aV.x, p1.y + q.aV.y, p1.z + q.aV.z};
    const sVec3 p3 = {p0.x + q.aV.x, p0.y + q.aV.y, p0.z + q.aV.z};
    const float nx = q.aNormal.x;
    const float ny = q.aNormal.y;
    const float nz = q.aNormal.z;
    const unsigned int i0 = addVertex(scene, p0.x, p0.y, p0.z, nx, ny, nz);
    const unsigned int i1 = addVertex(scene, p1.x, p1.y, p1.z, nx, ny, nz);
    const unsigned int i2 = addVertex(scene, p2.x, p2.y, p2.z, nx, ny, nz);
    const unsigned int i3 = addVertex(scene, p3.x, p3.y, p3.z, nx, ny, nz);
    const unsigned int material = static_cast<unsigned int>(q.aMaterial);
    addTriangle(scene, i0, i1, i2, material);
    addTriangle(scene, i0, i2, i3, material);
}

void addSphere(sTriScene *scene, const sSphere &s) {
    const unsigned int material = static_cast<unsigned int>(s.aMaterial);
    const unsigned int base = static_cast<unsigned int>(scene->aPositions.size() / 4u);
    for (int stack = 0; stack <= kSphereStacks; ++stack) {
        const float phi = kPi * static_cast<float>(stack) / static_cast<float>(kSphereStacks);
        const float y = std::cos(phi);
        const float ring = std::sin(phi);
        for (int slice = 0; slice <= kSphereSlices; ++slice) {
            const float theta =
                2.0f * kPi * static_cast<float>(slice) / static_cast<float>(kSphereSlices);
            const float nx = ring * std::sin(theta);
            const float nz = ring * std::cos(theta);
            addVertex(scene, s.aCenter.x + nx * s.aRadius, s.aCenter.y + y * s.aRadius,
                      s.aCenter.z + nz * s.aRadius, nx, y, nz);
        }
    }
    const unsigned int row = static_cast<unsigned int>(kSphereSlices + 1);
    for (int stack = 0; stack < kSphereStacks; ++stack) {
        for (int slice = 0; slice < kSphereSlices; ++slice) {
            const unsigned int a = base + static_cast<unsigned int>(stack) * row +
                                   static_cast<unsigned int>(slice);
            const unsigned int b = a + 1u;
            const unsigned int c = a + row;
            const unsigned int d = c + 1u;
            addTriangle(scene, a, c, b, material);
            addTriangle(scene, b, c, d, material);
        }
    }
}

void buildTriangleScene(const sCornellGeometry &geometry, sTriScene *scene) {
    for (size_t i = 0; i < geometry.aQuads.size(); ++i) addQuad(scene, geometry.aQuads[i]);
    for (size_t i = 0; i < geometry.aSpheres.size(); ++i) addSphere(scene, geometry.aSpheres[i]);
}

VkDeviceAddress bufferAddress(const sAsFns &fns, VkDevice device, VkBuffer buffer) {
    VkBufferDeviceAddressInfo info = {VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO};
    info.buffer = buffer;
    return fns.getBufferAddress(device, &info);
}

bool buildBottomLevel(VkDevice device, VkPhysicalDevice physical, const sAsFns &fns,
                      VkCommandBuffer cmd, VkBuffer vertexBuffer, VkBuffer indexBuffer,
                      uint32_t maxVertex, uint32_t primitiveCount, VkAccelerationStructureKHR *out,
                      sVkBuffer *storage, sVkBuffer *scratchOut) {
    VkAccelerationStructureGeometryKHR geometry = {
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR};
    geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
    geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
    geometry.geometry.triangles.sType =
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
    geometry.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
    geometry.geometry.triangles.vertexData.deviceAddress =
        bufferAddress(fns, device, vertexBuffer);
    geometry.geometry.triangles.vertexStride = 16;
    geometry.geometry.triangles.maxVertex = maxVertex;
    geometry.geometry.triangles.indexType = VK_INDEX_TYPE_UINT32;
    geometry.geometry.triangles.indexData.deviceAddress =
        bufferAddress(fns, device, indexBuffer);

    VkAccelerationStructureBuildGeometryInfoKHR build = {
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR};
    build.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    build.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    build.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    build.geometryCount = 1;
    build.pGeometries = &geometry;
    VkAccelerationStructureBuildSizesInfoKHR sizes = {
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
    fns.getSizes(device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &build,
                 &primitiveCount, &sizes);
    if (!createVkBuffer(device, physical, sizes.accelerationStructureSize,
                        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
                            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, false, storage)) {
        return false;
    }
    VkAccelerationStructureCreateInfoKHR createInfo = {
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR};
    createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    createInfo.size = sizes.accelerationStructureSize;
    createInfo.buffer = storage->aBuffer;
    if (fns.createAs(device, &createInfo, nullptr, out) != VK_SUCCESS) return false;

    sVkBuffer scratch;
    if (!createVkBuffer(device, physical, sizes.buildScratchSize,
                        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, false, &scratch)) {
        return false;
    }
    build.dstAccelerationStructure = *out;
    build.scratchData.deviceAddress = bufferAddress(fns, device, scratch.aBuffer);
    VkAccelerationStructureBuildRangeInfoKHR range = {primitiveCount, 0, 0, 0};
    const VkAccelerationStructureBuildRangeInfoKHR *ranges = &range;
    fns.cmdBuild(cmd, 1, &build, &ranges);
    *scratchOut = scratch; // scratch 必须活到命令提交完成之后
    return true;
}

bool buildTopLevel(VkDevice device, VkPhysicalDevice physical, const sAsFns &fns,
                   VkCommandBuffer cmd, VkAccelerationStructureKHR blas,
                   VkAccelerationStructureKHR *out, sVkBuffer *storage, sVkBuffer *scratchOut,
                   sVkBuffer *instancesOut) {
    sVkBuffer instances;
    if (!createVkBuffer(device, physical, sizeof(VkAccelerationStructureInstanceKHR),
                        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
                            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                        true, &instances)) {
        return false;
    }
    VkAccelerationStructureInstanceKHR instance = {};
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 4; ++col) {
            instance.transform.matrix[row][col] = (row == col) ? 1.0f : 0.0f;
        }
    }
    instance.mask = 0xFF;
    VkAccelerationStructureDeviceAddressInfoKHR addressInfo = {
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR};
    addressInfo.accelerationStructure = blas;
    instance.accelerationStructureReference = fns.getAsAddress(device, &addressInfo);
    std::memcpy(instances.aMapped, &instance, sizeof(instance));

    VkAccelerationStructureGeometryKHR geometry = {
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR};
    geometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    geometry.geometry.instances.sType =
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
    geometry.geometry.instances.arrayOfPointers = VK_FALSE;
    geometry.geometry.instances.data.deviceAddress =
        bufferAddress(fns, device, instances.aBuffer);

    VkAccelerationStructureBuildGeometryInfoKHR build = {
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR};
    build.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    build.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    build.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    build.geometryCount = 1;
    build.pGeometries = &geometry;
    uint32_t primitiveCount = 1;
    VkAccelerationStructureBuildSizesInfoKHR sizes = {
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
    fns.getSizes(device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &build,
                 &primitiveCount, &sizes);
    if (!createVkBuffer(device, physical, sizes.accelerationStructureSize,
                        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
                            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, false, storage)) {
        destroyVkBuffer(device, &instances);
        return false;
    }
    VkAccelerationStructureCreateInfoKHR createInfo = {
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR};
    createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    createInfo.size = sizes.accelerationStructureSize;
    createInfo.buffer = storage->aBuffer;
    if (fns.createAs(device, &createInfo, nullptr, out) != VK_SUCCESS) {
        destroyVkBuffer(device, &instances);
        return false;
    }
    sVkBuffer scratch;
    if (!createVkBuffer(device, physical, sizes.buildScratchSize,
                        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, false, &scratch)) {
        destroyVkBuffer(device, &instances);
        return false;
    }
    build.dstAccelerationStructure = *out;
    build.scratchData.deviceAddress = bufferAddress(fns, device, scratch.aBuffer);
    VkAccelerationStructureBuildRangeInfoKHR range = {primitiveCount, 0, 0, 0};
    const VkAccelerationStructureBuildRangeInfoKHR *ranges = &range;
    fns.cmdBuild(cmd, 1, &build, &ranges);
    *scratchOut = scratch;
    *instancesOut = instances; // 同样要活到提交完成
    return true;
}

bool submitBuild(VkDevice device, VkQueue queue, uint32_t family,
                 const std::function<void(VkCommandBuffer)> &record) {
    VkCommandPoolCreateInfo poolInfo = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    poolInfo.queueFamilyIndex = family;
    VkCommandPool pool = VK_NULL_HANDLE;
    if (vkCreateCommandPool(device, &poolInfo, nullptr, &pool) != VK_SUCCESS) return false;
    VkCommandBufferAllocateInfo allocInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    allocInfo.commandPool = pool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    if (vkAllocateCommandBuffers(device, &allocInfo, &cmd) != VK_SUCCESS) {
        vkDestroyCommandPool(device, pool, nullptr);
        return false;
    }
    VkCommandBufferBeginInfo begin = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &begin);
    record(cmd);
    vkEndCommandBuffer(cmd);
    VkFenceCreateInfo fenceInfo = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    VkFence fence = VK_NULL_HANDLE;
    vkCreateFence(device, &fenceInfo, nullptr, &fence);
    VkSubmitInfo submit = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd;
    bool ok = vkQueueSubmit(queue, 1, &submit, fence) == VK_SUCCESS &&
              vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX) == VK_SUCCESS;
    vkDestroyFence(device, fence, nullptr);
    vkDestroyCommandPool(device, pool, nullptr);
    return ok;
}

} // namespace

bool buildRayQueryScene(VkDevice device, VkPhysicalDevice physical, VkQueue queue,
                        uint32_t queueFamily, const sCornellGeometry &geometry,
                        sRtAsScene *out) {
    if (!out) return false;
    *out = sRtAsScene{};
    sAsFns fns = {};
    if (!loadAsFns(device, &fns)) {
        std::fprintf(stderr, "[cornellRtAs] 加载 AS 扩展函数失败\n");
        return false;
    }
    sTriScene triangles;
    buildTriangleScene(geometry, &triangles);
    const uint32_t vertexCount = static_cast<uint32_t>(triangles.aPositions.size() / 4u);
    const uint32_t triangleCount = static_cast<uint32_t>(triangles.aMeta.size() / 4u);
    if (vertexCount == 0 || triangleCount == 0) return false;

    const VkBufferUsageFlags asInput = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
                                       VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    if (!createVkBuffer(device, physical, triangles.aPositions.size() * sizeof(float),
                        asInput | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                        true, &out->aVertexBuffer)) {
        return false;
    }
    std::memcpy(out->aVertexBuffer.aMapped, triangles.aPositions.data(),
                triangles.aPositions.size() * sizeof(float));
    if (!createVkBuffer(device, physical, triangles.aNormals.size() * sizeof(float),
                        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                        true, &out->aNormalBuffer)) {
        destroyRayQueryScene(device, out);
        return false;
    }
    std::memcpy(out->aNormalBuffer.aMapped, triangles.aNormals.data(),
                triangles.aNormals.size() * sizeof(float));
    if (!createVkBuffer(device, physical, triangles.aMeta.size() * sizeof(unsigned int),
                        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                        true, &out->aTriMetaBuffer)) {
        destroyRayQueryScene(device, out);
        return false;
    }
    std::memcpy(out->aTriMetaBuffer.aMapped, triangles.aMeta.data(),
                triangles.aMeta.size() * sizeof(unsigned int));
    if (!createVkBuffer(device, physical, triangles.aIndices.size() * sizeof(unsigned int),
                        asInput,
                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                        true, &out->aIndexBuffer)) {
        destroyRayQueryScene(device, out);
        return false;
    }
    std::memcpy(out->aIndexBuffer.aMapped, triangles.aIndices.data(),
                triangles.aIndices.size() * sizeof(unsigned int));

    VkAccelerationStructureKHR blas = VK_NULL_HANDLE;
    sVkBuffer blasStorage = {};
    sVkBuffer tlasStorage = {};
    sVkBuffer blasScratch = {};
    sVkBuffer tlasScratch = {};
    sVkBuffer instances = {};
    bool built = submitBuild(device, queue, queueFamily, [&](VkCommandBuffer cmd) {
        if (!buildBottomLevel(device, physical, fns, cmd, out->aVertexBuffer.aBuffer,
                              out->aIndexBuffer.aBuffer, vertexCount - 1u, triangleCount, &blas,
                              &blasStorage, &blasScratch)) {
            return;
        }
        out->aBlas = blas;
        VkMemoryBarrier toBuild = {VK_STRUCTURE_TYPE_MEMORY_BARRIER};
        toBuild.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
        toBuild.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                             VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0, 1,
                             &toBuild, 0, nullptr, 0, nullptr);
        if (!buildTopLevel(device, physical, fns, cmd, blas, &out->aTlas, &tlasStorage,
                           &tlasScratch, &instances)) {
            return;
        }
        VkMemoryBarrier toShader = {VK_STRUCTURE_TYPE_MEMORY_BARRIER};
        toShader.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
        toShader.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &toShader, 0, nullptr, 0,
                             nullptr);
    });
    destroyVkBuffer(device, &blasScratch);
    destroyVkBuffer(device, &tlasScratch);
    destroyVkBuffer(device, &instances);
    if (!built || out->aTlas == VK_NULL_HANDLE) {
        destroyRayQueryScene(device, out);
        return false;
    }
    out->aBlasBuffer = blasStorage;
    out->aTlasBuffer = tlasStorage;
    out->aTriangleCount = triangleCount;
    out->aVertexCount = vertexCount;
    // BLAS/TLAS 的 build 已完成；索引缓冲只在建 AS 时用，可释放。
    destroyVkBuffer(device, &out->aIndexBuffer);
    return true;
}

void destroyRayQueryScene(VkDevice device, sRtAsScene *scene) {
    if (!scene) return;
    // TLAS/BLAS 句柄：按扩展销毁；函数指针在销毁时再取（设备可能已重建）。
    const auto destroyAs = reinterpret_cast<PFN_vkDestroyAccelerationStructureKHR>(
        vkGetDeviceProcAddr(device, "vkDestroyAccelerationStructureKHR"));
    if (destroyAs) {
        if (scene->aTlas != VK_NULL_HANDLE) destroyAs(device, scene->aTlas, nullptr);
        if (scene->aBlas != VK_NULL_HANDLE) destroyAs(device, scene->aBlas, nullptr);
    }
    destroyVkBuffer(device, &scene->aVertexBuffer);
    destroyVkBuffer(device, &scene->aNormalBuffer);
    destroyVkBuffer(device, &scene->aTriMetaBuffer);
    destroyVkBuffer(device, &scene->aIndexBuffer);
    destroyVkBuffer(device, &scene->aBlasBuffer);
    destroyVkBuffer(device, &scene->aTlasBuffer);
    destroyVkBuffer(device, &scene->aInstanceBuffer);
    *scene = sRtAsScene{};
}

} // namespace Cornell
