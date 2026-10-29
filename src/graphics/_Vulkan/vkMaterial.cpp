// vkMaterial.cpp — P1c GPU 材质实例：固定布局描述符集创建/写入与材质绘制。
//
// 契约：
//   * 固定 set 0 布局：binding 0 = UNIFORM_BUFFER（sMATERIAL_UBO_RESOURCE），
//     binding 1 = COMBINED_IMAGE_SAMPLER（sMATERIAL_TEX_RESOURCE）；描述里缺任一绑定即拒绝。
//   * 描述符集从 GN_Vk.descriptorPool 分配；释放只清句柄，UBO/纹理归调用方。
//   * 绘制走本模块自建的材质管线（aPipelineCache 持有），model 用 64 字节 push constant。
#include <GN.h>
#include "GN_Vk.h"
#include "vkInternal.h"

#include <cstddef>
#include <cstdio>

namespace GNg {

namespace {

// 材质管线的 GPU 对象；管线本身归 GN_Vk.aPipelineCache 所有，这里只留句柄。
VkDescriptorSetLayout s_materialSetLayout = VK_NULL_HANDLE;
VkPipelineLayout s_materialPipelineLayout = VK_NULL_HANDLE;
VkPipeline s_materialPipeline = VK_NULL_HANDLE;

// 固定 set 0 布局：binding 0 = UBO，binding 1 = 组合图像采样器。
// 只接受恰好这两个 binding；顺序、种类与 ValidateMaterialDesc 的通用校验都通过才算合法。
bool isSupportedMaterialDesc(const GN3d::sMATERIAL_DESC &desc) {
    if(!GN3d::ValidateMaterialDesc(desc)) return false;
    if(desc.aBindingCount != 2) return false;
    const GN3d::sMATERIAL_BINDING &ubo = desc.aBindings[0];
    const GN3d::sMATERIAL_BINDING &tex = desc.aBindings[1];
    if(ubo.aBinding != 0 || ubo.aKind != GN3d::eMAT_BIND_UNIFORM_BUFFER) return false;
    if(tex.aBinding != 1 || tex.aKind != GN3d::eMAT_BIND_COMBINED_IMAGE_SAMPLER) return false;
    return true;
}

bool createMaterialPipeline(const std::vector<uint32_t> &vertSpv,
                            const std::vector<uint32_t> &fragSpv) {
    // ── 固定 set 0 布局 ──────────────────────────────────
    VkDescriptorSetLayoutBinding dslb[2] = {};
    dslb[0].binding = 0;
    dslb[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    dslb[0].descriptorCount = 1;
    dslb[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    dslb[1].binding = 1;
    dslb[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    dslb[1].descriptorCount = 1;
    dslb[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    VkDescriptorSetLayoutCreateInfo dslci = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    dslci.bindingCount = 2;
    dslci.pBindings = dslb;
    if(vkCreateDescriptorSetLayout(GN_Vk.device, &dslci, 0, &s_materialSetLayout) != VK_SUCCESS) {
        s_materialSetLayout = VK_NULL_HANDLE;
        return false;
    }

    // ── 管线布局：64B model 矩阵 push constant ───────────
    VkPushConstantRange pcr = {};
    pcr.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pcr.offset = 0;
    pcr.size = 64;
    VkPipelineLayoutCreateInfo plci = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    plci.setLayoutCount = 1;
    plci.pSetLayouts = &s_materialSetLayout;
    plci.pushConstantRangeCount = 1;
    plci.pPushConstantRanges = &pcr;
    if(vkCreatePipelineLayout(GN_Vk.device, &plci, 0, &s_materialPipelineLayout) != VK_SUCCESS) {
        s_materialPipelineLayout = VK_NULL_HANDLE;
        return false;
    }

    // ── 材质图形管线（经 P1b 管线缓存创建）───────────────
    GN3d::sVERTEX_LAYOUT layout;
    layout.Add(0, (uint32_t)offsetof(VkVertex3D, px), VK_FORMAT_R32G32B32_SFLOAT);
    layout.Add(1, (uint32_t)offsetof(VkVertex3D, u), VK_FORMAT_R32G32_SFLOAT);
    layout.aStride = (uint32_t)sizeof(VkVertex3D);

    GN3d::sPIPELINE_DESC desc;
    desc.aKind = GN3d::ePIPE_GRAPHICS;
    desc.aVertSpv = vertSpv.data();
    desc.aVertSize = vertSpv.size() * sizeof(uint32_t);
    desc.aFragSpv = fragSpv.data();
    desc.aFragSize = fragSpv.size() * sizeof(uint32_t);
    desc.aVertexLayout = layout;
    desc.aRenderPass = GN_Vk.renderPass3D;
    desc.aSubpass = 0;
    desc.aLayout = s_materialPipelineLayout;
    desc.aDepthTest = true;
    desc.aDepthWrite = true;
    desc.aDepthCompare = VK_COMPARE_OP_LESS;
    desc.aDepthStencil = true;
    desc.aCullBack = true;
    desc.aFrontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE; // 世界 CCW + 正高度视口 + Y 翻转
    desc.aBlend = GN3d::eBLEND_NONE;
    desc.aTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    desc.aSamples = GN_Vk.msaaSamples;
    desc.aColorAttachmentCount = 1;
    desc.aDynamicViewport = false;
    desc.aDynamicScissor = false;
    desc.aViewport = VkViewport{0.0f, 0.0f, (float)GN_Vk.swapchainExtent.width,
                                (float)GN_Vk.swapchainExtent.height, 0.0f, 1.0f};
    desc.aScissor = VkRect2D{{0, 0}, GN_Vk.swapchainExtent};
    desc.aCustomBlend = false;
    s_materialPipeline = GN_Vk.aPipelineCache.GetOrCreate(desc);
    if(!s_materialPipeline) {
        fprintf(stderr, "[GN_Vk] 材质管线创建失败: %s\n", GN_Vk.aPipelineCache.GetLastError());
        return false;
    }
    return true;
}

} // namespace

bool init3DMaterial() {
    if(s_materialPipeline != VK_NULL_HANDLE) return true;
    if(!GN_Vk.device || !GN_Vk.renderPass3D) return false;

    std::vector<uint32_t> vertSpv = LoadSPIRVFile("assets/shaders/gn3d_material.vert.spv");
    std::vector<uint32_t> fragSpv = LoadSPIRVFile("assets/shaders/gn3d_material.frag.spv");
    if(vertSpv.empty() || fragSpv.empty()) {
        fprintf(stderr, "[GN_Vk] 材质着色器加载失败: gn3d_material.vert/frag.spv\n");
        return false;
    }
    if(!createMaterialPipeline(vertSpv, fragSpv)) {
        destroyMaterial();
        return false;
    }
    return true;
}

void destroyMaterial() {
    if(GN_Vk.device) {
        // 管线由管线缓存统一销毁，这里只释放本模块自建的布局。
        if(s_materialPipelineLayout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(GN_Vk.device, s_materialPipelineLayout, 0);
            s_materialPipelineLayout = VK_NULL_HANDLE;
        }
        if(s_materialSetLayout != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(GN_Vk.device, s_materialSetLayout, 0);
            s_materialSetLayout = VK_NULL_HANDLE;
        }
    }
    s_materialPipeline = VK_NULL_HANDLE;
}

bool GN_VkMaterialCreate(GN3d::sMATERIAL_RES *material) {
    if(!material || !GN_Vk.initialized || !GN_Vk.descriptorPool) return false;
    if(s_materialPipeline == VK_NULL_HANDLE || s_materialSetLayout == VK_NULL_HANDLE) return false;
    if(!isSupportedMaterialDesc(material->aDesc)) return false; // 失败时不改 material（含 aDescriptorSet）
    if(material->aDescriptorSet != 0) return true; // 已创建：幂等，避免重复分配

    const sMATERIAL_UBO_RESOURCE *ubo =
        (const sMATERIAL_UBO_RESOURCE *)material->aDesc.aBindings[0].aResource;
    const sMATERIAL_TEX_RESOURCE *tex =
        (const sMATERIAL_TEX_RESOURCE *)material->aDesc.aBindings[1].aResource;
    if(!ubo || !ubo->aBuffer || ubo->aRange == 0) return false;
    if(!tex || !tex->aImageView || !tex->aSampler) return false;

    VkDescriptorSet set = VK_NULL_HANDLE;
    VkDescriptorSetAllocateInfo dai = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    dai.descriptorPool = GN_Vk.descriptorPool;
    dai.descriptorSetCount = 1;
    dai.pSetLayouts = &s_materialSetLayout;
    if(vkAllocateDescriptorSets(GN_Vk.device, &dai, &set) != VK_SUCCESS) return false;

    VkDescriptorBufferInfo dbi = {};
    dbi.buffer = ubo->aBuffer;
    dbi.offset = ubo->aOffset;
    dbi.range = ubo->aRange;
    VkDescriptorImageInfo dii = {};
    dii.sampler = tex->aSampler;
    dii.imageView = tex->aImageView;
    dii.imageLayout = tex->aLayout;
    VkWriteDescriptorSet writes[2] = {{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET},
                                      {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET}};
    writes[0].dstSet = set;
    writes[0].dstBinding = 0;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[0].descriptorCount = 1;
    writes[0].pBufferInfo = &dbi;
    writes[1].dstSet = set;
    writes[1].dstBinding = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[1].descriptorCount = 1;
    writes[1].pImageInfo = &dii;
    vkUpdateDescriptorSets(GN_Vk.device, 2, writes, 0, 0);

    material->aDescriptorSet = (void *)set;
    return true;
}

void GN_VkMaterialDestroy(GN3d::sMATERIAL_RES *material) {
    if(!material) return;
    // 池带 FREE_DESCRIPTOR_SET_BIT（GN_Vk.cpp），可单独归还；清句柄保证可重复调用。
    if(material->aDescriptorSet && GN_Vk.device && GN_Vk.descriptorPool) {
        VkDescriptorSet set = (VkDescriptorSet)material->aDescriptorSet;
        vkFreeDescriptorSets(GN_Vk.device, GN_Vk.descriptorPool, 1, &set);
    }
    material->aDescriptorSet = 0;
}

void GN_VkDraw3DMaterial(const void *vertices, uint32_t vertexCount,
                         const uint32_t *indices, uint32_t indexCount,
                         const sMAT44 *modelMatrix, const GN3d::sMATERIAL_RES *material) {
    if(!GN_Vk.initialized || !s_in3DPass) return;
    if(!vertices || vertexCount == 0 || !modelMatrix || !material) return;
    if(s_materialPipeline == VK_NULL_HANDLE || s_materialPipelineLayout == VK_NULL_HANDLE) return;
    VkDescriptorSet set = (VkDescriptorSet)material->aDescriptorSet;
    if(set == VK_NULL_HANDLE) return;

    const uint32_t vtxSize = vertexCount * (uint32_t)sizeof(VkVertex3D);
    const uint32_t idxSize = (indices && indexCount > 0) ? indexCount * (uint32_t)sizeof(uint32_t) : 0;
    Ensure3DBufferCapacity(vtxSize, idxSize);

    void *mapped = 0;
    vkMapMemory(GN_Vk.device, GN_Vk.vertexMemory3D, s_vtxOfs3D, vtxSize, 0, &mapped);
    memcpy(mapped, vertices, vtxSize);
    vkUnmapMemory(GN_Vk.device, GN_Vk.vertexMemory3D);

    VkCommandBuffer cb = GN_Vk.commandBuffers[GN_Vk.currentFrame];
    vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, s_materialPipeline);
    vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, s_materialPipelineLayout,
                            0, 1, &set, 0, 0);

    VkDeviceSize offs[] = {s_vtxOfs3D};
    vkCmdBindVertexBuffers(cb, 0, 1, &GN_Vk.vertexBuffer3D, offs);
    s_vtxOfs3D += vtxSize;

    vkCmdPushConstants(cb, s_materialPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT,
                       0, 64, modelMatrix->m);

    if(indices && indexCount > 0) {
        vkMapMemory(GN_Vk.device, GN_Vk.indexMemory3D, s_idxOfs3D, idxSize, 0, &mapped);
        memcpy(mapped, indices, idxSize);
        vkUnmapMemory(GN_Vk.device, GN_Vk.indexMemory3D);
        vkCmdBindIndexBuffer(cb, GN_Vk.indexBuffer3D, s_idxOfs3D, VK_INDEX_TYPE_UINT32);
        s_idxOfs3D += idxSize;
        vkCmdDrawIndexed(cb, indexCount, 1, 0, 0, 0);
    } else {
        vkCmdDraw(cb, vertexCount, 1, 0, 0);
    }
}

} // namespace GNg
