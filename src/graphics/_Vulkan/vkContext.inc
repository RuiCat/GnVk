// GN_mini Vulkan 头片段：兼容类型 + GN_VkContext 全局状态结构。
// 2026-11-26 自 GN_Vk.h 第 191-484 行 move-only 拆出（REFACTOR_PLAN §13 B4）。
// 由 GN_Vk.h 按原顺序包含；不是独立头文件。
// ═══════════════════════════════════════════════════════════
//  Vulkan 全局状态 (引擎初始化时设置)
// ═══════════════════════════════════════════════════════════

// ═══════════════════════════════════════════════════════════
//  兼容类型 (替代旧 _Graphics 中的定义)
// ═══════════════════════════════════════════════════════════
// sWINDOW 定义在 GN_2Dtype.h 中

struct GN_VkContext {
    // ═══════════════════════════════════════════════════════════
    //  按子系统分组的 GPU 资源（A5）。
    //
    //  为什么用匿名 struct：字段名与字段集合都没变，所以 3155 处
    //  `GN_Vk.xxx` 调用与 sizeof（2008 字节）都不受影响；但物理上已经按
    //  子系统切开，能一眼看出「每个 pass 到底要哪些资源」。
    //  新增资源请放进对应组，不要再往顶层加字段。
    //
    //  ⚠ 匿名 struct 是 GNU/MS 扩展（ISO C++ 不允许）。本项目不用 -Wpedantic，
    //    且 GCC/Clang/MSVC 均支持；换来的是零调用点改动。
    // ═══════════════════════════════════════════════════════════
    //  非平凡成员（有构造/析构，不能放进匿名 struct）——顶层只放这类成员，
    //  平凡字段一律进下面的分组（tests/lint/context_groups.sh 守卫）。
    //  管线缓存持有所有 VkPipeline；须在 vkDestroyDevice 之前 Release。
    // ═══════════════════════════════════════════════════════════
    GN3d::cPIPELINE_CACHE aPipelineCache;
    // B7-2（2026-11-26）：3D 动态缓冲退休表（原 vk3dShutdown.cpp 匿名命名空间 static 收敛，c4）。
    // 扩容退休的旧缓冲只在 GN_VkBeginFrame 的 fence 之后释放；GN_VkShutdown3D 清空。
    std::vector<sRETIRED_3D_BUFFER> aRetired3DBuffers;
    // B7-2：天空参数（原 vk3dPasses.cpp s_skyParams；NSDMI 初值与原全局一致）。
#ifdef GN_USING_3D
    VkSkyParams aSkyParams;
#endif
    // B7-2：延迟 pass 最近一次 viewProj（原 vk3dPasses.cpp s_deferredViewProj，SSAO/SSGI 用）。
    // 全局 GN_Vk 静态存储先零初始化，sMAT44() {} 不改 m ⇒ 与旧 static 初值一致；
    // 读取由 hasDeferredViewProj 守卫，未写入前不会被消费。
    sMAT44 aDeferredViewProj;

    // ── sDeviceRes：设备与队列：Vulkan 实例、物理设备、逻辑设备、队列族。子系统存活的根本前提
    struct {
        VkInstance instance;
        VkPhysicalDevice physicalDevice;
        VkDevice device;
        VkQueue graphicsQueue;
        uint32_t graphicsFamily;
        bool initialized;
        bool rayQuerySupported;
    };

    // ── sSwapchainRes：交换链与帧：窗口/离屏图、帧同步、帧计数、逻辑尺寸
    struct {
        VkSurfaceKHR surface;
        VkSwapchainKHR swapchain;
        VkFormat swapchainFormat;
        VkExtent2D swapchainExtent;
        uint32_t swapchainImageCount;
        VkImage *swapchainImages;
        VkImageView *swapchainViews;
        VkFramebuffer *framebuffers;
        VkSemaphore imageAvailable;
        VkSemaphore renderFinished;
        VkFence inFlightFence;
        uint32_t currentFrame;
        int viewWidth, viewHeight;
        bool headless;
        VkImageLayout swapFinalLayout;
        VkDeviceMemory *swapchainMemories;
        // B7-2（2026-11-26）：2D 叠层 pass/framebuffers（原 vkSwapchain.cpp 文件级全局收敛，c4）。
        // Init 创建、Shutdown 销毁；以 swapchainView 为附件，resize 重建。
        VkRenderPass renderPass2DOverlay = VK_NULL_HANDLE;
        VkFramebuffer *framebuffers2DOverlay = nullptr;
    };

