// GN_mini Vulkan 头片段：3D 渲染 API（#ifdef GN_USING_3D 守卫原样保留）。
// 2026-11-26 自 GN_Vk.h 第 589-1037 行 move-only 拆出（REFACTOR_PLAN §13 B4）。
// 由 GN_Vk.h 按原顺序包含；不是独立头文件。
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

// 在 3D pass 之后重新打开一个 2D 叠层通道（loadOp=LOAD，保留 3D 画面）：
// 之后的 2D / UI / 文本绘制会在帧末 GN_VkEndFrame 里画进该通道，实现 HUD 叠在 3D 画面上。
void GN_VkBegin2DPass();


// 提交 3D 模型绘制 (PBR 模式: 使用 roughness/metallic/ao, 支持纹理/法线贴图)
void GN_VkDraw3DPBR(const void *vertices, uint32_t vertexCount,
                    const uint32_t *indices, uint32_t indexCount,
                    const sMAT44 *modelMatrix,
                    float diffuseR, float diffuseG, float diffuseB,
                    float roughness = 0.5f, float metallic = 0.0f, float ao = 1.0f,
                    const sIMAGE *albedoTex = nullptr, const sIMAGE *normalTex = nullptr,
                    float alphaCutoff = 0.0f, bool alphaBlend = false,
                    // 自发光：RGB = 颜色（线性），intensity = 强度倍率。
                    // 默认 0 ⇒ 与旧调用点逐位一致（不加参数就是"不发光"）。
                    float emissiveR = 0.0f, float emissiveG = 0.0f, float emissiveB = 0.0f,
                    float emissiveIntensity = 0.0f);

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
// L3c CSM：cascadeIndex 选择 0..3 级（越界钳制；默认 0 与旧单级行为等价）。
// 每次 Begin/End 对应一级；查看器每帧按级循环提交。
void GN_VkShadowPassBegin(const sMAT44 *lightViewProj, uint32_t cascadeIndex = 0);

// ── B2：点光/聚光 cube shadow（6 面）──
// 用法（在 GN_VkBeginFrame 之后、主 3D pass 之前）：
//
//   if (GN_VkCubeShadowBegin(lightPos, nearZ, farZ)) {
//       for (int face = 0; face < 6; ++face) {
//           GN_VkCubeShadowFaceBegin(lightPos, face, nearZ, farZ);
//           for (每 个 投射体) caster->DrawShadow();
//           GN_VkCubeShadowFaceEnd();
//       }
//       GN_VkCubeShadowEnd();
//   }
//
// `GN_VkCubeShadowBegin` 返回 false 表示资源不可用（设备未初始化 / cube map 未创建 /
// 已经在别的 shadow pass 里）；此时调用方应当跳过整个 cube shadow 阶段，
// 画面退化成"点光无阴影"而不是崩溃。
//
// nearZ/farZ：光源到场景的深度范围。点光需要覆盖整个影响半径，通常取
//   nearZ = 0.05~0.2，farZ = 灯的 range。
//
// **多盏灯**：cube shadow 是 CUBE ARRAY，每盏灯一层。用
// `GN_VkCubeShadowBeginForLight(i, ...)` 渲染第 i 盏；旧 `GN_VkCubeShadowBegin`
// 等价于第 0 盏（不破坏已有调用方）。
// 超过 GN_MAX_CUBE_SHADOW_LIGHTS（4）盏需要阴影时，超出的按"无阴影"处理 ——
// 不做静默复用（那会让阴影出现在错误的灯上，比没有阴影更糟）。
// 多光源：给第 lightIndex 盏灯（0..GN_MAX_CUBE_SHADOW_LIGHTS-1）渲染 cube shadow。
// 每盏灯占用 cube array 的独立一层，互不覆盖。
bool GN_VkCubeShadowBeginForLight(uint32_t lightIndex, const sVEC3 &lightPos, float nearZ,
                                 float farZ);
// 等价于 BeginForLight(0, ...)（保留旧签名，不破坏已有调用方）。
bool GN_VkCubeShadowBegin(const sVEC3 &lightPos, float nearZ, float farZ);
void GN_VkCubeShadowFaceBegin(const sVEC3 &lightPos, uint32_t face, float nearZ, float farZ);
void GN_VkCubeShadowFaceEnd();
void GN_VkCubeShadowEnd();
// 当前 cube shadow 的边长（未创建返回 0）。诊断/验收用。
uint32_t GN_VkCubeShadowSize();
// 本帧已渲染了几盏灯的 cube shadow（需在帧首调 GN_VkResetCubeShadowLightCount 清零）。
uint32_t GN_VkCubeShadowLightCount();
void GN_VkResetCubeShadowLightCount();

