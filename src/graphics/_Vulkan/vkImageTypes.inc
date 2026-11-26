// GN_mini Vulkan 头片段：sIMAGE 完整定义 + 3D 顶点/UBO/push constant 类型。
// 2026-11-26 自 GN_Vk.h 第 43-189 行 move-only 拆出（REFACTOR_PLAN §13 B4）。
// 由 GN_Vk.h 按原顺序包含；不是独立头文件。
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
    bool vkSrgb;     // true: 纹理格式用 R8G8B8A8_SRGB（硬件解码 sRGB→线性；3D 颜色贴图应开启，法线/ORM 贴图保持 false）
    VkSamplerAddressMode vkWrapU; // 采样寻址 U（默认 CLAMP_TO_EDGE；glTF 10497=REPEAT、33071=CLAMP、33648=MIRRORED_REPEAT）
    VkSamplerAddressMode vkWrapV; // 采样寻址 V（同上）
    bool skipDefer; // 跳过延迟上传(FreeType字体atlas等需即时更新)
    void *z__attrb;

    static sIMAGE *zRTexture;

    sIMAGE();
    ~sIMAGE();
    sIMAGE &operator=(const sIMAGE &s);
    sIMAGE(const sIMAGE &s);
    // 释放 GPU 资源（image / memory / view / sampler）。
    // 与其他类型对齐的规范名：`Release()` 在本类型历史上就等于“释放 GPU”，
    // 而 `sMESH::Release()` 却是“释放 CPU”——同名反义。新代码请用 ReleaseGPU()。
    // Release() 保留为等价别名（现有 56 处调用与析构函数都依赖它）。
    // 设备已销毁时会跳过所有 vkDestroy*，只把句柄清空（见 GN_VkDeviceAlive）。
    void ReleaseGPU();
    void Release(bool immed = false); // 兼容别名：等价于 ReleaseGPU()
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
    float pbrParams[4]; // x=roughness, y=metallic, z=ao, w=alphaCutoff（0=不裁）
    // RGB = 自发光颜色（线性），a = 强度倍率。**不做光照计算，直接加到出射辐射上**
    // （因此在 ACES 之前，>1 的值能被 Bloom 抓住）。默认全 0 = 与旧行为逐位一致。
    float emissive[4]; // 112 bytes total, 16-byte aligned
};
static_assert(sizeof(VkModelPushConstPBR) == 112, "PBR push constant 必须与 ModelPC 逐字段一致（112 B）");

// 向后兼容别名 (Blinn-Phong 是默认)
typedef VkModelPushConstBP VkModelPushConst;

// 天空参数（CPU → 天空 UBO；与 assets/shaders/gn3d_skybox.frag 的 SkyboxUBO 逐字段一致）
// L4：末尾新增 vec4 atmosphereParams（144 B；UBO 总长 64 + 80 = 144）。
struct VkSkyParams {
    float sunDirection[3] = {0.45f, 0.78f, 0.44f};
    float sunIntensity = 1.0f;
    float horizonColor[3] = {0.68f, 0.78f, 0.92f};
    float cloudCoverage = 0.15f;
    float zenithColor[3] = {0.16f, 0.32f, 0.62f};
    float cloudSharpness = 0.6f;
    float timeSeconds = 0.0f;
    float windX = 0.0f;
    float windZ = 0.0f;
    float exposure = 1.0f;
    // L4：x=atmosphereMix (0=解析渐变旧路径, 1=sky-view LUT 大气天空), y=辐亮度倍率, z/w=保留
    float atmosphereMix = 1.0f;
    float atmosphereIntensity = 1.0f;
    float _pad[2] = {0.0f, 0.0f};
};
static_assert(sizeof(VkSkyParams) == 80, "VkSkyParams 必须为 80 字节");
#endif // GN_USING_3D

// B7-1（2026-11-26）：VkVertex 完整定义在 vkInternal.h；这里前向声明供 GN_VkContext 指针成员用。
struct VkVertex;

// B7-1：pass 内延迟纹理上传项（原 vkInternal.h 定义迁入，GN_VkContext.s2DRes 数组成员需要完整类型）
struct sPendingUpload {
    sIMAGE *img;
    unsigned char *data;
    int w, h, fmt;
};

// B7-2（2026-11-26）：3D 动态缓冲退休表条目（原 vk3dShutdown.cpp 匿名命名空间定义迁入，
// GN_VkContext.aRetired3DBuffers 成员需要完整类型）。扩容时旧缓冲先入表，
// 只在 GN_VkBeginFrame 的 vkWaitForFences 之后释放（防同帧 UAF，见 vk3dShutdown.cpp）。
struct sRETIRED_3D_BUFFER {
    VkBuffer aBuffer;
    VkDeviceMemory aMemory;
    uint64_t aRetireTick; // 退休时的帧同步点序号
};