    // ── sFrameRes：每帧录制：命令池/命令缓冲/描述符池
    struct {
        VkCommandPool commandPool;
        VkCommandBuffer *commandBuffers;
        VkDescriptorPool descriptorPool;
    };

    // ── s2DRes：2D 渲染：render pass、批绘管线（normal/bright/multiply/null）、动态顶点索引、默认白纹理
    struct {
        VkRenderPass renderPass;
        VkPipelineLayout pipelineLayout;
        VkPipeline pipeline;
        VkPipeline pipelineBright;                                             // additive blend for eBLEND_BRIGHT
        VkPipeline pipelineMultiply;                                           // multiply blend for eBLEND_MODULATE
        VkPipeline pipelineNull;                                               // no blend for eBLEND_NULL
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
        VkImage defaultImage;                                                  // 1×1 白色默认纹理图像
        VkDeviceMemory defaultImageMemory;                                     // 1×1 白色默认纹理内存
        VkImageView defaultImageView;                                          // 1×1 白色默认纹理视图 (纯色/无纹理绘制降级)
        VkDescriptorSet defaultDS;
        // B7-1（2026-11-26）：2D 帧状态，自 vkFrame2d.cpp 文件级全局/static 收敛（c4）。
        // 初值用 NSDMI 与原全局初值一致；原导出符号 s_currentScissor/s_scissorEnabled/s_scissorDirty 移除（abi/changes.md 登记）。
        VkRect2D scissorRect = {{0, 0}, {0, 0}};
        bool scissorEnabled = false;
        bool scissorDirty = false; // 首次由 GN_VkBeginFrame 设置
        sPendingUpload pendingUploads[256] = {}; // 支持大量纹理(文件浏览器等)
        int pendingUploadCnt = 0;
        bool inRenderPass = false;
        VkVertex *vbuf = nullptr;
        uint32_t vmax = 0;
        uint32_t vcnt = 0;
        VkDescriptorSet lastDS = VK_NULL_HANDLE;
        VkDescriptorSet batchDS[256] = {};
        uint32_t batchEnd[256] = {}; // cumulative vertex count at each batch boundary
        uint32_t batchCount = 0;
        VkBuffer pendingStagingBufs[32] = {};
        VkDeviceMemory pendingStagingMems[32] = {};
        int pendingStagingCnt = 0;
        unsigned int alphaColorKey = 0; // B7-2：原 vkDraw2d.cpp s_alphaColorKey（SetAlphaColorKey 写入）
        unsigned short lastBlendMode = 1; // B7-5：原 vkDraw2d.cpp ImageDraw 内 static（混排管线缓存）
    };

