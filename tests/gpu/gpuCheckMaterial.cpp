// P1c GPU 材质实例验收：固定 set 0 描述符集 + UBO/纹理绑定 + 材质管线绘制回读。
//
// 覆盖契约 §3 P1c 五条：
//   1) 合法 sMATERIAL_DESC + 键稳定；
//   2) GN_VkMaterialCreate 成功且 aDescriptorSet 非空，重复 Destroy 安全；
//   3) 材质管线非空（Create 成功即其前置条件）、两次绘制不新增管线缓存 miss；
//   4) 全屏四边形回读中心像素 ≈ UBO 颜色（±0.02）；
//   5) 非法描述（binding 0 不是 UBO / 缺 binding 1）返回 false 且不改材料。
#include <GN.h>
#include "GN_Vk.h"
#include "GN_Material.h"

#include "gpuCheckMaterial.h"
#include "gpuCheckReport.h"
#include "gpuPixelRead.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

namespace GpuCheck {
namespace {

const float kColorTolerance = 0.02f;
const uint32_t kQuadIndexCCW[6] = {0, 2, 1, 0, 3, 2};
const uint32_t kTextureSize = 8;

struct sMATERIAL_TEST_RES {
    VkBuffer aUboBuffer;
    VkDeviceMemory aUboMemory;
    VkImage aImage;
    VkDeviceMemory aImageMemory;
    VkImageView aImageView;
    VkSampler aSampler;
    GNg::sMATERIAL_UBO_RESOURCE aUboRes;
    GNg::sMATERIAL_TEX_RESOURCE aTexRes;
};

uint32_t findMemoryTypeIndex(uint32_t typeBits, VkMemoryPropertyFlags props) {
    VkPhysicalDeviceMemoryProperties mp;
    vkGetPhysicalDeviceMemoryProperties(GNg::GN_Vk.physicalDevice, &mp);
    for(uint32_t i = 0; i < mp.memoryTypeCount; ++i) {
        if((typeBits & (1u << i)) && (mp.memoryTypes[i].propertyFlags & props) == props) return i;
    }
    return 0;
}

bool createHostBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                      VkBuffer &buffer, VkDeviceMemory &memory) {
    VkBufferCreateInfo bi = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bi.size = size;
    bi.usage = usage;
    bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if(vkCreateBuffer(GNg::GN_Vk.device, &bi, 0, &buffer) != VK_SUCCESS) return false;
    VkMemoryRequirements mr;
    vkGetBufferMemoryRequirements(GNg::GN_Vk.device, buffer, &mr);
    VkMemoryAllocateInfo ai = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    ai.allocationSize = mr.size;
    ai.memoryTypeIndex = findMemoryTypeIndex(mr.memoryTypeBits,
                                             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                             VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if(vkAllocateMemory(GNg::GN_Vk.device, &ai, 0, &memory) != VK_SUCCESS) return false;
    return vkBindBufferMemory(GNg::GN_Vk.device, buffer, memory, 0) == VK_SUCCESS;
}

// 8×8 纯白纹理：LINEAR tiling + host-visible 内存，写入后转为 SHADER_READ_ONLY_OPTIMAL。
bool createWhiteTexture(sMATERIAL_TEST_RES &res) {
    VkImageCreateInfo ii = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    ii.imageType = VK_IMAGE_TYPE_2D;
    ii.extent = {kTextureSize, kTextureSize, 1};
    ii.mipLevels = 1;
    ii.arrayLayers = 1;
    ii.format = VK_FORMAT_R8G8B8A8_UNORM;
    ii.tiling = VK_IMAGE_TILING_LINEAR;
    ii.initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
    ii.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
    ii.samples = VK_SAMPLE_COUNT_1_BIT;
    ii.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if(vkCreateImage(GNg::GN_Vk.device, &ii, 0, &res.aImage) != VK_SUCCESS) return false;

    VkMemoryRequirements mr;
    vkGetImageMemoryRequirements(GNg::GN_Vk.device, res.aImage, &mr);
    VkMemoryAllocateInfo ai = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    ai.allocationSize = mr.size;
    ai.memoryTypeIndex = findMemoryTypeIndex(mr.memoryTypeBits,
                                             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                             VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if(vkAllocateMemory(GNg::GN_Vk.device, &ai, 0, &res.aImageMemory) != VK_SUCCESS) return false;
    if(vkBindImageMemory(GNg::GN_Vk.device, res.aImage, res.aImageMemory, 0) != VK_SUCCESS) {
        return false;
    }

    // 写入白色像素（按线性 tiling 的 rowPitch 逐行填充）。
    VkImageSubresource sub = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0};
    VkSubresourceLayout layout = {};
    vkGetImageSubresourceLayout(GNg::GN_Vk.device, res.aImage, &sub, &layout);
    void *mapped = 0;
    if(vkMapMemory(GNg::GN_Vk.device, res.aImageMemory, 0, VK_WHOLE_SIZE, 0, &mapped) != VK_SUCCESS) {
        return false;
    }
    unsigned char *base = (unsigned char *)mapped + layout.offset;
    for(uint32_t y = 0; y < kTextureSize; ++y) {
        std::memset(base + (size_t)y * layout.rowPitch, 0xFF, kTextureSize * 4u);
    }
    vkUnmapMemory(GNg::GN_Vk.device, res.aImageMemory);

    // PREINITIALIZED → SHADER_READ_ONLY_OPTIMAL。
    VkCommandBufferAllocateInfo cbai = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    cbai.commandPool = GNg::GN_Vk.commandPool;
    cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cbai.commandBufferCount = 1;
    VkCommandBuffer cb = VK_NULL_HANDLE;
    if(vkAllocateCommandBuffers(GNg::GN_Vk.device, &cbai, &cb) != VK_SUCCESS) return false;
    VkCommandBufferBeginInfo cbbi = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    cbbi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cb, &cbbi);
    VkImageMemoryBarrier barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.oldLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = res.aImage;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         0, 0, 0, 0, 0, 1, &barrier);
    vkEndCommandBuffer(cb);
    VkSubmitInfo si = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
    si.commandBufferCount = 1;
    si.pCommandBuffers = &cb;
    vkQueueSubmit(GNg::GN_Vk.graphicsQueue, 1, &si, VK_NULL_HANDLE);
    vkQueueWaitIdle(GNg::GN_Vk.graphicsQueue);
    vkFreeCommandBuffers(GNg::GN_Vk.device, GNg::GN_Vk.commandPool, 1, &cb);

