// vkAtmosphereLut.cpp — L4：大气 LUT（透射率静态上传 + sky-view GPU compute 按需重建）。
//
// 职责：CPU 生成透射率 LUT（256×64）并静态上传；sky-view LUT（192×108）由
// assets/shaders/gn3d_skyview.comp 在太阳方向/大气参数变化时用 compute 重建。
// 参数化与数值模型必须与 src/graphics/atmosphereParams.cpp 的 computeSkyViewLut /
// computeSkyRadiance 逐 texel 一致（见 docs/L4_L3_CONTRACT.md §1）。
// 本文件不参与天空着色、描述符接线与天气映射；LUT 尺寸固定，不随 resize。
#include <GN.h>
#include "GN_Vk.h"
#include "vkInternal.h"
#include "atmosphereParams.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

namespace GNg {

namespace {

// sky-view compute 的 local_size 与 gn3d_skyview.comp 一致（8×8）。
const uint32_t SKYVIEW_WORKGROUP_SIZE = 8;
// 太阳方向变化阈值：与上次方向点积小于该值即视为变化（契约 §1）。
const float SKYVIEW_DIRTY_DOT = 0.9995f;

// sky-view compute 的 std140 参数 UBO（96 B，6 个 vec4 行）。
// 与 gn3d_skyview.comp 的 AtmosphereParams 逐字段一致。
struct AtmosphereLutUbo {
    float sunDirection[4]; // xyz=单位太阳方向（世界空间 y-up，指向太阳），w=太阳强度
    float radii[4];        // x=地面半径(m)，y=大气顶半径(m)，z=Mie 各向异性 g
    float rayleigh[4];     // xyz=Rayleigh 散射系数(1/m)，w=Rayleigh 尺度高度(m)
    float mie[4];          // x=Mie 散射系数，y=Mie 吸收系数，z=Mie 尺度高度(m)
    float ozone[4];        // xyz=Ozone 吸收系数(1/m)，w=Ozone 帐篷半宽(m)
    float ozoneCenter[4];  // x=Ozone 帐篷中心高度(m)
};
static_assert(sizeof(AtmosphereLutUbo) == 96, "AtmosphereLutUbo 必须为 96 字节（std140）");

VkImage s_transImage = VK_NULL_HANDLE;
VkDeviceMemory s_transMemory = VK_NULL_HANDLE;
VkImageView s_transView = VK_NULL_HANDLE;
VkSampler s_transSampler = VK_NULL_HANDLE;
VkImage s_skyImage = VK_NULL_HANDLE;
VkDeviceMemory s_skyMemory = VK_NULL_HANDLE;
VkImageView s_skyView = VK_NULL_HANDLE;
VkSampler s_skySampler = VK_NULL_HANDLE;

// sky-view compute 资源（描述符布局/管线/私有描述符池/参数 UBO）。缺失时回退到 init 上传的 CPU LUT。
bool s_computeReady = false;
VkDescriptorSetLayout s_setLayout = VK_NULL_HANDLE;
VkPipelineLayout s_pipelineLayout = VK_NULL_HANDLE;
VkPipeline s_pipeline = VK_NULL_HANDLE; // 归 GN_Vk.aPipelineCache
VkDescriptorPool s_descriptorPool = VK_NULL_HANDLE;
VkDescriptorSet s_descriptorSet = VK_NULL_HANDLE;
VkBuffer s_uboBuffer = VK_NULL_HANDLE;
VkDeviceMemory s_uboMemory = VK_NULL_HANDLE;
void *s_uboMapped = nullptr;

// 上次生效的参数/太阳与脏标记（init 上传的 CPU LUT 对应当前默认值与初始太阳）。
GN3d::sATMOSPHERE_PARAMS s_lastParams = {};
float s_lastSunDir[3] = {0.5f, 0.70710678f, 0.5f};
bool s_dirty = false;
bool s_warnedInRenderPass = false;

bool createLutImage(uint32_t width, uint32_t height, VkImageUsageFlags extraUsage,
                    VkImage &image, VkDeviceMemory &memory, VkImageView &view) {
    VkImageCreateInfo ii = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    ii.imageType = VK_IMAGE_TYPE_2D;
    ii.extent = {width, height, 1};
    ii.mipLevels = 1;
    ii.arrayLayers = 1;
    ii.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    ii.tiling = VK_IMAGE_TILING_OPTIMAL;
    ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    ii.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | extraUsage;
    ii.samples = VK_SAMPLE_COUNT_1_BIT;
    ii.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if(vkCreateImage(GN_Vk.device, &ii, 0, &image) != VK_SUCCESS) return false;
    VkMemoryRequirements mr;
    vkGetImageMemoryRequirements(GN_Vk.device, image, &mr);
    VkMemoryAllocateInfo mai = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    mai.allocationSize = mr.size;
    mai.memoryTypeIndex = FindMemType(mr.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if(vkAllocateMemory(GN_Vk.device, &mai, 0, &memory) != VK_SUCCESS) return false;
    vkBindImageMemory(GN_Vk.device, image, memory, 0);

    VkImageViewCreateInfo vi = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    vi.image = image;
    vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vi.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    vi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    vi.subresourceRange.levelCount = 1;
    vi.subresourceRange.layerCount = 1;
    return vkCreateImageView(GN_Vk.device, &vi, 0, &view) == VK_SUCCESS;
}

// wrapU：sky-view LUT 的 u 是方位角（0..1 周期），必须 REPEAT 才能跨 u=0/1 正确插值；
// 透射率 LUT 的 u = mu*0.5+0.5 是非周期量，必须 CLAMP（REPEAT 会把两个物理端点接起来）。
VkSampler createLutSampler(bool wrapU) {
    VkSamplerCreateInfo sci = {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    sci.magFilter = VK_FILTER_LINEAR;
    sci.minFilter = VK_FILTER_LINEAR;
    sci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    sci.addressModeU = wrapU ? VK_SAMPLER_ADDRESS_MODE_REPEAT : VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sci.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    VkSampler sampler = VK_NULL_HANDLE;
    vkCreateSampler(GN_Vk.device, &sci, 0, &sampler);
    return sampler;
}

// 把当前参数写成 std140 UBO 行（与 shader 的 AtmosphereParams 一致）。
void fillAtmosphereLutUbo(AtmosphereLutUbo &ubo, const GN3d::sATMOSPHERE_PARAMS &params,
                          const float sunDir[3]) {
    std::memset(&ubo, 0, sizeof(ubo));
    ubo.sunDirection[0] = sunDir[0];
    ubo.sunDirection[1] = sunDir[1];
    ubo.sunDirection[2] = sunDir[2];
    ubo.sunDirection[3] = params.sunIntensity;
    ubo.radii[0] = params.groundRadius;
    ubo.radii[1] = params.atmosphereRadius;
    ubo.radii[2] = params.mieAnisotropy;
    ubo.rayleigh[0] = params.rayleighScattering[0];
    ubo.rayleigh[1] = params.rayleighScattering[1];
    ubo.rayleigh[2] = params.rayleighScattering[2];
    ubo.rayleigh[3] = params.rayleighScaleHeight;
    ubo.mie[0] = params.mieScattering;
    ubo.mie[1] = params.mieAbsorption;
    ubo.mie[2] = params.mieScaleHeight;
    ubo.ozone[0] = params.ozoneAbsorption[0];
    ubo.ozone[1] = params.ozoneAbsorption[1];
    ubo.ozone[2] = params.ozoneAbsorption[2];
    ubo.ozone[3] = params.ozoneWidth;
    ubo.ozoneCenter[0] = params.ozoneCenterHeight;
}

// 逐字段比较（15 个标量）；用于契约 §1 的脏判据。
bool atmosphereParamsEqual(const GN3d::sATMOSPHERE_PARAMS &a, const GN3d::sATMOSPHERE_PARAMS &b) {
    if(a.groundRadius != b.groundRadius || a.atmosphereRadius != b.atmosphereRadius) return false;
    if(a.rayleighScaleHeight != b.rayleighScaleHeight) return false;
    if(a.mieScattering != b.mieScattering || a.mieAbsorption != b.mieAbsorption) return false;
    if(a.mieScaleHeight != b.mieScaleHeight || a.mieAnisotropy != b.mieAnisotropy) return false;
    if(a.ozoneCenterHeight != b.ozoneCenterHeight || a.ozoneWidth != b.ozoneWidth) return false;
    if(a.sunIntensity != b.sunIntensity) return false;
    for(int c = 0; c < 3; ++c) {
        if(a.rayleighScattering[c] != b.rayleighScattering[c]) return false;
        if(a.ozoneAbsorption[c] != b.ozoneAbsorption[c]) return false;
    }
    return true;
}

// 释放 compute 资源（可重复调用）；管线本体归 GN_Vk.aPipelineCache，这里只清句柄。
void destroySkyViewComputeResources() {
    s_computeReady = false;
    s_dirty = false;
    if(!GN_Vk.device) {
        s_setLayout = VK_NULL_HANDLE;
        s_pipelineLayout = VK_NULL_HANDLE;
        s_pipeline = VK_NULL_HANDLE;
        s_descriptorPool = VK_NULL_HANDLE;
        s_descriptorSet = VK_NULL_HANDLE;
        s_uboBuffer = VK_NULL_HANDLE;
        s_uboMemory = VK_NULL_HANDLE;
        s_uboMapped = nullptr;
        return;
    }
    if(s_uboMapped) vkUnmapMemory(GN_Vk.device, s_uboMemory);
    s_uboMapped = nullptr;
    if(s_descriptorPool) vkDestroyDescriptorPool(GN_Vk.device, s_descriptorPool, 0);
    if(s_uboBuffer) vkDestroyBuffer(GN_Vk.device, s_uboBuffer, 0);
    if(s_uboMemory) vkFreeMemory(GN_Vk.device, s_uboMemory, 0);
    if(s_pipelineLayout) vkDestroyPipelineLayout(GN_Vk.device, s_pipelineLayout, 0);
    if(s_setLayout) vkDestroyDescriptorSetLayout(GN_Vk.device, s_setLayout, 0);
    s_descriptorPool = VK_NULL_HANDLE;
    s_descriptorSet = VK_NULL_HANDLE;
    s_uboBuffer = VK_NULL_HANDLE;
    s_uboMemory = VK_NULL_HANDLE;
    s_pipelineLayout = VK_NULL_HANDLE;
    s_setLayout = VK_NULL_HANDLE;
    s_pipeline = VK_NULL_HANDLE;
}

// 建立 sky-view compute：描述符布局（透射率/输出/UBO）、compute 管线、参数 UBO 与描述符集。
// 失败时调用方保留 init 上传的 CPU LUT（回退），不视为致命错误。
bool createSkyViewComputeResources() {
    if(!GN_Vk.device) return false;

    VkDescriptorSetLayoutBinding bindings[3] = {};
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[2].binding = 2;
    bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[2].descriptorCount = 1;
    bindings[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    VkDescriptorSetLayoutCreateInfo layoutInfo = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    layoutInfo.bindingCount = 3;
    layoutInfo.pBindings = bindings;
    if(vkCreateDescriptorSetLayout(GN_Vk.device, &layoutInfo, 0, &s_setLayout) != VK_SUCCESS) {
        s_setLayout = VK_NULL_HANDLE;
        return false;
    }

    VkPipelineLayoutCreateInfo pipelineInfo = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    pipelineInfo.setLayoutCount = 1;
    pipelineInfo.pSetLayouts = &s_setLayout;
    if(vkCreatePipelineLayout(GN_Vk.device, &pipelineInfo, 0, &s_pipelineLayout) != VK_SUCCESS) {
        s_pipelineLayout = VK_NULL_HANDLE;
        return false;
    }

    std::vector<uint32_t> compSpv = LoadSPIRVFile("assets/shaders/gn3d_skyview.comp.spv");
    if(compSpv.empty()) {
        fprintf(stderr, "[GN_Vk] sky-view 计算着色器加载失败: gn3d_skyview.comp.spv\n");
        return false;
    }
    GN3d::sPIPELINE_DESC desc;
    desc.aKind = GN3d::ePIPE_COMPUTE;
    desc.aCompSpv = compSpv.data();
    desc.aCompSize = compSpv.size() * sizeof(uint32_t);
    desc.aLayout = s_pipelineLayout;
    s_pipeline = GN_Vk.aPipelineCache.GetOrCreate(desc);
    if(!s_pipeline) {
        fprintf(stderr, "[GN_Vk] sky-view 计算管线创建失败: %s\n",
                GN_Vk.aPipelineCache.GetLastError());
        return false;
    }

    // GN_Vk.descriptorPool 未预留 STORAGE_IMAGE 尺寸（GN_Vk.cpp 由 Lead 维护），
    // 因此 compute 用独立小池；也避免与全局池的 maxSets 争用。
    VkDescriptorPoolSize sizes[3] = {{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1},
                                     {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1},
                                     {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1}};
    VkDescriptorPoolCreateInfo poolInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    poolInfo.maxSets = 1;
    poolInfo.poolSizeCount = 3;
    poolInfo.pPoolSizes = sizes;
    if(vkCreateDescriptorPool(GN_Vk.device, &poolInfo, 0, &s_descriptorPool) != VK_SUCCESS) {
        s_descriptorPool = VK_NULL_HANDLE;
        return false;
    }
    VkDescriptorSetAllocateInfo allocInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    allocInfo.descriptorPool = s_descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &s_setLayout;
    if(vkAllocateDescriptorSets(GN_Vk.device, &allocInfo, &s_descriptorSet) != VK_SUCCESS) {
        s_descriptorSet = VK_NULL_HANDLE;
        return false;
    }

    CreateBuf(sizeof(AtmosphereLutUbo), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
              s_uboBuffer, s_uboMemory);
    if(s_uboBuffer == VK_NULL_HANDLE || s_uboMemory == VK_NULL_HANDLE) return false;
    if(vkMapMemory(GN_Vk.device, s_uboMemory, 0, sizeof(AtmosphereLutUbo), 0, &s_uboMapped) !=
       VK_SUCCESS) {
        s_uboMapped = nullptr;
        return false;
    }

    VkDescriptorImageInfo transInfo = {};
    transInfo.sampler = s_transSampler;
    transInfo.imageView = s_transView;
    transInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    VkDescriptorImageInfo skyInfo = {};
    skyInfo.imageView = s_skyView;
    skyInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL; // dispatch 前 barrier 到 GENERAL 写入
    VkDescriptorBufferInfo uboInfo = {s_uboBuffer, 0, sizeof(AtmosphereLutUbo)};
    VkWriteDescriptorSet writes[3] = {{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET},
                                      {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET},
                                      {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET}};
    writes[0].dstSet = s_descriptorSet;
    writes[0].dstBinding = 0;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[0].descriptorCount = 1;
    writes[0].pImageInfo = &transInfo;
    writes[1].dstSet = s_descriptorSet;
    writes[1].dstBinding = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    writes[1].descriptorCount = 1;
    writes[1].pImageInfo = &skyInfo;
    writes[2].dstSet = s_descriptorSet;
    writes[2].dstBinding = 2;
    writes[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[2].descriptorCount = 1;
    writes[2].pBufferInfo = &uboInfo;
    vkUpdateDescriptorSets(GN_Vk.device, 3, writes, 0, 0);
    return true;
}

// sky-view 图像的布局/访问屏障（dispatch 前 GENERAL，之后回 SHADER_READ_ONLY）。
void recordSkyViewImageBarrier(VkCommandBuffer cb, VkImageLayout oldLayout, VkImageLayout newLayout,
                               VkAccessFlags srcAccess, VkAccessFlags dstAccess,
                               VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage) {
    VkImageMemoryBarrier barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = s_skyImage;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = srcAccess;
    barrier.dstAccessMask = dstAccess;
    vkCmdPipelineBarrier(cb, srcStage, dstStage, 0, 0, 0, 0, 0, 1, &barrier);
}

} // namespace

bool init3DAtmosphereLut() {
    if(s_transImage != VK_NULL_HANDLE) return true;
    if(!GN_Vk.device) return false;

    const GN3d::sATMOSPHERE_PARAMS params = GN3d::makeDefaultAtmosphereParams();
    const int tw = GN3d::ATMOSPHERE_TRANSMITTANCE_WIDTH;
    const int th = GN3d::ATMOSPHERE_TRANSMITTANCE_HEIGHT;
    const int sw = GN3d::ATMOSPHERE_SKYVIEW_LUT_WIDTH;
    const int sh = GN3d::ATMOSPHERE_SKYVIEW_LUT_HEIGHT;

    std::vector<float> trans((size_t)tw * (size_t)th * 4u, 0.0f);
    if(!GN3d::computeTransmittanceLut(params, tw, th, trans.data())) return false;
    const float sun[3] = {0.5f, 0.70710678f, 0.5f}; // 初始太阳（GN_VkSetAtmosphere 会替换）
    std::vector<float> sky((size_t)sw * (size_t)sh * 4u, 0.0f);
    if(!GN3d::computeSkyViewLut(params, trans.data(), tw, th, sun, sw, sh, sky.data())) return false;

    // sky-view 图像需要 STORAGE_BIT 供 compute 写入；透射率只做静态上传。
    if(!createLutImage((uint32_t)tw, (uint32_t)th, 0, s_transImage, s_transMemory, s_transView) ||
       !createLutImage((uint32_t)sw, (uint32_t)sh, VK_IMAGE_USAGE_STORAGE_BIT, s_skyImage,
                       s_skyMemory, s_skyView)) {
        destroy3DAtmosphereLut();
        return false;
    }
    s_transSampler = createLutSampler(false); // 透射率：u = mu，非周期 ⇒ CLAMP
    s_skySampler = createLutSampler(true);    // sky-view：u = 方位角，周期 ⇒ REPEAT
    if(!s_transSampler || !s_skySampler) {
        destroy3DAtmosphereLut();
        return false;
    }

    // compute 资源失败只告警：init 上传的 CPU LUT 仍可作为回退（sky-view 不再按需重建）。
    if(createSkyViewComputeResources()) {
        s_computeReady = true;
    } else {
        fprintf(stderr, "[GN_Vk] 警告: sky-view compute 不可用，回退到 init 上传的 CPU LUT\n");
        destroySkyViewComputeResources();
    }

    // 两张 LUT 共用一块 staging（先透射率、再 sky-view），一次提交。
    std::vector<unsigned short> halves(trans.size() + sky.size());
    for(size_t i = 0; i < trans.size(); ++i) halves[i] = GN_ValueFloatToHalf(trans[i]);
    for(size_t i = 0; i < sky.size(); ++i) {
        halves[trans.size() + i] = GN_ValueFloatToHalf(sky[i]);
    }
    const VkDeviceSize transBytes = (VkDeviceSize)trans.size() * 2u;
    const VkDeviceSize skyBytes = (VkDeviceSize)sky.size() * 2u;
    VkBuffer staging = VK_NULL_HANDLE;
    VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
    CreateBuf(transBytes + skyBytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, staging,
              stagingMemory);
    if(staging == VK_NULL_HANDLE) {
        destroy3DAtmosphereLut();
        return false;
    }
    void *mapped = nullptr;
    if(vkMapMemory(GN_Vk.device, stagingMemory, 0, VK_WHOLE_SIZE, 0, &mapped) != VK_SUCCESS) {
        vkDestroyBuffer(GN_Vk.device, staging, 0);
        vkFreeMemory(GN_Vk.device, stagingMemory, 0);
        destroy3DAtmosphereLut();
        return false;
    }
    std::memcpy(mapped, halves.data(), (size_t)(transBytes + skyBytes));
    vkUnmapMemory(GN_Vk.device, stagingMemory);

    VkCommandBuffer cb = beginOnceCommand();
    const VkImage images[2] = {s_transImage, s_skyImage};
    const VkExtent3D extents[2] = {{(uint32_t)tw, (uint32_t)th, 1}, {(uint32_t)sw, (uint32_t)sh, 1}};
    const VkDeviceSize offsets[2] = {0, transBytes};
    for(int k = 0; k < 2; ++k) {
        VkImageMemoryBarrier b = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        b.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        b.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.image = images[k];
        b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        b.subresourceRange.levelCount = 1;
        b.subresourceRange.layerCount = 1;
        b.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             0, 0, 0, 0, 0, 1, &b);
        VkBufferImageCopy region = {};
        region.bufferOffset = offsets[k];
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.layerCount = 1;
        region.imageExtent = extents[k];
        vkCmdCopyBufferToImage(cb, staging, images[k], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                               &region);
        b.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        b.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        b.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, 0, 0, 0, 1, &b);
    }
    const bool ok = submitOnceCommand(cb);
    vkDestroyBuffer(GN_Vk.device, staging, 0);
    vkFreeMemory(GN_Vk.device, stagingMemory, 0);
    if(!ok) {
        destroy3DAtmosphereLut();
        return false;
    }

    // init 上传的 LUT 即对应默认参数与初始太阳：脏标记从 false 开始。
    s_lastParams = params;
    s_lastSunDir[0] = sun[0];
    s_lastSunDir[1] = sun[1];
    s_lastSunDir[2] = sun[2];
    s_dirty = false;
    return true;
}

void destroy3DAtmosphereLut() {
    destroySkyViewComputeResources();
    if(s_transSampler) vkDestroySampler(GN_Vk.device, s_transSampler, 0);
    if(s_skySampler) vkDestroySampler(GN_Vk.device, s_skySampler, 0);
    if(s_transView) vkDestroyImageView(GN_Vk.device, s_transView, 0);
    if(s_skyView) vkDestroyImageView(GN_Vk.device, s_skyView, 0);
    if(s_transImage) vkDestroyImage(GN_Vk.device, s_transImage, 0);
    if(s_skyImage) vkDestroyImage(GN_Vk.device, s_skyImage, 0);
    if(s_transMemory) vkFreeMemory(GN_Vk.device, s_transMemory, 0);
    if(s_skyMemory) vkFreeMemory(GN_Vk.device, s_skyMemory, 0);
    s_transSampler = VK_NULL_HANDLE;
    s_skySampler = VK_NULL_HANDLE;
    s_transView = VK_NULL_HANDLE;
    s_skyView = VK_NULL_HANDLE;
    s_transImage = VK_NULL_HANDLE;
    s_skyImage = VK_NULL_HANDLE;
    s_transMemory = VK_NULL_HANDLE;
    s_skyMemory = VK_NULL_HANDLE;
}

bool isAtmosphereLutReady() { return s_transImage != VK_NULL_HANDLE && s_skyImage != VK_NULL_HANDLE; }

bool GN_VkGetAtmosphereLut(VkImage *transmittance, VkImage *skyView) {
    if(!isAtmosphereLutReady()) return false;
    if(transmittance) *transmittance = s_transImage;
    if(skyView) *skyView = s_skyImage;
    return true;
}

bool GN_VkGetAtmosphereLutViews(VkImageView *transmittanceView, VkImageView *skyView,
                                VkSampler *transmittanceSampler, VkSampler *skySampler) {
    if(!isAtmosphereLutReady()) return false;
    if(!s_transView || !s_skyView || !s_transSampler || !s_skySampler) return false;
    if(transmittanceView) *transmittanceView = s_transView;
    if(skyView) *skyView = s_skyView;
    if(transmittanceSampler) *transmittanceSampler = s_transSampler;
    if(skySampler) *skySampler = s_skySampler;
    return true;
}

void GN_VkSetAtmosphere(const GN3d::sATMOSPHERE_PARAMS &params, const float sunDir[3]) {
    if(!GN_Vk.device || !isAtmosphereLutReady() || !sunDir) return;

    const float lengthSq = sunDir[0] * sunDir[0] + sunDir[1] * sunDir[1] + sunDir[2] * sunDir[2];
    if(!(lengthSq > 1e-12f) || !std::isfinite(lengthSq)) return; // 非法方向：保留上次值
    const float invLength = 1.0f / std::sqrt(lengthSq);
    const float sun[3] = {sunDir[0] * invLength, sunDir[1] * invLength, sunDir[2] * invLength};

    // validate 负责钳制（含太阳强度 >= 0、各系数非负、半径/尺度高度为正）；非法输入用钳制值继续。
    GN3d::sATMOSPHERE_PARAMS fixed;
    GN3d::validateAtmosphereParams(params, &fixed);

    const float lastDot = sun[0] * s_lastSunDir[0] + sun[1] * s_lastSunDir[1] +
                          sun[2] * s_lastSunDir[2];
    if(lastDot >= SKYVIEW_DIRTY_DOT && atmosphereParamsEqual(fixed, s_lastParams)) return;

    s_lastParams = fixed;
    s_lastSunDir[0] = sun[0];
    s_lastSunDir[1] = sun[1];
    s_lastSunDir[2] = sun[2];
    s_dirty = true;
}

void recordAtmosphereLutUpdate(VkCommandBuffer cb) {
    if(!s_computeReady || !s_dirty || !cb) return;
    if(s_inRenderPass) {
        // 契约 §1：必须在 render pass 之外录制；此处保留脏标记，等待合法调用点。
        if(!s_warnedInRenderPass) {
            fprintf(stderr, "[GN_Vk] 警告: recordAtmosphereLutUpdate 在 render pass 内被跳过\n");
            s_warnedInRenderPass = true;
        }
        return;
    }
    if(!s_uboMapped) return;

    AtmosphereLutUbo ubo;
    fillAtmosphereLutUbo(ubo, s_lastParams, s_lastSunDir);
    std::memcpy(s_uboMapped, &ubo, sizeof(ubo)); // HOST_COHERENT：提交时对 GPU 可见

    recordSkyViewImageBarrier(cb, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                              VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_READ_BIT,
                              VK_ACCESS_SHADER_WRITE_BIT,
                              VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
                                  VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                              VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
    const uint32_t groupsX =
        ((uint32_t)GN3d::ATMOSPHERE_SKYVIEW_LUT_WIDTH + SKYVIEW_WORKGROUP_SIZE - 1u) /
        SKYVIEW_WORKGROUP_SIZE;
    const uint32_t groupsY =
        ((uint32_t)GN3d::ATMOSPHERE_SKYVIEW_LUT_HEIGHT + SKYVIEW_WORKGROUP_SIZE - 1u) /
        SKYVIEW_WORKGROUP_SIZE;
    vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_COMPUTE, s_pipeline);
    vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_COMPUTE, s_pipelineLayout, 0, 1,
                            &s_descriptorSet, 0, nullptr);
    vkCmdDispatch(cb, groupsX, groupsY, 1);
    recordSkyViewImageBarrier(cb, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                              VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                              VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                              VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
                                  VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
    s_dirty = false;
}

} // namespace GNg