    // ── sForwardRes：3D 前向：MSAA/深度、3D 管线与 UBO、PBR（含透明变体）、阴影 CSM、3D 动态顶点索引
    struct {
        VkSampleCountFlagBits msaaSamples;                                     // 多重采样数 (默认 4×)
        VkImage depthImage;
        VkDeviceMemory depthMemory;
        VkImageView depthView;
        VkFormat depthFormat;
        VkImage msaaColorImage;
        VkDeviceMemory msaaColorMemory;
        VkImageView msaaColorView;
        VkRenderPass renderPass3D;
        VkPipelineLayout pipelineLayout3D;
        VkPipeline pipeline3D;
        VkDescriptorSetLayout descriptorLayout3D;
        VkDescriptorSet descriptorSet3D;
        VkBuffer uniformBuffer3D;
        VkDeviceMemory uniformMemory3D;
        VkBuffer lightBuffer3D;
        VkDeviceMemory lightMemory3D;
        VkPipelineLayout pipelineLayoutPBR;
        VkPipeline pipelinePBR;
        VkPipeline pipelinePBRBlend;                                           // 透明变体：srcAlpha/oneMinusSrcAlpha、深度写关、双面（BLEND 材质）
        VkDescriptorSetLayout descriptorLayoutPBR;
        VkDescriptorSet descriptorSetPBR;                                      // PBR 专用描述符集 (含 normal + shadow)
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
        // ── B2：点光/聚光 cube shadow（单个光源，6 面）──
        // 与上面的 CSM 2D array 是**两套独立资源**：CSM 给方向光，cube 给点光/聚光。
        // 共用一张图会让两者的渲染时机互相踩（CSM 在帧首渲染，cube 需要知道点光位置）。
        VkImage cubeShadowImage;
        VkDeviceMemory cubeShadowMemory;
        VkImageView cubeShadowView; // CUBE_ARRAY 视图（采样用；layerCount = 6*N）
        // 每"灯-面"一个 2D 视图。索引 = lightIndex * 6 + face。
        VkImageView cubeShadowFaceView[GN_CUBE_SHADOW_TOTAL_LAYERS];
        VkSampler cubeShadowSampler;
        VkRenderPass cubeShadowRenderPass;
        VkFramebuffer cubeShadowFramebuffer[GN_CUBE_SHADOW_TOTAL_LAYERS];
        // cube 专用的 depth-only 管线：**动态 viewport**。
        // 不能复用 CSM 的 shadowPipeline —— 它的 viewport 写死 smSize(2048)，
        // 而 cube 是 1024，渲染结果会被缩放到图幅的右下角（实测 bbox x[686,1023]）。
        VkPipeline cubeShadowPipeline;
        uint32_t cubeShadowSize; // 默认 1024（单灯 6 面 × 1024² ≈ 25 MB @ D32）
        uint32_t cubeShadowLightCount; // 本帧实际渲染了几盏灯的 cube shadow（0..4）
        VkBuffer shadowUBOBuffer;
        VkDeviceMemory shadowUBOMemory;
        uint32_t shadowMapSize;                                                // 默认 2048
        VkBuffer vertexBuffer3D;
        VkDeviceMemory vertexMemory3D;
        VkBuffer indexBuffer3D;
        VkDeviceMemory indexMemory3D;
    };

    // ── s3DPassRes：3D pass 共享状态（B7-2 收敛，c4）：pass 开关、绘制偏移/容量、前向
    //    framebuffers、阴影管线选择、延迟 viewProj 标志、退休 tick、蒙皮路径诊断。
    //    原 vk3dInit/vk3dPasses/vk3dShutdown/vkSkinnedDispatch 文件级全局/static；NSDMI 初值与原一致。
    struct {
        bool in3DPass = false;
        bool inShadowPass = false;
        bool inCubeShadowPass = false;
        uint32_t cubeShadowLightIndex = 0; // 当前正在渲染哪盏灯（0..GN_MAX_CUBE_SHADOW_LIGHTS-1）
        sVEC3 cubeShadowLightPos = {0.0f, 0.0f, 0.0f}; // 当前 cube pass 的光源位置（BeginForLight 写入）
        // 当前 shadow pass 用哪条管线：默认 CSM 的 shadowPipeline；cube pass 临时设成
        // cubeShadowPipeline（GN_VkShadowDraw 据此绑管线，见 vk3dPasses.cpp 的 B2 教训）。
        VkPipeline shadowPassPipeline = VK_NULL_HANDLE;
        VkFramebuffer *framebuffers3D = nullptr;
        VkDeviceSize vtxOfs3D = 0; // 同一帧内多个 draw 的写入偏移，避免相互覆盖
        VkDeviceSize idxOfs3D = 0;
        uint32_t vtxCap3D = 0; // 3D 绘制缓冲容量追踪（Ensure3DBufferCapacity）
        uint32_t idxCap3D = 0;
        bool hasDeferredViewProj = false; // 顶层 aDeferredViewProj 的有效标志
        uint64_t retireTick3D = 0; // 退休表帧同步点序号（releaseRetired3DBuffers 自增）
        GN3d::eSKIN_PALETTE_PATH lastSkinnedPath = GN3d::eSKIN_PATH_NONE; // 上次 GN_VkDrawSkinnedAuto 走的路径
    };

    // ── sRtRes：离屏渲染目标（B7-2 收敛，c4）：RT pass/framebuffer 与当前目标。
    //    原 vkRenderTarget.cpp 文件级 static/全局；NSDMI 初值与原一致。
    struct {
        VkRenderPass rtRenderPass = VK_NULL_HANDLE;
        VkFramebuffer rtFramebuffer = VK_NULL_HANDLE;
        sIMAGE *activeRT = nullptr; // 当前渲染目标
        bool rtInProgress = false; // RT pass 进行中
    };