// 当前是否处于 3D pass 内部。
// 用途：GN_VkBegin3DPass 失败时（设备未初始化 / 重复进入）是静默返回的，
// 调用方（尤其是 RAII 守卫）必须能问一句“到底进去了没”，否则会误以为在 pass 内。
bool GN_VkIsIn3DPass();

// 逐级提交 shadow pass，绘制给定模型的所有投影体。
// 这是两个示例（mmdViewer / imoScene）逐字相同的循环，收到引擎里：
//     for (i < count) { ShadowPassBegin(vp[i], i); model.DrawShadow(); ShadowPassEnd(); }
// count 钓到 GN_VkShadowCascadeCount()；0 时什么都不做（安全）。
// 只接受裸数组而不要求 sSHADOW_CASCADE_RESULT：示例里光矩阵可能是自己存的，
// 强制它们改保存结构体是不必要的侵入。
void GN_VkDrawShadowCascades(const GN3d::cMODEL &caster, const sMAT44 *lightVPs,
                             uint32_t count);

// ── 3D pass 作用域守卫 ────────────────────────────────
// 为什么需要它：GN_VkBegin3DPass / End3DPass 靠引擎内部的 GN_Vk.in3DPass 标记成对维持。
// 漏调 End3DPass 的后果不是“少画一点”，而是：
//   1) GN_Vk.in3DPass 永远为 true ⇒ 之后所有 Begin3DPass 都静默失效（画面永久冻结）；
//   2) GN_VkCompositeToSwapchain 不被调用 ⇒ HDR 从不合成到交换链（纯黑）。
// 两个都是“静默黑屏”级的难查问题。而“在绘制循环里加个 early return”是很自然的写法。
// 这个 RAII 对象保证：只要构造成功，无论怎么退出作用域（包括 return / 抛异常），
// End3DPass + Begin2DPass 都会被调用。
struct sFRAME_3D_SCOPE {
    bool aActive;

    // drawSkybox = true 时自动画天空盒（两边示例都是“天空盒永远第一”）。
    sFRAME_3D_SCOPE(const sMAT44 *viewProj, const sVEC3 *cameraPos, const void *lightUBO,
                    uint32_t lightUBOSize, bool drawSkybox = true);
    ~sFRAME_3D_SCOPE();

    // 未成功进入 3D pass（例如设备未初始化/重复进入）；调用方应直接 return。
    bool Valid() const { return aActive; }

    // 生命周期唯一，不得拷贝（拷贝会导致双重 End3DPass）。
    sFRAME_3D_SCOPE(const sFRAME_3D_SCOPE &) = delete;
    sFRAME_3D_SCOPE &operator=(const sFRAME_3D_SCOPE &) = delete;
};
// L3c：阴影级联层数（固定 4；gpuCheck 用它断言按层 framebuffer 已建）。
uint32_t GN_VkShadowCascadeCount();
void GN_VkShadowPassEnd();
void GN_VkShadowDraw(const void *vertices, uint32_t vertexCount,
                     const uint32_t *indices, uint32_t indexCount,
                     const sMAT44 *modelMatrix);

// — HDR 合成 (Bloom + 色调映射) —
// 在 3D Pass 之后调用, 将 HDR 内容合成到 swapchain；GN_VkEnd3DPass /
// GN_VkDeferredLighting 会自动调用一次（L1：3D 输出统一为线性 HDR）。
void GN_VkCompositeToSwapchain();

// — L1 后处理（曝光/色调映射/Bloom）—
// 写 POST UBO；非法字段被钳制后写入（返回值表示输入是否原样合法）。缓冲不存在时静默忽略。
bool GN_VkSetPost(const GN3d::sPOST_UBO &ubo);
// 读取当前 POST 参数（空指针安全）；未初始化返回 false。
bool GN_VkGetPost(GN3d::sPOST_UBO *out);
// 以 EV 设置曝光（exposure = 2^EV）；EV 钳制到 [-10,10]。
void GN_VkSetExposure(float ev);

