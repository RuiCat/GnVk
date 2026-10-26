#ifndef _GN_VK_H_
#define _GN_VK_H_
// ╔══════════════════════════════════════════════════════════╗
// ║  GN_mini — Vulkan 后端 (替代 OpenGL/_Graphics)          ║
// ╚══════════════════════════════════════════════════════════╝
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <cstring>
#include <cstdlib>

namespace GNg {
// ═══════════════════════════════════════════════════════════
//  sIMAGE — 图像 (完整定义，替代旧 _Graphics/GN_G2D.h 中的定义)
// ═══════════════════════════════════════════════════════════
struct sIMAGE {
    int width, height;
    float uRange, vRange;
    unsigned int texnum;
    char *fileName;
    bool hasMipmap, add_0000, cloneImg;
    int cFormat; // 颜色格式
    unsigned int *texture; // 纹理ID数组 (Vulkan 中为 VkImageView 索引)

    // Vulkan 资源
    VkImage vkImage;
    VkDeviceMemory vkImageMemory;
    VkImageView vkImageView;
    VkSampler vkSampler;
    VkDescriptorSet vkDescriptorSet;
    bool vkIsLinear; // LINEAR tiling: 零GPU操作直接CPU写入(视频用)
    bool skipDefer; // 跳过延迟上传(FreeType字体atlas等需即时更新)
    void *z__attrb;

    static sIMAGE *zRTexture;