    // ── sPostRes：后处理：HDR 离屏目标 + 合成管线（色调映射/Bloom/Gamma）
    struct {
        VkImage hdrImage;
        VkDeviceMemory hdrMemory;
        VkImageView hdrView;
        VkSampler hdrSampler;
        VkRenderPass hdrRenderPass;
        VkFramebuffer *hdrFramebuffers;
        VkPipelineLayout compositePipelineLayout;
        VkPipeline compositePipeline;
        VkDescriptorSetLayout compositeDescriptorLayout;
        VkDescriptorSet compositeDescriptorSet;
        VkRenderPass compositeRenderPass;
        VkFramebuffer *compositeFramebuffers;
    };

    // ── sAtmosphereRes：大气与间接光基础：天空盒、环境 6 面烘焙、预滤波 cubemap、BRDF LUT、GI UBO
    struct {
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
        VkImage envBakeImage;                                                  // 128²×6 RGBA16F 源 cubemap（烘焙目标）
        VkDeviceMemory envBakeMemory;
        VkImageView envBakeFaceViews[6];                                       // 每面 2D 视图（render pass 附件）
        VkRenderPass envBakeRenderPass;
        VkFramebuffer *envBakeFramebuffers;                                    // 6 个面各一个
        VkPipelineLayout envBakePipelineLayout;
        VkPipeline envBakePipeline;                                            // 归 aPipelineCache
        VkDescriptorSetLayout envBakeDescriptorLayout;
        VkDescriptorSet envBakeDescriptorSets[6];                              // 每面一个 128 B UBO（逐面绑定）
        VkBuffer envBakeUBOBuffer;
        VkDeviceMemory envBakeUBOMemory;
        int envBakeSize;
        VkImage envPrefilteredImage;
        VkDeviceMemory envPrefilteredMemory;
        VkImageView envPrefilteredView;
        VkSampler envPrefilteredSampler;
        int envPrefilteredMipCount;
        VkImage brdfLutImage;
        VkDeviceMemory brdfLutMemory;
        VkImageView brdfLutView;
        VkSampler brdfLutSampler;
        VkBuffer giUBOBuffer;
        VkDeviceMemory giUBOMemory;
    };

