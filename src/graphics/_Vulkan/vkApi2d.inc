// GN_mini Vulkan 头片段：初始化/关闭/resize + 2D 绘制 API + scissor。
// 2026-11-26 自 GN_Vk.h 第 485-588 行 move-only 拆出（REFACTOR_PLAN §13 B4）。
// 由 GN_Vk.h 按原顺序包含；不是独立头文件。
// ═══════════════════════════════════════════════════════════
//  Vulkan 初始化 (由 _Main 调用)
// ═══════════════════════════════════════════════════════════
// window 传 0 即走无窗口（离屏）路径：不建 VkSurface、不建 swapchain，渲染到自建图像，
// 之后靠 GN_VkReadBack 回读。此路径不需要任何窗口系统（X11/Wayland 都不用）。
void GN_VkInit(GLFWwindow *window, int w, int h);
void GN_VkShutdown();
// 等待 GPU 空闲。必须在销毁任何可能仍被在飞帧引用的 GPU 资源之前调（例如程序退出时
// 先等空闲、再释放模型 AS/网格）；GN_VkShutdown 内部也会先等，但那时用户资源已释放过了。
void GN_VkWaitIdle();
void GN_VkResize(int w, int h);
// 当前交换链是否为 8-bit sRGB 格式（决定 2D 层是否需要按 sRGB 纹理解码）。
bool GN_VkSwapchainIsSrgb();
// 引擎设备是否支持并已启用硬件光追（VK_KHR_acceleration_structure + VK_KHR_ray_query）。
bool GN_VkRayQuerySupported();

// ── 场景加速结构（GI 用）────────────────────────────────────────
// 用法：BeginScene → 若干 AddMesh(world-space) → EndScene（构建 BLAS/TLAS）→
//       SceneReady() 为 true 后，shader 可用 SceneTlas() 做 ray query。
// v1 只支持静态场景；场景变化需 DestroyScene 后重建。
bool GN_VkRayQueryBeginScene();
void GN_VkRayQueryAddMesh(const float *positions, const float *normals, const uint32_t *indices,
                          uint32_t vertexCount, uint32_t indexCount, uint32_t materialId);
bool GN_VkRayQueryEndScene();
bool GN_VkRayQuerySceneReady();
VkAccelerationStructureKHR GN_VkRayQuerySceneTlas();
void GN_VkRayQueryDestroyScene();
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
// 从内存 PNG 载入纹理。srgb=true 时用 R8G8B8A8_SRGB（颜色贴图；采样即线性，无需 shader 解码），
// wrapU/wrapV 为 VkSamplerAddressMode（glTF sampler.wrapS/wrapT：10497→REPEAT、
// 33071→CLAMP_TO_EDGE、33648→MIRRORED_REPEAT；缺省 CLAMP_TO_EDGE 保持 2D/UI 行为）。
bool ImageLoadFromMem(const void *data, int len, sIMAGE *out,
                      bool srgb = false,
                      VkSamplerAddressMode wrapU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                      VkSamplerAddressMode wrapV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
void ImageSaveToFile(const char *fn, sIMAGE *img);
bool ImageToData(sIMAGE *img, void *out, int fmt);

// 辅助
void VkCheck(VkResult err, const char *msg);

// 设备是否还活着（已初始化且未销毁）。
// 用途：所有“释放 GPU 资源”的入口都要先问一句。对象可能在 GN_VkShutdown 之后才被析构
// （静态/全局对象、AppUnInit 较晚释放的模型），此时拿旧句柄去 vkDestroy* 是未定义行为。
// 返回 false 时调用方应当只把句柄清空、不做任何 GPU 操作。
bool GN_VkDeviceAlive();

// ═══════════════════════════════════════════════════════════
//  Vulkan Scissor 裁剪 (替代 OpenGL glScissor)
// ═══════════════════════════════════════════════════════════
void GN_VkSetScissor(int x, int y, int w, int h, bool enable);

// 帧缓冲回读 (替代 OpenGL glReadPixels → GT_CopyToDataFromFrameBuffer)
uint8_t *GN_VkReadBack(int x, int y, int w, int h, int *size_out);
void GN_VkReadBackFree(uint8_t *data);

// ── 描述符改写体检（vkDescriptorGuard）───────────────────
// 统计 vkUpdateDescriptorSets 总次数，以及其中「渲染通道内改写已绑定规范集」的违规次数。
// 后者必须为 0；gpuCheck 的 desc.noUpdateWhileBound 就是断言它。
uint32_t GN_VkDescriptorUpdatesTotal();
uint32_t GN_VkDescriptorUpdatesWhileBound();
const char *GN_VkDescriptorUpdateSite();    // 首个违规站点（文件路径），无违规为 nullptr
const char *GN_VkDescriptorUpdateSetName(); // 首个违规涉及的规范集名

} // namespace GNg