    sIMAGE();
    ~sIMAGE();
    sIMAGE &operator=(const sIMAGE &s);
    sIMAGE(const sIMAGE &s);
    void Release(bool immed = false);
    void InitToData(char *filename, unsigned short imgnum = 1, int fmt = 0,
                    bool mip = false, unsigned char lv = 1);
    void CreateEmptyToData(int w, int h, unsigned short imgnum = 0, int fmt = 0,
                           unsigned char lv = 1, bool mip = false, bool immed = false);
    static bool RenderTargetSupport();
    static void *RenderTargetGet(sIMAGE *img = 0);
    void BeginRender(int msaa = 0, bool is3d = false, bool isnew = false);
    void EndRender();
    void CopyOut(sIMAGE *dst);
    P<sIMAGE> CopyOut(int x, int y, int w, int h, bool alphaOnly = false, bool flipY = false);
    P<sIMAGE> CopyOut();
    void DataFill(uint8 *data = 0, int fmt = 0, int w = 0, int h = 0, int sw = 0, int sh = 0, unsigned short *texid = 0, bool mip = false);
    uint8 *DataGet(int *w = 0, int *h = 0, int *fmt = 0);
    uint8 *DataGetEx(int x, int y, int w, int h, unsigned short *fmt = 0, bool flip = false);
    uint8 *DataGet(int *w, short &h, short &x, short &y, short &rw, unsigned short &fmt, bool flip);
};

// 补充绘制函数
void ImageDraw4P(sIMAGE *img, int x, int y, int w, int h,
                 unsigned int color0, unsigned int color1,
                 unsigned int color2, unsigned int color3,
                 unsigned short blend = 0);
void ImageDraw4P(sIMAGE *img, float *vt, unsigned int color, unsigned short texID,
                 unsigned short blend, float u1, float v1, float u2, float v2);
void ImageDrawEx(sIMAGE *img, int x, int y, int w, int h,
                 unsigned int color, unsigned short blend,
                 float angle = 0, float cx = 0.5f, float cy = 0.5f);
void ImageDrawDitherBlur(sIMAGE *img, int x, int y, int w, int h,
                         unsigned int color, unsigned short tid,
                         unsigned short blend, int blur, float dither);
void Draw2DLine(sIMAGE *img, float x1, float y1, float x2, float y2,
                float width, float center);
void TextDraw(void *textArt, const char *text, int x, int y, int w, int h);

// ── 前向声明 ──────────────────────────────────────────────
struct sIMAGE;
class cVIEW2D;
struct sRECT;
struct sVEC2;
struct sDRAW_RANGE;
struct sVEC3;
struct sMAT44;

// sIMAGE 定义在 GN_2D.h 中，这里只提供 Vulkan 后端实现

// cVIEW2D 定义在 GN_2D.h 中，这里只提供 Vulkan 后端实现

// ═══════════════════════════════════════════════════════════
//  3D 顶点结构 (GN_USING_3D 启用时编译)
// ═══════════════════════════════════════════════════════════
#ifdef GN_USING_3D
struct VkVertex3D {
    float px, py, pz; // 位置
    float nx, ny, nz; // 法线
    float tx, ty, tz; // 切线 (预留)
    float u, v; // UV 坐标
};

// 3D 场景 UBO (每帧更新一次)
struct VkSceneUBO {
    float viewProj[16]; // 摄像机 VP 矩阵 (列主序)
    float cameraPos[4]; // 摄像机世界位置 (w=padding)
    float ambientLight[4]; // 环境光 RGB + 强度
};

// 3D 模型 Push Constant (每个模型不同)
// ── Blinn-Phong 材质布局 (着色器: gn3d.vert/frag) ──
struct alignas(16) VkModelPushConstBP {
    float model[16]; // 模型矩阵 (列主序) — 64 bytes
    float diffuse[4]; // 漫反射 RGBA — 16 bytes
    float specular[4]; // 镜面反射 RGB + shininess — 16 bytes
}; // 96 bytes total, 16-byte aligned

// ── PBR 材质布局 (着色器: gn3d_pbr.vert/frag) ──
// 注意: 与 Blinn-Phong 共用同一 Push Constant 空间但语义不同
struct alignas(16) VkModelPushConstPBR {
    float model[16]; // 模型矩阵 (列主序)
    float albedo[4]; // Albedo RGB + _pad
    float pbrParams[4]; // x=roughness, y=metallic, z=ao, w=_pad
}; // 96 bytes total, 16-byte aligned

// 向后兼容别名 (Blinn-Phong 是默认)
typedef VkModelPushConstBP VkModelPushConst;
#endif // GN_USING_3D

// ═══════════════════════════════════════════════════════════
//  Vulkan 全局状态 (引擎初始化时设置)
// ═══════════════════════════════════════════════════════════

// ═══════════════════════════════════════════════════════════
//  兼容类型 (替代旧 _Graphics 中的定义)
// ═══════════════════════════════════════════════════════════
// sWINDOW 定义在 GN_2Dtype.h 中

struct GN_VkContext {
    VkInstance instance;
    VkPhysicalDevice physicalDevice;
    VkDevice device;
    VkQueue graphicsQueue;
    uint32_t graphicsFamily;
    VkSurfaceKHR surface;
    VkSwapchainKHR swapchain;
    VkFormat swapchainFormat;
    VkExtent2D swapchainExtent;
    uint32_t swapchainImageCount;
    VkImage *swapchainImages;
    VkImageView *swapchainViews;
    VkFramebuffer *framebuffers;
    VkRenderPass renderPass;
    VkPipelineLayout pipelineLayout;
    VkPipeline pipeline;
    VkPipeline pipelineBright; // additive blend for eBLEND_BRIGHT
    VkPipeline pipelineMultiply; // multiply blend for eBLEND_MODULATE
    VkPipeline pipelineNull; // no blend for eBLEND_NULL
    VkCommandPool commandPool;
    VkCommandBuffer *commandBuffers;
    VkDescriptorPool descriptorPool;
    VkDescriptorSetLayout descriptorLayout;
    VkBuffer uniformBuffer;
    VkDeviceMemory uniformMemory;
    VkBuffer vertexBuffer;
    VkDeviceMemory vertexMemory;
    uint32_t vertexCount;
    uint32_t vertexCapacity;
    VkBuffer indexBuffer;
    VkDeviceMemory indexMemory;
    uint32_t indexCount;
    uint32_t indexCapacity;
    VkSampler defaultSampler;
    VkImage defaultImage; // 1×1 白色默认纹理图像
    VkDeviceMemory defaultImageMemory; // 1×1 白色默认纹理内存
    VkImageView defaultImageView; // 1×1 白色默认纹理视图 (纯色/无纹理绘制降级)
    VkDescriptorSet defaultDS;
    VkSemaphore imageAvailable;
    VkSemaphore renderFinished;
    VkFence inFlightFence;
    uint32_t currentFrame;
    bool initialized;
    int viewWidth, viewHeight;

#ifdef GN_USING_3D
    // — MSAA —
    VkSampleCountFlagBits msaaSamples; // 多重采样数 (默认 4×)

    // — 深度缓冲 (MSAA) —
    VkImage depthImage;
    VkDeviceMemory depthMemory;
    VkImageView depthView;
    VkFormat depthFormat;