    // ── sDeferredRes：延迟渲染：G-Buffer 四附件、延迟光照、SSAO、SSGI、SH9 探针、G-Buffer 调试视图
    struct {
        VkImage ssaoRawImage;
        VkDeviceMemory ssaoRawMemory;
        VkImageView ssaoRawView;
        VkImage ssaoBlurImage;
        VkDeviceMemory ssaoBlurMemory;
        VkImageView ssaoBlurView;
        VkSampler ssaoSampler;
        VkRenderPass ssaoRenderPass;
        VkRenderPass ssaoBlurRenderPass;
        VkFramebuffer ssaoFramebuffer;
        VkFramebuffer ssaoBlurFramebuffer;
        VkDescriptorSetLayout ssaoDescriptorLayout;
        VkDescriptorSet ssaoDescriptorSet;
        VkDescriptorSetLayout ssaoBlurDescriptorLayout;
        VkDescriptorSet ssaoBlurDescriptorSet;
        VkPipelineLayout ssaoPipelineLayout;
        VkPipeline ssaoPipeline;
        VkPipelineLayout ssaoBlurPipelineLayout;
        VkPipeline ssaoBlurPipeline;
        VkBuffer ssaoUBOBuffer;                                                // 400 B：sSSAO_UBO
        VkDeviceMemory ssaoUBOMemory;
        VkImage ssgiRawImage;
        VkDeviceMemory ssgiRawMemory;
        VkImageView ssgiRawView;
        VkImage ssgiDenoisedImage;
        VkDeviceMemory ssgiDenoisedMemory;
        VkImageView ssgiDenoisedView;
        VkSampler ssgiSampler;                                                 // 颜色/raw/denoise 上采样：LINEAR
        VkSampler ssgiNearestSampler;                                          // 全分辨率 G-Buffer 深度/法线/albedo：NEAREST
        VkRenderPass ssgiRenderPass;
        VkRenderPass ssgiDenoiseRenderPass;
        VkFramebuffer ssgiFramebuffer;
        VkFramebuffer ssgiDenoiseFramebuffer;
        VkDescriptorSetLayout ssgiDescriptorLayout;
        VkDescriptorSet ssgiDescriptorSet;
        VkDescriptorSetLayout ssgiDenoiseDescriptorLayout;
        VkDescriptorSet ssgiDenoiseDescriptorSet;
        VkPipelineLayout ssgiPipelineLayout;
        VkPipeline ssgiPipeline;
        VkPipelineLayout ssgiDenoisePipelineLayout;
        VkPipeline ssgiDenoisePipeline;
        VkBuffer ssgiUBOBuffer;                                                // 160 B：sSSGI_UBO
        VkDeviceMemory ssgiUBOMemory;
        VkImage probeImages[9];
        VkDeviceMemory probeMemories[9];
        VkImageView probeViews[9];
        VkSampler probeSampler;
        VkBuffer probeUBOBuffer;                                               // 48 B：sPROBE_UBO
        VkDeviceMemory probeUBOMemory;
        VkImage gNormalRoughImage, gAlbedoMetalImage, gAoImage, gDepthImage;
        VkDeviceMemory gNormalRoughMemory, gAlbedoMetalMemory, gAoMemory, gDepthMemory;
        VkImageView gNormalRoughView, gAlbedoMetalView, gAoView, gDepthView;
        VkSampler gBufferSampler;
        VkRenderPass gBufferRenderPass;
        VkFramebuffer *gBufferFramebuffers;
        VkPipelineLayout gBufferPipelineLayout;
        VkPipeline gBufferPipeline;
        VkDescriptorSetLayout gBufferDescriptorLayout;
        VkDescriptorSet gBufferDescriptorSet;
        VkPipelineLayout deferredLightPipelineLayout;
        VkPipeline deferredLightPipeline;
        VkDescriptorSetLayout deferredLightDescriptorLayout;
        VkDescriptorSet deferredLightDescriptorSet;
        VkRenderPass deferredLightRenderPass;
        VkFramebuffer *deferredLightFramebuffers;
        VkPipelineLayout gBufferDebugPipelineLayout;
        VkPipeline gBufferDebugPipeline;
        uint32_t probeCursor = 0; // B7-5：原 vkProbe.cpp 更新函数内 static（探针轮转游标）
    };