// L1b：Bloom 三参数。intensity < 0 按 0 处理；threshold/knee 钳到 >= 0。
// 用途：火焰/自发光的辉光需要比默认（0.3 / 0.8 / 0.3）更强的强度与更低的阈值，
// 而夜景又不能让整屏发雾 —— 没有这个 setter 就只能改 POST UBO 的内部静态量。
void GN_VkSetBloom(float intensity, float threshold, float knee);
// 切换色调映射模式（0=NONE / 1=Reinhard / 2=ACES，越界钳制）。
void GN_VkSetTonemapMode(int32_t mode);
// L4c：读取大气 LUT 纹理（RGBA16F，SHADER_READ_ONLY；未就绪返回 false）。
// 天空 shader 接线前供 gpuCheck 回读校验；transmittance 为 256×64、skyView 为 192×108。
bool GN_VkGetAtmosphereLut(VkImage *transmittance, VkImage *skyView);
// L4：设置当前大气参数与太阳方向（世界空间 y-up、指向太阳；内部归一化 + validate）。
// 参数/太阳变化时标记 sky-view LUT 为脏，下一次 3D pass 前由 compute 重建；未初始化安全忽略。
void GN_VkSetAtmosphere(const GN3d::sATMOSPHERE_PARAMS &params, const float sunDir[3]);
// L4：读取大气 LUT 视图与采样器（供天空/光照描述符绑定；未就绪返回 false）。
bool GN_VkGetAtmosphereLutViews(VkImageView *transmittanceView, VkImageView *skyView,
                                VkSampler *transmittanceSampler, VkSampler *skySampler);
// L2a：合成着色器内 FXAA 开关（false=关，默认；true=开）。只改 POST UBO.fxaa。
void GN_VkSetFxaa(bool enabled);

// — 天空盒 —
// 设置天空参数（下一次 GN_VkDrawSkybox 生效）；未设置时使用 VkSkyParams 默认值。
void GN_VkSetSky(const VkSkyParams &params);
// 读取当前天空参数（G3 重烘用；空指针安全）。
void GN_VkGetSky(VkSkyParams *out);
void GN_VkDrawSkybox(const sMAT44 *viewProj);

// — GI（G1a）：更新 256 B GI UBO（SH9/倍率/AO/invViewProj），缓冲不存在时静默忽略。
void GN_VkSetGI(const GN3d::sGI_UBO &ubo);

// — G1b：开/关 GI IBL。只改 GI UBO 的 indirect[0]/[1]（漫反射/镜面倍率），
// 保留 SH9/envParams/invViewProj；GI UBO 不存在时静默返回。
void GN_VkEnableGI(bool enable);

// — B1：**环境光（IBL）强度连续可调**。复用 indirect[0]/[1]（这两个字段的语义本来就是
// "漫反射倍率 / 镜面倍率"，此前只有 0 和 1 两档，等于把倍率当布尔用）。
//
// 为什么必须有它：天空 IBL 的贡献量级很容易盖过方向光。实测在火焰场景里把环境光与
// 所有光源都归零后画面仍然被照亮 —— 那是 IBL 在出光，于是方向光的阴影只占最终色的
// 一小部分，投影几乎看不见。没有连续倍率就只能"要么全开要么全关"。
//
// scale：漫反射倍率，钳制到 [0, 4]；镜面倍率取 scale * specularRatio（默认 0.25，
// 与 INDOOR 预设一致 —— 环境镜面反射通常比漫反射弱得多）。
// scale == 0 等价于 GN_VkEnableGI(false)；scale == 1 等价于 GN_VkEnableGI(true)。
// GI UBO 不存在时静默返回。
void GN_VkSetGIIntensity(float scale, float specularRatio = 0.25f);

// — G1b：环境光照预设（引擎侧决定来源，demo 只传语义）—
// INDOOR：均匀暖灰 SH9 + 漫反射 1/镜面 0.25/AO 1/SSAO 0（避免天空 IBL 把室内洗蓝）；
// SKY：恢复烘焙环境的 GI UBO（未烘过则保留当前值）。ambientScale 仅 INDOOR 使用，钳制 [0,4]。
void GN_VkSetEnvironmentLighting(GN3d::eENV_LIGHTING mode, float ambientScale);

