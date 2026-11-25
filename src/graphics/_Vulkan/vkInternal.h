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

// 帧首同步纹理环集（定义在 vk3dForward.cpp / vk3dPasses.cpp），必须在命令缓冲开始录制之前调用。
// 三条路径（PBR / 前向 Blinn-Phong / G-Buffer）各自维护按纹理组合分配的私有描述符集：
// 渲染通道内只做绑定，绝不再改写规范集（VUID-vkUpdateDescriptorSets-None-03047）。
GN_VK_INTERNAL void refreshPBRTextureSets();
GN_VK_INTERNAL void refreshForwardTextureSets();
GN_VK_INTERNAL void refreshGBufferTextureSets();
// 帧首调用：把上帧标记的环集重置真正执行（含 vkFreeDescriptorSets）。
GN_VK_INTERNAL void applyRingResetIfRequested();
// 设备销毁前清空三套环集缓存（描述符集随池释放，缓存里的句柄会悬垂）。
GN_VK_INTERNAL void resetTextureSetCaches();
GN_VK_INTERNAL void resetGBufferTextureSetCache();

// 归还一批环集：池开了 FREE_DESCRIPTOR_SET_BIT，必须显式 vkFreeDescriptorSets。
// 三套环集结构不同但都有 aSet 成员，用模板统一。
template <typename T>
void ReleaseRingSets(std::vector<T> &sets, VkDescriptorPool pool, VkDevice device) {
    if(!sets.empty() && pool != VK_NULL_HANDLE) {
        std::vector<VkDescriptorSet> handles;
        handles.reserve(sets.size());
        for(size_t i = 0; i < sets.size(); ++i) handles.push_back(sets[i].aSet);
        vkFreeDescriptorSets(device, pool, (uint32_t)handles.size(), handles.data());
    }
    sets.clear();
}

// ── 环集缓存的两条效率/寿命约束 ─────────────────────────
// 1) 每个环集拷的是「规范集的静态绑定」，而它只在 init / 大气 LUT 接线 / GI-探针更新 /
//    resize 时变化。用代际号跳过绝大多数帧的全量重同步（否则每帧要做
//    N套 × 14 次描述符操作）。代际号由 GuardedUpdateDescriptorSets 在写到规范集时自增。
GN_VK_INTERNAL uint32_t GN_VkCanonicalDescGeneration();
// 2) 纹理解放时视图句柄会失效，环集里缓存的旧句柄必须作废。标记为「下帧帧首重置」，
//    避开「录制中释放描述符集」的时机问题。
GN_VK_INTERNAL void GN_VkRequestTextureSetCacheReset();
// 每套环集的条目上限。32 太小：imoScene 这类真实场景（房间 59 张 + 角色 31 张）
// 需要 90+ 套，超出后整段绘制回退到规范集（默认白图）→ 白模。池容量见 GN_Vk.cpp 的 dps。
const uint32_t kTextureSetCacheCap = 256;

// ═══════════════════════════════════════════════════════
//  描述符改写守卫（VUID-vkUpdateDescriptorSets-None-03047）
//  已录制进命令缓冲并绑定的描述符集不得再被改写，否则命令缓冲进入 invalid state，
//  驱动会丢弃其后的 bind/draw。这里把所有 vkUpdateDescriptorSets 调用改为经过守卫，
//  统计「渲染通道内改写规范集」的次数，由 gpuCheck 断言为 0（可重复回归）。
// ═══════════════════════════════════════════════════════
GN_VK_INTERNAL void GuardedUpdateDescriptorSets(const char *file, int line, VkDevice device,
                                               uint32_t writeCount,
                                               const VkWriteDescriptorSet *writes,
                                               uint32_t copyCount,
                                               const VkCopyDescriptorSet *copies);
#ifndef GN_VK_NO_DESC_GUARD
#define vkUpdateDescriptorSets(device, wc, pw, cc, pc)                                  \
    ::GNg::GuardedUpdateDescriptorSets(__FILE__, __LINE__, (device), (wc), (pw), (cc), (pc))
#endif

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

// 3D 动态缓冲退休表：扩容时旧缓冲先入表，只在 GN_VkBeginFrame 的 vkWaitForFences
// 之后释放。CSM 4 级在同一 command buffer 内录制，pass 内释放会让已录制的 draw
// 引用已销毁显存（真机表现为 amdgpu gfxhub page fault → GPU reset）。
GN_VK_INTERNAL void releaseRetired3DBuffers();