    // 说明：粒子 / 布料 / 贴花 / 降水的 GPU 资源不在本结构体内，
    // 它们各自持有文件内静态资源（见 vkParticleCompute.cpp 等）。
    GN_VkContext() : initialized(false), rayQuerySupported(false), headless(false),
                     swapFinalLayout(VK_IMAGE_LAYOUT_PRESENT_SRC_KHR), swapchainMemories(0),
                     swapchainImages(0), swapchainViews(0),
                     framebuffers(0), commandBuffers(0),
                     defaultImage(VK_NULL_HANDLE), defaultImageMemory(VK_NULL_HANDLE),
                     defaultImageView(VK_NULL_HANDLE), defaultDS(VK_NULL_HANDLE),
                     vertexCapacity(4096), indexCapacity(6144),
                     vertexCount(0), indexCount(0)
#ifdef GN_USING_3D
                     ,
                     msaaSamples(VK_SAMPLE_COUNT_4_BIT), depthImage(VK_NULL_HANDLE), depthMemory(VK_NULL_HANDLE), depthView(VK_NULL_HANDLE), depthFormat(VK_FORMAT_D32_SFLOAT), msaaColorImage(VK_NULL_HANDLE), msaaColorMemory(VK_NULL_HANDLE), msaaColorView(VK_NULL_HANDLE), renderPass3D(VK_NULL_HANDLE), pipelineLayout3D(VK_NULL_HANDLE), pipeline3D(VK_NULL_HANDLE), descriptorLayout3D(VK_NULL_HANDLE), descriptorSet3D(VK_NULL_HANDLE), uniformBuffer3D(VK_NULL_HANDLE), uniformMemory3D(VK_NULL_HANDLE), lightBuffer3D(VK_NULL_HANDLE), lightMemory3D(VK_NULL_HANDLE), pipelineLayoutPBR(VK_NULL_HANDLE), pipelinePBR(VK_NULL_HANDLE), pipelinePBRBlend(VK_NULL_HANDLE), descriptorLayoutPBR(VK_NULL_HANDLE), descriptorSetPBR(VK_NULL_HANDLE), shadowImage(VK_NULL_HANDLE), shadowMemory(VK_NULL_HANDLE), shadowView(VK_NULL_HANDLE), shadowSampler(VK_NULL_HANDLE), shadowRenderPass(VK_NULL_HANDLE), shadowFramebuffer(VK_NULL_HANDLE), shadowPipelineLayout(VK_NULL_HANDLE), shadowPipeline(VK_NULL_HANDLE), shadowDescriptorLayout(VK_NULL_HANDLE), shadowDescriptorSet(VK_NULL_HANDLE), cubeShadowImage(VK_NULL_HANDLE), cubeShadowMemory(VK_NULL_HANDLE), cubeShadowView(VK_NULL_HANDLE), cubeShadowSampler(VK_NULL_HANDLE), cubeShadowRenderPass(VK_NULL_HANDLE), cubeShadowPipeline(VK_NULL_HANDLE), cubeShadowSize(0), cubeShadowLightCount(0), shadowUBOBuffer(VK_NULL_HANDLE), shadowUBOMemory(VK_NULL_HANDLE), shadowMapSize(2048), vertexBuffer3D(VK_NULL_HANDLE), vertexMemory3D(VK_NULL_HANDLE), indexBuffer3D(VK_NULL_HANDLE), indexMemory3D(VK_NULL_HANDLE), hdrImage(VK_NULL_HANDLE), hdrMemory(VK_NULL_HANDLE), hdrView(VK_NULL_HANDLE), hdrSampler(VK_NULL_HANDLE), hdrRenderPass(VK_NULL_HANDLE), hdrFramebuffers(nullptr), compositePipelineLayout(VK_NULL_HANDLE), compositePipeline(VK_NULL_HANDLE), compositeDescriptorLayout(VK_NULL_HANDLE), compositeDescriptorSet(VK_NULL_HANDLE), compositeRenderPass(VK_NULL_HANDLE), compositeFramebuffers(nullptr), skyboxImage(VK_NULL_HANDLE), skyboxMemory(VK_NULL_HANDLE), skyboxView(VK_NULL_HANDLE), skyboxSampler(VK_NULL_HANDLE), skyboxPipelineLayout(VK_NULL_HANDLE), skyboxPipeline(VK_NULL_HANDLE), skyboxDescriptorLayout(VK_NULL_HANDLE), skyboxDescriptorSet(VK_NULL_HANDLE), skyboxUBOBuffer(VK_NULL_HANDLE), skyboxUBOMemory(VK_NULL_HANDLE), envBakeImage(VK_NULL_HANDLE), envBakeMemory(VK_NULL_HANDLE), envBakeRenderPass(VK_NULL_HANDLE), envBakeFramebuffers(nullptr), envBakePipelineLayout(VK_NULL_HANDLE), envBakePipeline(VK_NULL_HANDLE), envBakeDescriptorLayout(VK_NULL_HANDLE), envBakeUBOBuffer(VK_NULL_HANDLE), envBakeUBOMemory(VK_NULL_HANDLE), envBakeSize(0), envPrefilteredImage(VK_NULL_HANDLE), envPrefilteredMemory(VK_NULL_HANDLE), envPrefilteredView(VK_NULL_HANDLE), envPrefilteredSampler(VK_NULL_HANDLE), envPrefilteredMipCount(0), brdfLutImage(VK_NULL_HANDLE), brdfLutMemory(VK_NULL_HANDLE), brdfLutView(VK_NULL_HANDLE), brdfLutSampler(VK_NULL_HANDLE), giUBOBuffer(VK_NULL_HANDLE), giUBOMemory(VK_NULL_HANDLE), ssaoRawImage(VK_NULL_HANDLE), ssaoRawMemory(VK_NULL_HANDLE), ssaoRawView(VK_NULL_HANDLE), ssaoBlurImage(VK_NULL_HANDLE), ssaoBlurMemory(VK_NULL_HANDLE), ssaoBlurView(VK_NULL_HANDLE), ssaoSampler(VK_NULL_HANDLE), ssaoRenderPass(VK_NULL_HANDLE), ssaoBlurRenderPass(VK_NULL_HANDLE), ssaoFramebuffer(VK_NULL_HANDLE), ssaoBlurFramebuffer(VK_NULL_HANDLE), ssaoDescriptorLayout(VK_NULL_HANDLE), ssaoDescriptorSet(VK_NULL_HANDLE), ssaoBlurDescriptorLayout(VK_NULL_HANDLE), ssaoBlurDescriptorSet(VK_NULL_HANDLE), ssaoPipelineLayout(VK_NULL_HANDLE), ssaoPipeline(VK_NULL_HANDLE), ssaoBlurPipelineLayout(VK_NULL_HANDLE), ssaoBlurPipeline(VK_NULL_HANDLE), ssaoUBOBuffer(VK_NULL_HANDLE), ssaoUBOMemory(VK_NULL_HANDLE), ssgiRawImage(VK_NULL_HANDLE), ssgiRawMemory(VK_NULL_HANDLE), ssgiRawView(VK_NULL_HANDLE), ssgiDenoisedImage(VK_NULL_HANDLE), ssgiDenoisedMemory(VK_NULL_HANDLE), ssgiDenoisedView(VK_NULL_HANDLE), ssgiSampler(VK_NULL_HANDLE), ssgiNearestSampler(VK_NULL_HANDLE), ssgiRenderPass(VK_NULL_HANDLE), ssgiDenoiseRenderPass(VK_NULL_HANDLE), ssgiFramebuffer(VK_NULL_HANDLE), ssgiDenoiseFramebuffer(VK_NULL_HANDLE), ssgiDescriptorLayout(VK_NULL_HANDLE), ssgiDescriptorSet(VK_NULL_HANDLE), ssgiDenoiseDescriptorLayout(VK_NULL_HANDLE), ssgiDenoiseDescriptorSet(VK_NULL_HANDLE), ssgiPipelineLayout(VK_NULL_HANDLE), ssgiPipeline(VK_NULL_HANDLE), ssgiDenoisePipelineLayout(VK_NULL_HANDLE), ssgiDenoisePipeline(VK_NULL_HANDLE), ssgiUBOBuffer(VK_NULL_HANDLE), ssgiUBOMemory(VK_NULL_HANDLE), probeSampler(VK_NULL_HANDLE), probeUBOBuffer(VK_NULL_HANDLE), probeUBOMemory(VK_NULL_HANDLE), gNormalRoughImage(VK_NULL_HANDLE), gAlbedoMetalImage(VK_NULL_HANDLE), gAoImage(VK_NULL_HANDLE), gDepthImage(VK_NULL_HANDLE), gNormalRoughMemory(VK_NULL_HANDLE), gAlbedoMetalMemory(VK_NULL_HANDLE), gAoMemory(VK_NULL_HANDLE), gDepthMemory(VK_NULL_HANDLE), gNormalRoughView(VK_NULL_HANDLE), gAlbedoMetalView(VK_NULL_HANDLE), gAoView(VK_NULL_HANDLE), gDepthView(VK_NULL_HANDLE), gBufferSampler(VK_NULL_HANDLE), gBufferRenderPass(VK_NULL_HANDLE), gBufferFramebuffers(nullptr), gBufferPipelineLayout(VK_NULL_HANDLE), gBufferPipeline(VK_NULL_HANDLE), gBufferDescriptorLayout(VK_NULL_HANDLE), gBufferDescriptorSet(VK_NULL_HANDLE), deferredLightPipelineLayout(VK_NULL_HANDLE), deferredLightPipeline(VK_NULL_HANDLE), deferredLightDescriptorLayout(VK_NULL_HANDLE), deferredLightDescriptorSet(VK_NULL_HANDLE), deferredLightRenderPass(VK_NULL_HANDLE), deferredLightFramebuffers(nullptr), gBufferDebugPipelineLayout(VK_NULL_HANDLE), gBufferDebugPipeline(VK_NULL_HANDLE)
#endif
    {
    }
};

extern GN_VkContext GN_Vk;