// — L0a：GI 子开关（只改对应字段，保留其余；缓冲不存在时静默返回）—
// SSAO 开关写 GI UBO.indirect.w；SSGI 强度写 envParams.w（0=关，钳制到 [0,1]）。
void GN_VkSetSsaoEnabled(bool enable);
void GN_VkSetSsgiStrength(float strength);

// — G3：按当前天空参数重烘环境（6 面 → CPU 预滤波 → 上传 → 只更新 GI UBO 的 SH9）。
// 必须在 3D pass 之外调用；未初始化/资源缺失/失败返回 false 且保留旧环境。
bool GN_VkRebakeEnvironment();

// — SSAO（G2b）：在 G-Buffer 绘制之后、GN_VkDeferredLighting 之前调用。
// GI UBO 的 indirect.w<=0 时直接返回（默认关，观感/开销不变）。
void GN_VkSSAOPass();

// — SSGI（G4）：在 SSAOPass 之后、GN_VkDeferredLighting 之前调用。
// GI UBO 的 envParams.w<=0 时直接返回（默认关，观感/开销不变）。
void GN_VkSSGIPass();

// — 探针（G5b-2）：设置探针权重（0=关，默认；写 probe UBO.weight）。
void GN_VkSetProbeWeight(float weight);
// 每帧最多 2 个探针向当前环境 SH9 做 alpha=0.15 混合；weight=0 时直接返回（无调用者即无更新）。
void GN_VkUpdateProbes();

// — 延迟渲染 —
void GN_VkBegin3DPassDeferred(const sMAT44 *viewProj, const sVEC3 *cameraPos,
                              const void *lightUBO, uint32_t lightUBOSize);
void GN_VkDraw3D_GBuffer(const void *vertices, uint32_t vertexCount,
                         const uint32_t *indices, uint32_t indexCount,
                         const sMAT44 *modelMatrix,
                         float albedoR, float albedoG, float albedoB,
                         float roughness = 0.5f, float metallic = 0.0f, float ao = 1.0f,
                         const sIMAGE *albedoTex = nullptr, const sIMAGE *normalTex = nullptr,
                         float alphaCutoff = 0.0f);
void GN_VkDeferredLighting(const sMAT44 *viewProj, const sVEC3 *cameraPos,
                           const void *lightUBO, uint32_t lightUBOSize);

// — G0b：G-Buffer 四通道调试视图（只读附件 1..4；必须在 3D pass 之外调用）。
// channel：0=NormalRough、1=AlbedoMetal、2=AO/材质、3=线性化深度（越界钳制 0..3）。
void GN_VkDebugGBuffer(int channel);

// ── P1c：GPU 材质实例（描述符集 + UBO/纹理绑定）────────────
// sMATERIAL_BINDING::aResource 按 aKind 指向下面两种资源描述之一。
struct sMATERIAL_UBO_RESOURCE {
    VkBuffer aBuffer;
    VkDeviceSize aOffset;
    VkDeviceSize aRange;
};
struct sMATERIAL_TEX_RESOURCE {
    VkImageView aImageView;
    VkSampler aSampler;
    VkImageLayout aLayout;
};

// 材质固定布局（set 0）：binding 0 = UNIFORM_BUFFER，binding 1 = COMBINED_IMAGE_SAMPLER。
// 成功时把 VkDescriptorSet 写入 material->aDescriptorSet 并返回 true；
// 描述非法/绑定不是 0,1/未初始化/池耗尽返回 false。
bool GN_VkMaterialCreate(GN3d::sMATERIAL_RES *material);
// 归还材质描述符集（不销毁 UBO/纹理本身）；nullptr/未创建时安全。
void GN_VkMaterialDestroy(GN3d::sMATERIAL_RES *material);
// 用材质实例的 set 0 绘制；vertices 为 VkVertex3D（11 float）数组，model 走 push constant。
void GN_VkDraw3DMaterial(const void *vertices, uint32_t vertexCount,
                         const uint32_t *indices, uint32_t indexCount,
                         const sMAT44 *modelMatrix, const GN3d::sMATERIAL_RES *material);