    // — MSAA 颜色缓冲 (resolve target = swapchain image) —
    VkImage msaaColorImage;
    VkDeviceMemory msaaColorMemory;
    VkImageView msaaColorView;

    // — 3D 管线 —
    VkRenderPass renderPass3D;
    VkPipelineLayout pipelineLayout3D;
    VkPipeline pipeline3D;
    VkDescriptorSetLayout descriptorLayout3D;
    VkDescriptorSet descriptorSet3D;

    // — 3D UBO —
    VkBuffer uniformBuffer3D;
    VkDeviceMemory uniformMemory3D;
    VkBuffer lightBuffer3D;
    VkDeviceMemory lightMemory3D;

    // — PBR 管线 —
    VkPipelineLayout pipelineLayoutPBR;
    VkPipeline pipelinePBR;
    VkDescriptorSetLayout descriptorLayoutPBR;
    VkDescriptorSet descriptorSetPBR; // PBR 专用描述符集 (含 normal + shadow)

    // — 阴影映射 —
    VkImage shadowImage;
    VkDeviceMemory shadowMemory;
    VkImageView shadowView;
    VkSampler shadowSampler;
    VkRenderPass shadowRenderPass;
    VkFramebuffer shadowFramebuffer;
    VkPipelineLayout shadowPipelineLayout;
    VkPipeline shadowPipeline;
    VkDescriptorSetLayout shadowDescriptorLayout;
    VkDescriptorSet shadowDescriptorSet;
    VkBuffer shadowUBOBuffer;
    VkDeviceMemory shadowUBOMemory;
    uint32_t shadowMapSize; // 默认 2048

    // — 3D 顶点/索引缓冲 —
    VkBuffer vertexBuffer3D;
    VkDeviceMemory vertexMemory3D;
    VkBuffer indexBuffer3D;
    VkDeviceMemory indexMemory3D;

    // — HDR 离屏目标 + 合成管线 —
    VkImage hdrImage;
    VkDeviceMemory hdrMemory;
    VkImageView hdrView;
    VkSampler hdrSampler;
    VkRenderPass hdrRenderPass;
    VkFramebuffer *hdrFramebuffers;
    // 合成管线 (HDR→Swapchain: 色调映射+Bloom+Gamma)
    VkPipelineLayout compositePipelineLayout;
    VkPipeline compositePipeline;
    VkDescriptorSetLayout compositeDescriptorLayout;
    VkDescriptorSet compositeDescriptorSet;
    VkRenderPass compositeRenderPass;
    VkFramebuffer *compositeFramebuffers;

    // — 天空盒 —
    VkImage skyboxImage;
    VkDeviceMemory skyboxMemory;
    VkImageView skyboxView;
    VkSampler skyboxSampler;
    VkPipelineLayout skyboxPipelineLayout;
    VkPipeline skyboxPipeline;
    VkDescriptorSetLayout skyboxDescriptorLayout;
    VkDescriptorSet skyboxDescriptorSet;
    VkBuffer skyboxUBOBuffer;
    VkDeviceMemory skyboxUBOMemory;

    // — 延迟渲染 G-Buffer —
    VkImage gAlbedoImage, gNormalImage, gPBRImage, gDepthImage;
    VkDeviceMemory gAlbedoMemory, gNormalMemory, gPBRMemory, gDepthMemory;
    VkImageView gAlbedoView, gNormalView, gPBRView, gDepthView;
    VkSampler gBufferSampler;
    VkRenderPass gBufferRenderPass;
    VkFramebuffer *gBufferFramebuffers;
    // G-Buffer 管线
    VkPipelineLayout gBufferPipelineLayout;
    VkPipeline gBufferPipeline;
    VkDescriptorSetLayout gBufferDescriptorLayout;
    VkDescriptorSet gBufferDescriptorSet;
    // 延迟光照管线
    VkPipelineLayout deferredLightPipelineLayout;
    VkPipeline deferredLightPipeline;
    VkDescriptorSetLayout deferredLightDescriptorLayout;
    VkDescriptorSet deferredLightDescriptorSet;
    VkRenderPass deferredLightRenderPass;
    VkFramebuffer *deferredLightFramebuffers;
#endif