// 阴影 light VP：4 级各自独立的 UBO 槽（每级 64 B 矩阵，槽距 256 B），
// 单槽会被最后一级覆盖，导致 4 级 cascade 全部用同一矩阵。
// 写入走初始化期建立的常驻映射：vkMapMemory 的 offset 还必须满足
// minMemoryMapAlignment（部分驱动 4096），不能按槽偏移临时映射。
GN_VK_INTERNAL VkDescriptorSet shadowCascadeDescriptorSet(uint32_t cascadeIndex);
GN_VK_INTERNAL void writeShadowCascadeLightVP(uint32_t cascadeIndex, const float *matrix16);
GN_VK_INTERNAL void releaseShadowCascadeUbo();

GN_VK_INTERNAL void BatchSwitch(VkDescriptorSet ds);
// 交换链 present mode：GN_VSYNC=0 时优先 IMMEDIATE/MAILBOX，缺省 FIFO。
GN_VK_INTERNAL VkPresentModeKHR ChoosePresentMode(VkPhysicalDevice physical, VkSurfaceKHR surface);
// sIMAGE::DataFill 的持久上传缓存：Begin 返回映射写指针，End 提交并等待。
GN_VK_INTERNAL unsigned char *BeginImageUpload(VkDeviceSize bytes);
GN_VK_INTERNAL bool EndImageUpload(VkImage image, uint32_t width, uint32_t height,
                                   int32_t offsetX, int32_t offsetY, bool fromUndefined);
GN_VK_INTERNAL void DestroyImageUploadCache();
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
GN_VK_INTERNAL void create3DHdrImages(); // L1：HDR 目标须早于 renderPass3D 创建（resolve 目标）
GN_VK_INTERNAL void init3DPost();        // L1：POST UBO + composite binding 2
GN_VK_INTERNAL void destroy3DPost();
GN_VK_INTERNAL bool init3DAtmosphereLut(); // L4c：CPU 大气 LUT → RGBA16F GPU 纹理
GN_VK_INTERNAL void destroy3DAtmosphereLut();
GN_VK_INTERNAL bool isAtmosphereLutReady();
// L4：sky-view LUT 的 GPU compute（参数/太阳变化时重建；须在 render pass 之外录制）
GN_VK_INTERNAL void recordAtmosphereLutUpdate(VkCommandBuffer cb);
// L4：把 LUT 视图/采样器写入天空、PBR、延迟、环境烘焙的描述符集（LUT 就绪后调用一次）
GN_VK_INTERNAL void wireAtmosphereDescriptors();
// L3c CSM：4 层阴影图的按层 framebuffer（cascadeIndex 越界返回层 0）
GN_VK_INTERNAL void destroy3DShadowCascades();
// B2：点光/聚光 cube shadow（单光源，6 面）。size 会钳到 [128, 2048]。
GN_VK_INTERNAL void init3DCubeShadow(uint32_t size);
// cube shadow 第 face 面用的描述符集（绑着该面自己的 light-VP UBO 槽）。
// 未初始化/分配失败返回 VK_NULL_HANDLE。
// slot = lightIndex * 6 + face（0..23）
GN_VK_INTERNAL VkDescriptorSet cubeShadowFaceDescriptorSet(uint32_t slot);
// 写第 face 面的 light-VP 到它自己的 UBO 槽（槽基址 kCubeShadowSlotBase）。
// slot = lightIndex * 6 + face（0..23）
GN_VK_INTERNAL void writeCubeShadowLightVP(uint32_t slot, const float *matrix16);
GN_VK_INTERNAL void destroy3DCubeShadow();
GN_VK_INTERNAL VkFramebuffer shadowFramebufferForCascade(uint32_t cascadeIndex);
GN_VK_INTERNAL bool isPostReady();       // POST UBO 可用才允许合成（避免未绑定 UBO 的 draw）
GN_VK_INTERNAL bool init3DBloom();       // L1d：Bloom mip 链（render pass/管线/描述符/图）
GN_VK_INTERNAL void destroy3DBloom();
GN_VK_INTERNAL bool recreate3DBloomTargets(); // resize：mip 图/fb/管线按新尺寸重建
GN_VK_INTERNAL bool isBloomReady();
GN_VK_INTERNAL void recordBloomPasses(VkCommandBuffer cb); // composite 前录制（pass 之外）
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
GN_VK_INTERNAL void setSsgiEnabled(float strength); // 同步 SSGI UBO 的 shader 开关位
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
// 初始化期一次性命令（帧外上传用）：分配/开始 + 结束/提交/等待/释放。
GN_VK_INTERNAL VkCommandBuffer beginOnceCommand();
GN_VK_INTERNAL bool submitOnceCommand(VkCommandBuffer cb);
GN_VK_INTERNAL uint32_t FindMemType(uint32_t f, VkMemoryPropertyFlags p,
                                    bool preferCached = false);
GN_VK_INTERNAL void CreateBuf(VkDeviceSize sz, VkBufferUsageFlags u,
                              VkMemoryPropertyFlags p, VkBuffer &b, VkDeviceMemory &m,
                              bool preferCached = false);
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