// ── P2：GPU 蒙皮绘制（顶点 = GN3d::sSKINNED_VERTEX，68 字节）────
// palette 为关节调色板（world × inverseBind）：蒙皮在模型空间完成后再乘 modelMatrix。
// 成功提交返回 true；未初始化/参数为空/调色板打包失败返回 false。
bool GN_VkDraw3DSkinned(const void *vertices, uint32_t vertexCount,
                        const uint32_t *indices, uint32_t indexCount,
                        const sMAT44 *palette, uint32_t paletteCount,
                        const sMAT44 *modelMatrix,
                        float r, float g, float b);

// ── P9-b：SSBO 调色板蒙皮（关节数不再受 128 UBO 上限；v1 上限 4096）────
// 顶点格式与 GN_VkDraw3DSkinned 相同（GN3d::sSKINNED_VERTEX，68B）；palette 存 SSBO。
// paletteCount 为 0 或 > 4096、参数为空、3D pass 外调用均返回 false。
bool GN_VkDraw3DSkinnedSsbo(const void *vertices, uint32_t vertexCount,
                            const uint32_t *indices, uint32_t indexCount,
                            const sMAT44 *palette, uint32_t paletteCount,
                            const sMAT44 *modelMatrix,
                            float r, float g, float b);

// ── P10：动态 LOD 诊断钩子 ────
// 最近一次 GN_VkDraw3DPBR 提交的 indexCount（无索引绘制 / 尚未绘制时为 0）。
// 仅供验收与诊断：选级正确性由 GN_3Dlod 的纯 CPU 单测保证。
uint32_t GN_VkLast3DIndexCount();

// ── P8：3D pass 内的调试线框（世界坐标，颜色 0xAARRGGBB）────
void GN_VkDrawDebugLines(const void *lines, uint32_t lineCount, unsigned int color);

// ── 编辑器控件：**每线自带颜色**的批量线段 ────
// `lines` 指向 GN3d::sEDIT_LINE 数组（start/end/color 三段）。与上面那个的区别只有颜色
// 粒度：网格/坐标轴/Gizmo 必须在一帧内出现 X红/Y绿/Z蓝 + 主线/次线等多种颜色，而单色
// 批次就得拆成 5~6 次绘制（每次一个 vkCmdBindPipeline + 一段顶点上传），既慢又容易
// 把顺序写错。顶点格式与调试线完全相同（位置 + RGBA），复用同一管线。
// 返回值：实际提交的线段数（未初始化/不在 pass 内/空输入返回 0）。
uint32_t GN_VkDrawEditLines(const void *lines, uint32_t lineCount);

// 同上，但**关闭深度测试** —— 编辑器控件（Gizmo/网格/包围盒）必须画在实体之上。
//
// 为什么单独一个入口而不是全局改：物理调试线、BVH 可视化这些**需要**正确的遮挡关系
// （看不到被墙挡住的碰撞盒才说明深度是对的）。两类需求不能共用一条管线。
// 变换控件则相反：箭头从物体中心伸出去，一半在物体内部，开深度测试就只剩箭头尖
// ——所有 DCC 都是把控件画在最上层，这是工具语义而不是渲染缺陷。
uint32_t GN_VkDrawEditLinesOverlay(const void *lines, uint32_t lineCount);

// ── P1c：纹理上传（RGBA8 → VkImage，glTF sampler → VkSampler）────
struct sTEXTURE_GPU {
    VkImage aImage;
    VkDeviceMemory aMemory;
    VkImageView aView;
    VkSampler aSampler;
    uint32_t aWidth;
    uint32_t aHeight;

    sTEXTURE_GPU();
};

// pixels 为逐行 top-down 的 RGBA8（width*height*4 字节），上传为 R8G8B8A8_SRGB。
// sampler 取 GN3d::samplerFromAssetTexture() 的结果；成功时填满 out 并返回 true。
// out 已被占用（aImage != 空）时拒绝，要求先 GN_VkDestroyTexture。
bool GN_VkCreateTextureRGBA8(const unsigned char *pixels, uint32_t width, uint32_t height,
                             const GN3d::sTEXTURE_SAMPLER &sampler, sTEXTURE_GPU &out);
// 释放纹理的 sampler/view/image/memory；可重复调用。
void GN_VkDestroyTexture(sTEXTURE_GPU &texture);