    VkImageViewCreateInfo vi = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    vi.image = res.aImage;
    vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vi.format = VK_FORMAT_R8G8B8A8_UNORM;
    vi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    vi.subresourceRange.levelCount = 1;
    vi.subresourceRange.layerCount = 1;
    if(vkCreateImageView(GNg::GN_Vk.device, &vi, 0, &res.aImageView) != VK_SUCCESS) return false;

    VkSamplerCreateInfo sci = {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    sci.magFilter = VK_FILTER_LINEAR;
    sci.minFilter = VK_FILTER_LINEAR;
    sci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    sci.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sci.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    return vkCreateSampler(GNg::GN_Vk.device, &sci, 0, &res.aSampler) == VK_SUCCESS;
}

void destroyTestResources(sMATERIAL_TEST_RES &res) {
    VkDevice device = GNg::GN_Vk.device;
    if(!device) return;
    if(res.aSampler) vkDestroySampler(device, res.aSampler, 0);
    if(res.aImageView) vkDestroyImageView(device, res.aImageView, 0);
    if(res.aImage) vkDestroyImage(device, res.aImage, 0);
    if(res.aImageMemory) vkFreeMemory(device, res.aImageMemory, 0);
    if(res.aUboBuffer) vkDestroyBuffer(device, res.aUboBuffer, 0);
    if(res.aUboMemory) vkFreeMemory(device, res.aUboMemory, 0);
    std::memset(&res, 0, sizeof(res));
}

bool createTestResources(sMATERIAL_TEST_RES &res) {
    std::memset(&res, 0, sizeof(res));
    if(!createHostBuffer(16, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, res.aUboBuffer, res.aUboMemory)) {
        return false;
    }
    void *mapped = 0;
    if(vkMapMemory(GNg::GN_Vk.device, res.aUboMemory, 0, 16, 0, &mapped) != VK_SUCCESS) return false;
    const float baseColor[4] = {0.25f, 0.5f, 0.75f, 1.0f};
    std::memcpy(mapped, baseColor, sizeof(baseColor));
    vkUnmapMemory(GNg::GN_Vk.device, res.aUboMemory);

    res.aUboRes.aBuffer = res.aUboBuffer;
    res.aUboRes.aOffset = 0;
    res.aUboRes.aRange = 16;
    if(!createWhiteTexture(res)) return false;
    res.aTexRes.aImageView = res.aImageView;
    res.aTexRes.aSampler = res.aSampler;
    res.aTexRes.aLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    return true;
}

void makeMaterialDesc(const sMATERIAL_TEST_RES &res, GN3d::sMATERIAL_DESC &desc) {
    std::memset(&desc, 0, sizeof(desc));
    desc.aPipelineKey = 0x1ull; // 非 0：由 P1c 固定材质管线承载
    desc.aBindingCount = 2;
    desc.aBindings[0].aBinding = 0;
    desc.aBindings[0].aKind = GN3d::eMAT_BIND_UNIFORM_BUFFER;
    desc.aBindings[0].aStageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    desc.aBindings[0].aResource = &res.aUboRes;
    desc.aBindings[1].aBinding = 1;
    desc.aBindings[1].aKind = GN3d::eMAT_BIND_COMBINED_IMAGE_SAMPLER;
    desc.aBindings[1].aStageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    desc.aBindings[1].aResource = &res.aTexRes;
}

void makeQuad(GNg::VkVertex3D quad[4]) {
    std::memset(quad, 0, 4 * sizeof(GNg::VkVertex3D));
    const float xs[4] = {-1.0f, 1.0f, 1.0f, -1.0f};
    const float ys[4] = {-1.0f, -1.0f, 1.0f, 1.0f};
    for(int i = 0; i < 4; ++i) {
        quad[i].px = xs[i];
        quad[i].py = ys[i];
        quad[i].pz = 0.5f;
        quad[i].nx = 0.0f;
        quad[i].ny = 0.0f;
        quad[i].nz = 1.0f;
        quad[i].tx = 1.0f;
        quad[i].u = (i == 1 || i == 2) ? 1.0f : 0.0f;
        quad[i].v = (i >= 2) ? 1.0f : 0.0f;
    }
}

bool drawMaterialQuad(const GNg::sMAT44 &model, const GN3d::sMATERIAL_RES &material) {
    GNg::VkVertex3D quad[4];
    makeQuad(quad);
    GNg::sMAT44 identity;
    GNg::Mat44Identity(&identity);
    GNg::sVEC3 eye = {0.0f, 0.0f, 1.0f};
    if(!GNg::GN_VkBeginFrame()) return false;
    GNg::GN_VkBegin3DPass(&identity, &eye, 0, 0);
    GNg::GN_VkDraw3DMaterial(quad, 4, kQuadIndexCCW, 6, &model, &material);
    GNg::GN_VkEnd3DPass();
    GNg::GN_VkEndFrame();
    vkQueueWaitIdle(GNg::GN_Vk.graphicsQueue);
    return true;
}

bool readCenterPixel(sFLOAT_PIXEL &out) {
    std::vector<uint8_t> pixels;
    const uint32_t width = GNg::GN_Vk.swapchainExtent.width;
    const uint32_t height = GNg::GN_Vk.swapchainExtent.height;
    if(!ReadSwapchain(0, 0, (int)width, (int)height, pixels)) return false;
    out = SampleSwapchain(pixels, (int)width, (int)(width / 2), (int)(height / 2));
    return true;
}

void reportDescCheck(const GN3d::sMATERIAL_DESC &desc) {
    const uint64_t keyA = GN3d::ComputeMaterialKey(desc);
    const uint64_t keyB = GN3d::ComputeMaterialKey(desc);
    char detail[160];
    std::snprintf(detail, sizeof(detail), "valid=%d key=0x%llx stable=%d",
                  (int)GN3d::ValidateMaterialDesc(desc), (unsigned long long)keyA,
                  (int)(keyA == keyB));
    if(GN3d::ValidateMaterialDesc(desc) && keyA != 0 && keyA == keyB) {
        ReportPass("material.desc", detail);
    } else {
        ReportFail("material.desc", detail);
    }
}

void reportCreateCheck(GN3d::sMATERIAL_RES &material) {
    char detail[128];
    const bool ok = GNg::GN_VkMaterialCreate(&material);
    const bool hasSet = ok && material.aDescriptorSet != 0;
    std::snprintf(detail, sizeof(detail), "create=%d descriptorSet=%p", (int)ok,
                  material.aDescriptorSet);
    if(hasSet) ReportPass("material.create", detail);
    else ReportFail("material.create", detail);
}

void reportInvalidChecks(const GN3d::sMATERIAL_DESC &validDesc) {
    GN3d::sMATERIAL_RES badKind;
    std::memset(&badKind, 0, sizeof(badKind));
    badKind.aDesc = validDesc;
    badKind.aDesc.aBindings[0].aKind = GN3d::eMAT_BIND_COMBINED_IMAGE_SAMPLER;

    GN3d::sMATERIAL_RES missingBinding;
    std::memset(&missingBinding, 0, sizeof(missingBinding));
    missingBinding.aDesc = validDesc;
    missingBinding.aDesc.aBindingCount = 1;

    const bool badKindOk = !GNg::GN_VkMaterialCreate(&badKind) && badKind.aDescriptorSet == 0;
    const bool missingOk =
        !GNg::GN_VkMaterialCreate(&missingBinding) && missingBinding.aDescriptorSet == 0;
    char detail[160];
    std::snprintf(detail, sizeof(detail), "badBinding0Kind=%d missingBinding1=%d untouched=%d",
                  (int)badKindOk, (int)missingOk,
                  (int)(badKind.aDescriptorSet == 0 && missingBinding.aDescriptorSet == 0));
    if(badKindOk && missingOk) ReportPass("material.invalid", detail);
    else ReportFail("material.invalid", detail);
}

void reportCacheCheck(unsigned long long missBefore, unsigned countBefore) {
    const unsigned long long missAfter = GNg::GN_Vk.aPipelineCache.GetMissCount();
    const unsigned countAfter = (unsigned)GNg::GN_Vk.aPipelineCache.GetCount();
    char detail[192];
    std::snprintf(detail, sizeof(detail),
                  "pipeline ready (create ok), 2 draws: miss %llu->%llu count %u->%u",
                  missBefore, missAfter, countBefore, countAfter);
    if(missAfter == missBefore && countAfter == countBefore) ReportPass("material.cache", detail);
    else ReportFail("material.cache", detail);
}

void reportDrawCheck(GN3d::sMATERIAL_RES &material) {
    GNg::sMAT44 model;
    GNg::Mat44Identity(&model);
    unsigned long long missBefore = GNg::GN_Vk.aPipelineCache.GetMissCount();
    unsigned countBefore = (unsigned)GNg::GN_Vk.aPipelineCache.GetCount();
    bool drew = drawMaterialQuad(model, material);
    if(drew) drew = drawMaterialQuad(model, material);
    reportCacheCheck(missBefore, countBefore);
    if(!drew) {
        ReportFail("material.draw", "GN_VkBeginFrame failed");
        return;
    }
    sFLOAT_PIXEL pixel = {0.0f, 0.0f, 0.0f, 0.0f};
    if(!readCenterPixel(pixel)) {
        ReportFail("material.draw", "swapchain readback failed");
        return;
    }
    char detail[160];
    std::snprintf(detail, sizeof(detail), "RGBA=%.3f,%.3f,%.3f,%.3f expected 0.250,0.500,0.750",
                  pixel.r, pixel.g, pixel.b, pixel.a);
    if(std::fabs(pixel.r - 0.25f) <= kColorTolerance &&
       std::fabs(pixel.g - 0.5f) <= kColorTolerance &&
       std::fabs(pixel.b - 0.75f) <= kColorTolerance) {
        ReportPass("material.draw", detail);
    } else {
        ReportFail("material.draw", detail);
    }
}

void reportDestroyCheck(GN3d::sMATERIAL_RES &material) {
    GNg::GN_VkMaterialDestroy(&material);
    const bool firstOk = material.aDescriptorSet == 0;
    GNg::GN_VkMaterialDestroy(&material); // 重复 Destroy 必须安全
    const bool secondOk = material.aDescriptorSet == 0;
    char detail[96];
    std::snprintf(detail, sizeof(detail), "doubleDestroySafe=%d handle=%p", (int)(firstOk && secondOk),
                  material.aDescriptorSet);
    if(firstOk && secondOk) ReportPass("material.destroy", detail);
    else ReportFail("material.destroy", detail);
}

} // namespace

void RunMaterialChecks() {
    if(!GNg::GN_Vk.initialized) {
        ReportSkip("material.setup", "GN_Vk not initialized");
        return;
    }
    sMATERIAL_TEST_RES res;
    if(!createTestResources(res)) {
        ReportFail("material.setup", "UBO/8x8 texture creation failed");
        destroyTestResources(res);
        return;
    }

    GN3d::sMATERIAL_DESC desc;
    makeMaterialDesc(res, desc);
    reportDescCheck(desc);

    GN3d::sMATERIAL_RES material;
    std::memset(&material, 0, sizeof(material));
    material.aDesc = desc;
    material.aKey = GN3d::ComputeMaterialKey(desc);
    reportCreateCheck(material);

    if(material.aDescriptorSet != 0) {
        reportDrawCheck(material);
        reportDestroyCheck(material);
    } else {
        ReportSkip("material.cache", "material create failed");
        ReportSkip("material.draw", "material create failed");
        ReportSkip("material.destroy", "material create failed");
    }
    reportInvalidChecks(desc);

    destroyTestResources(res);
}

} // namespace GpuCheck