    GN_VkContext() : initialized(false), swapchainImages(0), swapchainViews(0),
                     framebuffers(0), commandBuffers(0),
                     defaultImage(VK_NULL_HANDLE), defaultImageMemory(VK_NULL_HANDLE),
                     defaultImageView(VK_NULL_HANDLE), defaultDS(VK_NULL_HANDLE),
                     vertexCapacity(4096), indexCapacity(6144),
                     vertexCount(0), indexCount(0)
#ifdef GN_USING_3D
                     ,
                     msaaSamples(VK_SAMPLE_COUNT_4_BIT), depthImage(VK_NULL_HANDLE), depthMemory(VK_NULL_HANDLE), depthView(VK_NULL_HANDLE), depthFormat(VK_FORMAT_D32_SFLOAT), msaaColorImage(VK_NULL_HANDLE), msaaColorMemory(VK_NULL_HANDLE), msaaColorView(VK_NULL_HANDLE), renderPass3D(VK_NULL_HANDLE), pipelineLayout3D(VK_NULL_HANDLE), pipeline3D(VK_NULL_HANDLE), descriptorLayout3D(VK_NULL_HANDLE), descriptorSet3D(VK_NULL_HANDLE), uniformBuffer3D(VK_NULL_HANDLE), uniformMemory3D(VK_NULL_HANDLE), lightBuffer3D(VK_NULL_HANDLE), lightMemory3D(VK_NULL_HANDLE), pipelineLayoutPBR(VK_NULL_HANDLE), pipelinePBR(VK_NULL_HANDLE), descriptorLayoutPBR(VK_NULL_HANDLE), descriptorSetPBR(VK_NULL_HANDLE), shadowImage(VK_NULL_HANDLE), shadowMemory(VK_NULL_HANDLE), shadowView(VK_NULL_HANDLE), shadowSampler(VK_NULL_HANDLE), shadowRenderPass(VK_NULL_HANDLE), shadowFramebuffer(VK_NULL_HANDLE), shadowPipelineLayout(VK_NULL_HANDLE), shadowPipeline(VK_NULL_HANDLE), shadowDescriptorLayout(VK_NULL_HANDLE), shadowDescriptorSet(VK_NULL_HANDLE), shadowUBOBuffer(VK_NULL_HANDLE), shadowUBOMemory(VK_NULL_HANDLE), shadowMapSize(2048), vertexBuffer3D(VK_NULL_HANDLE), vertexMemory3D(VK_NULL_HANDLE), indexBuffer3D(VK_NULL_HANDLE), indexMemory3D(VK_NULL_HANDLE), hdrImage(VK_NULL_HANDLE), hdrMemory(VK_NULL_HANDLE), hdrView(VK_NULL_HANDLE), hdrSampler(VK_NULL_HANDLE), hdrRenderPass(VK_NULL_HANDLE), hdrFramebuffers(nullptr), compositePipelineLayout(VK_NULL_HANDLE), compositePipeline(VK_NULL_HANDLE), compositeDescriptorLayout(VK_NULL_HANDLE), compositeDescriptorSet(VK_NULL_HANDLE), compositeRenderPass(VK_NULL_HANDLE), compositeFramebuffers(nullptr), skyboxImage(VK_NULL_HANDLE), skyboxMemory(VK_NULL_HANDLE), skyboxView(VK_NULL_HANDLE), skyboxSampler(VK_NULL_HANDLE), skyboxPipelineLayout(VK_NULL_HANDLE), skyboxPipeline(VK_NULL_HANDLE), skyboxDescriptorLayout(VK_NULL_HANDLE), skyboxDescriptorSet(VK_NULL_HANDLE), skyboxUBOBuffer(VK_NULL_HANDLE), skyboxUBOMemory(VK_NULL_HANDLE), gAlbedoImage(VK_NULL_HANDLE), gNormalImage(VK_NULL_HANDLE), gPBRImage(VK_NULL_HANDLE), gDepthImage(VK_NULL_HANDLE), gAlbedoMemory(VK_NULL_HANDLE), gNormalMemory(VK_NULL_HANDLE), gPBRMemory(VK_NULL_HANDLE), gDepthMemory(VK_NULL_HANDLE), gAlbedoView(VK_NULL_HANDLE), gNormalView(VK_NULL_HANDLE), gPBRView(VK_NULL_HANDLE), gDepthView(VK_NULL_HANDLE), gBufferSampler(VK_NULL_HANDLE), gBufferRenderPass(VK_NULL_HANDLE), gBufferFramebuffers(nullptr), gBufferPipelineLayout(VK_NULL_HANDLE), gBufferPipeline(VK_NULL_HANDLE), gBufferDescriptorLayout(VK_NULL_HANDLE), gBufferDescriptorSet(VK_NULL_HANDLE), deferredLightPipelineLayout(VK_NULL_HANDLE), deferredLightPipeline(VK_NULL_HANDLE), deferredLightDescriptorLayout(VK_NULL_HANDLE), deferredLightDescriptorSet(VK_NULL_HANDLE), deferredLightRenderPass(VK_NULL_HANDLE), deferredLightFramebuffers(nullptr)
#endif
    {
    }
};

extern GN_VkContext GN_Vk;

// ═══════════════════════════════════════════════════════════
//  Vulkan 初始化 (由 _Main 调用)
// ═══════════════════════════════════════════════════════════
void GN_VkInit(GLFWwindow *window, int w, int h);
void GN_VkShutdown();
void GN_VkResize(int w, int h);
bool GN_VkBeginFrame();
void GN_VkEndFrame();

// ═══════════════════════════════════════════════════════════
//  2D 绘制 API (替代旧的 OpenGL 绘制)
// ═══════════════════════════════════════════════════════════
void ImageDraw(sIMAGE *img, int x, int y, int w = 0, int h = 0,
               unsigned int color = 0xffffffff, unsigned short texID = 0,
               unsigned short blendmode = 0,
               float u1 = 0.0f, float v1 = 0.0f, float u2 = 0.0f, float v2 = 0.0f);
void ImageDrawRot(sIMAGE *img, int x, int y, int w, int h,
                  float angle, float cx, float cy,
                  unsigned int color, int frame_id);
void ImageDrawMP(sIMAGE *img, sDRAW_RANGE *ranges, int count,
                 unsigned int color, unsigned short blend);
void LineFrame2dDraw(int x, int y, int w, int h, int lw,
                     unsigned int color = 0xffffffff, unsigned short blend = 0);
bool ImageLoad(const char *file, unsigned short num,
               sIMAGE *out, int fmt = 0);
void SetAlphaColorKey(unsigned int color);
void Draw2DAlphaDirectionSet(unsigned char dir = 0);
unsigned char Draw2DAlphaDirectionGet();
void RenderFinish(bool only3d = false);

// 更多绘制/图像 API
P<sIMAGE> ImageLoad(const char *fn, unsigned short num = 1, int fmt = 0);
bool ImageLoad(const char *fn, unsigned short num, sIMAGE *out, int fmt);
bool ImageLoadEx(const char *fn, sIMAGE *out, int w = 0, int h = 0);
void ImageDraw9Square(sIMAGE *img9, int x, int y, int w, int h,
                      unsigned int color, unsigned short blend, uint8 repeat);
void ImageDrawMaskRect(sIMAGE *img, int x, int y, int w, int h,
                       unsigned int color, unsigned short texID,
                       unsigned short blend, const void *rects, unsigned short n = 1);
void ImageDrawRng(sIMAGE *img, int x, int y, int w, int h,
                  unsigned int color, unsigned short texID,
                  unsigned short blend, float scl, int ntype);
void ImageDrawFull(sIMAGE *img, int x, int y, int w, int h,
                   unsigned int color, unsigned short texID,
                   unsigned short blend, float u1, float v1, float u2, float v2);
bool ImageLoadFromMem(const void *data, int len, sIMAGE *out);
void ImageSaveToFile(const char *fn, sIMAGE *img);
bool ImageToData(sIMAGE *img, void *out, int fmt);

// 辅助
void VkCheck(VkResult err, const char *msg);

// ═══════════════════════════════════════════════════════════
//  Vulkan Scissor 裁剪 (替代 OpenGL glScissor)
// ═══════════════════════════════════════════════════════════
void GN_VkSetScissor(int x, int y, int w, int h, bool enable);

// 帧缓冲回读 (替代 OpenGL glReadPixels → GT_CopyToDataFromFrameBuffer)
uint8_t *GN_VkReadBack(int x, int y, int w, int h, int *size_out);
void GN_VkReadBackFree(uint8_t *data);

} // namespace GNg