// ── P11-S3：雾（独立 Weather/Fog UBO，三条件 3D 片元管线共用）────
// 布局：color.rgb = 雾颜色，params = (density 1/m, heightFalloff 1/m, baseHeight m, maxOpacity)。
// 绑定点：前向 binding 3、PBR binding 6、延迟光照 binding 8（冻结槽位表）。
struct sFOG_UBO {
    float aColor[4];
    float aParams[4];
    // L4：大气空气透视（64 B 总长）。aAtmosphere.w 为太阳强度，aAtmosphere2.w 为开关。
    float aAtmosphere[4]; // xyz=太阳方向（世界空间、指向太阳、单位向量）, w=太阳强度
    float aAtmosphere2[4]; // x=地面半径(m), y=大气顶半径(m), z=相机高度(m), w=空气透视开关(0/1)
};
static_assert(sizeof(sFOG_UBO) == 64, "sFOG_UBO 必须为 64 字节");

// 写入雾参数（密度/高度衰减被钳到 >= 0，最大不透明度钳到 [0,1]）；未初始化时安全忽略。
// density == 0 表示关闭雾（默认值），既有画面不变。
void GN_VkSetFog(const sFOG_UBO &fog);

// ── L5b：云块（实例化**包围盒**：36 顶点 × N 实例，无顶点缓冲）──────────────
// 方块**只是范围**（中心 + 半边长），不是云的外形：顶点着色器反转绕序，只画盒子背面当占位，
// 片元在盒内 raymarch 三维噪声密度，并在盒内边界把密度收到 0 ⇒ 画面上看不到盒子。
// 预乘 alpha 混合、深度测试开 / 深度写关、背面剔除。上限 kCloudCubeMaxInstances = 128。
struct sCLOUD_CUBE_INSTANCE {
    float aCenter[3] = {0.0f, 0.0f, 0.0f}; // 盒中心（世界坐标，m）
    float aHalfSize = 50.0f; // 盒半边长（m）
    float aTint[3] = {1.0f, 1.0f, 1.0f}; // 云色调（线性 HDR）
    float aAlpha = 0.9f; // 密度倍率 0..1（不是不透明度）
};
struct sCLOUD_CUBE_PARAMS {
    sMAT44 aViewProj; // 主 pass 的 view-projection（列主序）
    GNg::sVEC3 aCameraPos; // 相机世界位置（距离雾用）
    GNg::sVEC3 aSunDir; // 指向太阳的单位向量
    float aSunIntensity = 1.0f; // 天气太阳强度 0..1
    GNg::sVEC3 aAmbient; // 天空环境色
    GNg::sVEC3 aFogColor; // 雾色
    float aFogDensity = 0.0f; // 雾密度（1/m）
    float aBaseAltitude = 220.0f; // 全场景统一的云底高度（m）：云底以下密度归零 ⇒ 平底
    float aOvercast = 0.0f; // 阴雨程度 0..1：压暗云体（雨天=满天乌云）
    const sCLOUD_CUBE_INSTANCE *aInstances = nullptr; // 方块数组（只读）
    uint32_t aCount = 0; // 1..32
};
// 未初始化 / 不在 3D pass 内 / 实例指针为空 / aCount 为 0 或 > 32 返回 false。
bool GN_VkDrawCloudCubes(const sCLOUD_CUBE_PARAMS &params);
// 释放立方体云的 UBO / 描述符集 / 管线布局（管线归 aPipelineCache）；可重复调用。
void GN_VkCloudCubeDestroy();

// ── P11-S5：降水（CPU 展开 billboard，alpha 混合，深度测试开 / 深度写关）────
// 每个活粒子展开成 2 个三角形（6 顶点，世界空间 billboard 面向相机）；
// 雨滴沿世界 -Y 按 aStreakScale 拉长（雪 = 1 ⇒ 圆形/方形飘落）；alpha = 粒子 aColor.w × aAlphaScale。
// v1 每帧 CPU 展开 + 上传动态顶点缓冲（≈36 B/顶点，8000 粒子 ≈ 1.7 MB/帧）。
// 参数非法（aCount > 0 但 aParticles 为空）/ 未初始化 / 3D pass 外返回 false。
struct sPRECIP_DRAW_PARAMS {
    sMAT44 aViewProj; // 主 pass 的 view-projection（列主序）
    GNg::sVEC3 aCameraRight; // 相机右向量（世界空间，单位）
    GNg::sVEC3 aCameraUp; // 相机上向量（世界空间，单位）
    const GN3d::sPARTICLE *aParticles = nullptr; // CPU 粒子池（只读）
    uint32_t aCount = 0;
    float aStreakScale = 1.0f; // 雨滴纵向拉伸（雪 = 1）
    float aAlphaScale = 1.0f; // 额外透明度缩放 0..1
};
bool GN_VkDrawPrecipitation(const sPRECIP_DRAW_PARAMS &params);
// 释放降水顶点缓冲/管线布局；可重复调用。
void GN_VkPrecipitationDestroy();

