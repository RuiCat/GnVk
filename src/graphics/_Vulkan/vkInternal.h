// Vulkan 后端内部共享声明：设备辅助、2D 批状态、3D pass 状态与初始化入口（模块内共享，不对外导出）。
#ifndef GN_VK_INTERNAL_H
#define GN_VK_INTERNAL_H

#include <vulkan/vulkan.h>
#include <cstdint>
#include <vector>

#if defined(__GNUC__)
#define GN_VK_INTERNAL __attribute__((visibility("hidden")))
#else
#define GN_VK_INTERNAL
#endif

namespace GNg {

struct sIMAGE;

// 2D 顶点格式（vkFrame2d/vkDraw2d 共享）
struct VkVertex {
    float x, y;
    float u, v;
    float r, g, b, a;
};

// 2D 投影 UBO
struct VkUBO {
    float proj[16];
};

// pass 内延迟纹理上传项
struct sPendingUpload {
    sIMAGE *img;
    unsigned char *data;
    int w, h, fmt;
};

GN_VK_INTERNAL extern VkVertex *gVBuf;
GN_VK_INTERNAL extern uint32_t gVMax;
GN_VK_INTERNAL extern uint32_t gVCnt;
GN_VK_INTERNAL extern VkDescriptorSet s_lastDS;
GN_VK_INTERNAL extern VkDescriptorSet s_batchDS[256];
GN_VK_INTERNAL extern uint32_t s_batchEnd[256];
GN_VK_INTERNAL extern uint32_t s_batchCount;
GN_VK_INTERNAL extern bool s_inRenderPass;
GN_VK_INTERNAL extern sPendingUpload s_pendingUploads[256];
GN_VK_INTERNAL extern int s_pendingUploadCnt;

// 3D pass 状态（vk3dPasses 与 GN_Vk.cpp 共享）
GN_VK_INTERNAL extern bool s_in3DPass;
GN_VK_INTERNAL extern bool s_inShadowPass;
GN_VK_INTERNAL extern VkFramebuffer *s_framebuffers3D;
GN_VK_INTERNAL extern VkDeviceSize s_vtxOfs3D;
GN_VK_INTERNAL extern VkDeviceSize s_idxOfs3D;

GN_VK_INTERNAL void BatchSwitch(VkDescriptorSet ds);
GN_VK_INTERNAL void FlushBatchIfScissorDirty();
GN_VK_INTERNAL void GrowVBufIfNeeded(uint32_t need);
GN_VK_INTERNAL void FlushVertexBatch();
GN_VK_INTERNAL void Ensure3DBufferCapacity(uint32_t vtxSize, uint32_t idxSize);
GN_VK_INTERNAL void init3DTargets();
GN_VK_INTERNAL void init3DForward(const std::vector<uint32_t> &vertSpv,
                                  const std::vector<uint32_t> &fragSpv);
GN_VK_INTERNAL void init3DPbr(const std::vector<uint32_t> &pbrVert,
                              const std::vector<uint32_t> &pbrFrag);
GN_VK_INTERNAL void init3DShadow(const std::vector<uint32_t> &shadowVertSpv);
GN_VK_INTERNAL void init3DHdr();
GN_VK_INTERNAL void init3DSkybox();
GN_VK_INTERNAL void init3DGbuffer();
GN_VK_INTERNAL void init3DDeferredLight();

GN_VK_INTERNAL std::vector<uint32_t> LoadSPIRVFile(const char *path);
GN_VK_INTERNAL uint32_t FindMemType(uint32_t f, VkMemoryPropertyFlags p);
GN_VK_INTERNAL void CreateBuf(VkDeviceSize sz, VkBufferUsageFlags u,
                              VkMemoryPropertyFlags p, VkBuffer &b, VkDeviceMemory &m);
GN_VK_INTERNAL VkImageView MakeImageView(VkImage img, VkFormat fmt);
void VkCheck(VkResult err, const char *msg);

} // namespace GNg

#endif // GN_VK_INTERNAL_H
