// Vulkan 后端内部共享声明：设备辅助、2D 批状态、3D pass 状态与初始化入口（模块内共享，不对外导出）。
#ifndef GN_VK_INTERNAL_H
#define GN_VK_INTERNAL_H

#include <vulkan/vulkan.h>
#include <cstdint>
#include <vector>

#include "GN_3DclothGPU.h" // P9：布料 SSBO 描述（绘制模块按网格参数生成索引）

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
GN_VK_INTERNAL void init3DEnvironment(); // G1a：GPU 6 面烘焙 → CPU 预滤波 → GI 资源上传
GN_VK_INTERNAL void destroy3DEnvironment();
GN_VK_INTERNAL void init3DGiBindings(); // G1b：GI UBO/预滤波 cubemap/BRDF LUT 描述符槽位
GN_VK_INTERNAL void init3DGbuffer();
GN_VK_INTERNAL void init3DDeferredLight();
GN_VK_INTERNAL void init3DSsao();        // G2b：SSAO 目标/管线/描述符（默认关）
GN_VK_INTERNAL void destroy3DSsao();
GN_VK_INTERNAL void recreate3DSsaoTargets(); // resize：R8 目标/fb/管线 + 描述符重写
GN_VK_INTERNAL bool isSsaoEnabled();         // 读 GI UBO indirect.w（0=关）
GN_VK_INTERNAL void updateSsaoUniforms(const sMAT44 &viewProj);
GN_VK_INTERNAL void recordSsaoPasses(VkCommandBuffer cb);
GN_VK_INTERNAL void init3DSsgi();        // G4：半分辨率 SSGI raw/denoise 资源（默认关）
GN_VK_INTERNAL void destroy3DSsgi();
GN_VK_INTERNAL void recreate3DSsgiTargets(); // resize：半分辨率目标/fb/管线 + 描述符重写
GN_VK_INTERNAL bool isSsgiEnabled();         // 读 GI UBO envParams.w（0=关）
GN_VK_INTERNAL void updateSsgiUniforms(const sMAT44 &viewProj);
GN_VK_INTERNAL void recordSsgiPasses(VkCommandBuffer cb);
GN_VK_INTERNAL void init3DProbes(); // G5b-2：9× 3D 探针纹理 + probe UBO（固定尺寸）
GN_VK_INTERNAL void destroy3DProbes();
GN_VK_INTERNAL void init3DGbufferDebug();        // G0b：四通道 G-Buffer 调试视图
GN_VK_INTERNAL void destroy3DGbufferDebug();
GN_VK_INTERNAL void recreate3DGbufferDebugPipeline(); // resize：固定视口管线按新 extent 重建
// task-8：resize 可重入的 3D 目标/管线 helper（初始化与 GN_VkResize 共用）。
GN_VK_INTERNAL void destroy3DForwardFramebuffers();
GN_VK_INTERNAL void destroy3DMsaaTargets();
GN_VK_INTERNAL bool create3DMsaaTargets();
GN_VK_INTERNAL void create3DForwardFramebuffers();
GN_VK_INTERNAL void recreate3DTargets();
GN_VK_INTERNAL void create3DForwardPipeline(const std::vector<uint32_t> &vertSpv,
                                            const std::vector<uint32_t> &fragSpv);
GN_VK_INTERNAL void create3DPbrPipeline(const std::vector<uint32_t> &pbrVert,
                                        const std::vector<uint32_t> &pbrFrag);
GN_VK_INTERNAL void create3DSkyboxPipeline();
GN_VK_INTERNAL void create3DHdrImages();
GN_VK_INTERNAL void create3DHdrFramebuffers();
GN_VK_INTERNAL void destroy3DHdrTargets();
GN_VK_INTERNAL void recreate3DHdrTargets();
GN_VK_INTERNAL void writeCompositeHdrBinding();
GN_VK_INTERNAL void create3DCompositePipeline();
GN_VK_INTERNAL void recreate3DCompositeTargets();
// task-26：resize 后重建其余固定视口 3D 管线（调试线框/材质/蒙皮/粒子/布料/贴花/降水/云）。
GN_VK_INTERNAL bool recreateDebugLinesPipeline();
GN_VK_INTERNAL bool recreateMaterialPipeline();
GN_VK_INTERNAL bool recreateSkinnedPipeline();
GN_VK_INTERNAL bool recreateParticleDrawPipeline();
GN_VK_INTERNAL bool recreateClothDrawPipeline();
GN_VK_INTERNAL bool recreateDecalPipeline();
GN_VK_INTERNAL bool recreatePrecipitationPipeline();
GN_VK_INTERNAL bool recreateCloudPipeline();
GN_VK_INTERNAL bool recreateSkinnedSsboPipeline(); // task-27：SSBO 蒙皮（>128 关节）固定视口