// ── P11-S6b：地面贴花（软圆斑 / 溅射环，alpha 混合、深度测试开 / 写关、不剔除）────
// 每个条目铺成一块**水平**四边形（世界 XZ 平面，y 由调用方给，通常贴地 +0.02），uv 供片元做软边：
//   * aKind == SPLASH_RING：环状 alpha（溅射圈）；
//   * 其它：中心亮的软圆斑（水渍）。
// 顶点缓冲按帧奇偶与帧内调用序号分配（4 槽），同帧可多次调用。
// 条目为空 / 未初始化 / 3D pass 外返回 false。
struct sDECAL_DRAW_PARAMS {
    sMAT44 aViewProj; // 主 pass 的 view-projection（列主序）
    const GN3d::sDECAL_RENDER_ITEM *aItems = nullptr;
    uint32_t aCount = 0;
    float aAlphaScale = 1.0f; // 额外透明度缩放 0..1
};
bool GN_VkDrawDecals(const sDECAL_DRAW_PARAMS &params);
// 释放贴花的顶点缓冲与管线布局；可重复调用。
void GN_VkDecalDestroy();

// ── P9：GPU 粒子（compute 模拟 + SSBO + 实例化 billboard）────
// 创建固定容量粒子系统：SSBO 为 host-visible（模拟在 compute 内完成，CPU 只做上传/回读）。
// capacity ∈ (0, PARTICLE_GPU_MAX_CAPACITY]；sim 非法或容量越界返回 false 且不改变既有系统。
// 重复调用会先释放旧系统再重建。
bool GN_VkParticleCreate(uint32_t capacity, const GN3d::sPARTICLE_SIM_DESC &sim);
// 把 count 个粒子写入 SSBO 的 [offset, offset+count) 区间；越界/空指针返回 false。
bool GN_VkParticleUpload(const GN3d::sGPU_PARTICLE *particles, uint32_t count, uint32_t offset);
// 提交一次 compute 模拟并等待完成（dt <= 0 或系统未创建返回 false）。
// 调用方须保证上一帧提交已完成（引擎在帧边界等待），v1 不跨帧流水化。
bool GN_VkParticleSimulate(float dt);
// 在 3D pass 内绘制 params.aCount 个 billboard（超过容量截断）；pass 外/参数非法返回 false。
bool GN_VkParticleDraw(const GN3d::sPARTICLE_DRAW_PARAMS &params);
// 只读回读接口（测试/调试）：返回映射后的粒子数组，未创建返回 0；outCount 可为空。
const GN3d::sGPU_PARTICLE *GN_VkParticleData(uint32_t *outCount);
// 释放 SSBO/UBO/描述符集；可重复调用。
void GN_VkParticleDestroy();

// ── P9-a：GPU 布料（compute 模拟 + SSBO + 网格绘制）────
// 惰性初始化：首次 Create 时建管线/缓冲；失败返回 false。
bool GN_VkClothCreate(const GN3d::sCLOTH_GPU_DESC &desc);
// 上传顶点（count 必须等于 width*height）；越界/空指针返回 false。
bool GN_VkClothUpload(const GN3d::sCLOTH_GPU_VERTEX *vertices, uint32_t count);
// 推进一步（compute 提交并等待完成）；未创建或 dt <= 0 返回 false。
bool GN_VkClothSimulate(float dt);
// 只读回读：返回映射后的顶点数组；未创建返回 0。
const GN3d::sCLOTH_GPU_VERTEX *GN_VkClothData(uint32_t *outCount);
// 在 3D pass 内绘制布料网格（顶点 pulling + 网格索引；albedo = (r,g,b)）。
bool GN_VkClothDraw(const sMAT44 &viewProj, float r, float g, float b);
// 释放布料 SSBO/管线布局/索引缓冲；可重复调用。
void GN_VkClothDestroy();

} // namespace GNg
#endif // GN_USING_3D