// ═══════════════════════════════════════════════════════════
//  3D 渲染 API (GN_USING_3D 启用时)
// ═══════════════════════════════════════════════════════════
#ifdef GN_USING_3D
namespace GNg {

// 初始化 3D 资源 (由 GN_VkInit 在 2D 管线创建后调用)
void GN_VkInit3D();

// 关闭 3D 资源 (由 GN_VkShutdown 调用)
void GN_VkShutdown3D();

// 开始 3D 渲染 pass (切换到 3D 管线, 上传场景 UBO 和光照 UBO)
void GN_VkBegin3DPass(const sMAT44 *viewProj, const sVEC3 *cameraPos,
                      const void *lightUBO, uint32_t lightUBOSize);

// 结束 3D 渲染 pass (切换回 2D 管线)
void GN_VkEnd3DPass();

// 提交 3D 模型绘制 (PBR 模式: 使用 roughness/metallic/ao, 支持纹理/法线贴图)
void GN_VkDraw3DPBR(const void *vertices, uint32_t vertexCount,
                    const uint32_t *indices, uint32_t indexCount,
                    const sMAT44 *modelMatrix,
                    float diffuseR, float diffuseG, float diffuseB,
                    float roughness = 0.5f, float metallic = 0.0f, float ao = 1.0f,
                    const sIMAGE *albedoTex = nullptr, const sIMAGE *normalTex = nullptr);

// 提交 3D 模型绘制 (Blinn-Phong 模式, 支持纹理)
void GN_VkDraw3D(const void *vertices, uint32_t vertexCount,
                 const uint32_t *indices, uint32_t indexCount,
                 const sMAT44 *modelMatrix,
                 float diffuseR, float diffuseG, float diffuseB,
                 float specularR = 1.0f, float specularG = 1.0f, float specularB = 1.0f,
                 float shininess = 32.0f, const sIMAGE *texture = nullptr);

// 上传 3D 模型数据到 GPU
void GN_VkUpload3DMesh(VkBuffer *outVB, VkDeviceMemory *outVM,
                       VkBuffer *outIB, VkDeviceMemory *outIM,
                       const void *vertices, uint32_t vertexCount, uint32_t vertexStride,
                       const uint32_t *indices, uint32_t indexCount);

// 释放 3D 模型 GPU 资源
void GN_VkRelease3DMesh(VkBuffer vb, VkDeviceMemory vm,
                        VkBuffer ib, VkDeviceMemory im);

// — PBR 渲染 —
void GN_VkBegin3DPassPBR(const sMAT44 *viewProj, const sVEC3 *cameraPos,
                         const void *lightUBO, uint32_t lightUBOSize);
// PBR HDR 变体 — 渲染到 HDR 目标供后续 GN_VkCompositeToSwapchain 合成
void GN_VkBegin3DPassPBR_HDR(const sMAT44 *viewProj, const sVEC3 *cameraPos,
                             const void *lightUBO, uint32_t lightUBOSize);

// — 阴影映射 —
void GN_VkShadowPassBegin(const sMAT44 *lightViewProj);
void GN_VkShadowPassEnd();
void GN_VkShadowDraw(const void *vertices, uint32_t vertexCount,
                     const uint32_t *indices, uint32_t indexCount,
                     const sMAT44 *modelMatrix);

// — HDR 合成 (Bloom + 色调映射) —
// 在 3D Pass 之后调用, 将 HDR 内容合成到 swapchain
void GN_VkCompositeToSwapchain();

// — 天空盒 —
void GN_VkDrawSkybox(const sMAT44 *viewProj);

// — 延迟渲染 —
void GN_VkBegin3DPassDeferred(const sMAT44 *viewProj, const sVEC3 *cameraPos,
                              const void *lightUBO, uint32_t lightUBOSize);
void GN_VkDraw3D_GBuffer(const void *vertices, uint32_t vertexCount,
                         const uint32_t *indices, uint32_t indexCount,
                         const sMAT44 *modelMatrix,
                         float albedoR, float albedoG, float albedoB,
                         float roughness = 0.5f, float metallic = 0.0f, float ao = 1.0f,
                         const sIMAGE *albedoTex = nullptr, const sIMAGE *normalTex = nullptr);
void GN_VkDeferredLighting(const sMAT44 *viewProj, const sVEC3 *cameraPos,
                           const void *lightUBO, uint32_t lightUBOSize);

} // namespace GNg
#endif // GN_USING_3D

#endif // _GN_VK_H_