// ── G-Buffer 附件契约（冻结）──
// 索引 = gn3d_gbuffer.frag 的 MRT location，也等于延迟着色器 binding - 1
// （binding 1=NormalRough、2=AlbedoMetal、3=AO、4=Depth）。见 docs/GI_PLAN.md §5.4。
enum eGBUFFER_ATTACHMENT {
    eGBUFFER_NORMAL_ROUGH = 0, // Normal(RGB, N*0.5+0.5) + Roughness(A)
    eGBUFFER_ALBEDO_METAL = 1, // Albedo(RGB) + Metallic(A)
    eGBUFFER_AO = 2,           // AO(R)，GBA 未用
    eGBUFFER_DEPTH = 3,        // D32 深度
    eGBUFFER_ATTACHMENT_COUNT = 4
};

// G-Buffer（vkGbuffer.cpp）：init 建全部；resize 只重建尺寸相关部分并保留描述符/render pass。
GN_VK_INTERNAL VkImageView gbufferAttachmentView(eGBUFFER_ATTACHMENT a);
GN_VK_INTERNAL void destroy3DGbuffer();
GN_VK_INTERNAL void recreate3DGbufferTargets();

// 引用 swapchainView 或 G-Buffer 视图的 framebuffer/描述符：resize 时必须成对重建。
GN_VK_INTERNAL void destroy3DCompositeFramebuffers();
GN_VK_INTERNAL void create3DCompositeFramebuffers();
GN_VK_INTERNAL void destroy3DDeferredLightFramebuffers();
GN_VK_INTERNAL void create3DDeferredLightTargets();

GN_VK_INTERNAL std::vector<uint32_t> LoadSPIRVFile(const char *path);
GN_VK_INTERNAL uint32_t FindMemType(uint32_t f, VkMemoryPropertyFlags p);
GN_VK_INTERNAL void CreateBuf(VkDeviceSize sz, VkBufferUsageFlags u,
                              VkMemoryPropertyFlags p, VkBuffer &b, VkDeviceMemory &m);
GN_VK_INTERNAL VkImageView MakeImageView(VkImage img, VkFormat fmt);
void VkCheck(VkResult err, const char *msg);

// 蒙皮关节调色板 UBO（P2-C，8KB：128 × mat4）；未初始化/失败返回 false。
GN_VK_INTERNAL bool createSkinPaletteBuffer();
GN_VK_INTERNAL void destroySkinPaletteBuffer();

// P1c / P2 / P8 新增子系统：初始化失败只告警，不影响既有 3D 路径；
// 实现分别位于 vkMaterial.cpp / vkSkinDraw.cpp / vkDebugDraw.cpp。
GN_VK_INTERNAL bool init3DMaterial();
GN_VK_INTERNAL void destroyMaterial();
GN_VK_INTERNAL bool init3DSkinned();
GN_VK_INTERNAL void destroySkinned();
GN_VK_INTERNAL bool init3DDebugLines();
GN_VK_INTERNAL void destroyDebugLines();

// P9 GPU 粒子：模拟（vkParticleCompute.cpp）与绘制（vkParticleDraw.cpp）两半。
// 绘制半在 SSBO 创建后由模拟半调用绑定；解绑先于缓冲销毁。
GN_VK_INTERNAL bool init3DParticle();
GN_VK_INTERNAL void destroyParticle();
GN_VK_INTERNAL bool init3DParticleDraw();
GN_VK_INTERNAL void destroyParticleDraw();
GN_VK_INTERNAL bool particleDrawBindBuffer(VkBuffer buffer, uint32_t capacity);
GN_VK_INTERNAL void particleDrawUnbindBuffer();

// P9 剩余片：GPU 布料（vkClothCompute/vkClothDraw）与 SSBO 蒙皮（vkSkinSsbo）。
// 均为惰性初始化，Lead 在 GN_VkInit3D 接线后同时走 init 路径。
GN_VK_INTERNAL bool init3DCloth();
GN_VK_INTERNAL void destroyCloth();
GN_VK_INTERNAL bool init3DSkinnedSsbo();
GN_VK_INTERNAL void destroySkinnedSsbo();

// 布料绘制模块（vkClothDraw.cpp）从模拟模块取当前 A 缓冲与网格参数。
GN_VK_INTERNAL VkBuffer clothGpuVertexBuffer();
GN_VK_INTERNAL uint32_t clothGpuVertexCount();
GN_VK_INTERNAL const GN3d::sCLOTH_GPU_DESC *clothGpuDesc();
GN_VK_INTERNAL void destroyClothDraw();

// P11-S3：雾 UBO（绑定 前向 3 / PBR 6 / 延迟 8）；由 GN_VkInit3D 最先创建。
GN_VK_INTERNAL bool init3DAtmosphere();
GN_VK_INTERNAL void destroyAtmosphere();
GN_VK_INTERNAL VkBuffer atmosphereFogBuffer();
GN_VK_INTERNAL VkDeviceSize atmosphereFogRange();

} // namespace GNg

#endif // GN_VK_INTERNAL_H
