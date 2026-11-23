# GnVk 未完成项与规划接管台账

> 2026-11-21 起接管。**本文件是唯一待办账本**：新增/完成/废弃都改这里，再回写对应计划文档。
> 口径：`docs/*.md` 的状态表 + `.agent-memory/` 的 known-defects/open-questions + 代码 `TODO/FIXME`。
> 状态：`done`（有证据）/ `in-progress` / `planned` / `blocked`（环境/硬件）/ `obsolete`（已被取代）。
> GPU/视觉项只记录实测证据；`SKIP`、软件渲染、未跑都不算完成。

## 0. 当前批次

| 批次 | 内容 | 状态 |
|---|---|---|
| B1 | 多层渲染 P0：CPU 契约（层/程序/材质/渲染项/解析/排序）+ 单测 | **done（2026-11-21）** |
| B2 | 多层渲染 P1a：管线缓存加特殊化常量/键版本（done）；P1b：按帧回收 API（done，销毁路径 GPU 未验） | **in-progress（API done / destruction GPU-unverified）** |
| B3 | DDGI：2f 法线偏置（code done，A/B 不判定）→ 2e relocation（CPU v1-lite + GPU 6 轴 done，视觉不判定）→ 2d 异步更新（**软件 compute done + parity/收敛/inactive/切光已验**；HW ray-query 变体未做） | **in-progress（仅剩 HW 变体与视觉）** |
| B4 | 引擎 RT GI S2–S5、G5 探针动态化 | planned（GPU） |
| B5 | 残余缺陷/文档漂移清理（HDR 双 tonemap、延迟上传 TODO、审计 P2、DEFECT_AUDIT 遗留项） | **done（B5b–B5g）**；残余 DA-M2 planned / DA-M3 blocked / DA-M1r blocked（需 ABI 决策） |

## 1. 已完成（本会话）

| ID | 项 | 证据 |
|---|---|---|
| B1-1 | `GN_RenderPipeline.h/.cpp`：7 层枚举、程序描述/入口、材质实例、渲染项、`ResolveLayerMask`/`ResolveEntry`、`ValidateShaderProgramDesc`/`ValidateRenderItem`、`SortRenderItems` | `src/graphics/GN_RenderPipeline.h`、`src/graphics/GN_RenderPipeline.cpp` |
| B1-2 | 单测 9 例：掩码三层回退、逐层覆盖、implicit-shadow 退让、程序/项校验、不透明前→后、透明后→前、稳定排序 | `tests/unit/testRenderPipeline.cpp`；`make unit` = **404 cases / 0 failed** |
| B1-3 | 接入 umbrella 头 | `src/core/GN.h` 新增 `GN_RenderPipeline.h` |
| B1-4 | 机检 | `lintCodeModularity` 3 文件 **0 违规 0 告警** |
| B2a | `sPIPELINE_DESC` 加 `aSpecCount/aSpec[8]/aKeyVersion`；校验/键/相等性纳入；`vkPipeline.cpp` 图形/计算创建注入 `VkSpecializationInfo` | `GN_Pipeline.h/.cpp`、`_Vulkan/vkPipeline.cpp`、`tests/unit/testPipeline.cpp`；`make unit` = **405/0**；lint 4 文件 0 违规 |
| B3c-1 | DDGI 异步更新 CPU 基座：范围调度（budget/环绕）、temporal alpha、SH9 混合 | `src/graphics/GN_3DprobeUpdate.h/.cpp`；`tests/unit/testProbeUpdate.cpp` 4 例；`make unit` = **413/0**；lint 0 |
| B3c-2 | DDGI 异步更新 GPU compute（软件解析路径）：`gn_cornell_probe_update.comp` + 共用 `cornellRayLib.glsl`；host 原地混合 + compute barrier + ProbeState/96B UBO + 回读校验 | parity：一次全量 vs 198 帧×64 探针 SH9 回读 hash 均 `f2447cc68304ca6f`（逐位一致）；DC 均值 0.979943 vs 烘焙 0.979817；ages 3 轮后=3；async=0 截图与改动前逐字节一致；inactive 跳过（前 100 个 SH 与资产逐值相等、age=0）与切光 age 归零（6 帧切光后 max=3 vs 不切光 6）已实测；`make shaders` 46/46、`make unit` 415/0 |

## 2. 待办（按优先级）

| ID | 项 | 来源 | 状态 | 证据/锚点 | 下一步与验收 |
|---|---|---|---|---|---|
| B2 | 多层渲染 P1a：`sPIPELINE_DESC` 特殊化常量/键版本（done）；P1b：`CollectGarbage` 按帧回收 API（done，销毁路径 GPU 未验） | `docs/MULTILAYER_RENDER_PIPELINE_PLAN.md` §5 P1 | **in-progress（API done / destruction GPU-unverified）** | `GN_Pipeline.h:144-190`；`GN_Shader.h:36-70`；`vkPipeline.cpp`/`vkShader.cpp`；`testPipeline.cpp` | P1a 405/0；P1b 无设备 no-op 单测通过；需 GPU 帧边界+fence 验证销毁时机 |
| B3a | DDGI Stage 2f：法线偏置（已接线，视觉收益未判定） | `docs/PROBE_BAKE_PLAN.md` §10.5 | **in-progress（code done / visual A/B inconclusive）** | `gn_cornell_rt.comp`、`_rq.comp`（wpB/背面降权）；`cornellGpuRt` UBO weight.z；`main.cpp` env | 已过 `make shaders` 45/45、`make cornellBox`、llvmpipe 出图；bias 0 vs 0.5×spacing mean|Δ|≈0.010/255、max=1 → 需 2e relocation/更强样本再判；默认 0 |
| B3b | DDGI Stage 2e：probe relocation（CPU v1-lite + **GPU 6 轴候选均已完成**；视觉未判定） | `docs/PROBE_BAKE_PLAN.md` §10.4 | **done（code；视觉未判定）** | `GN_3DprobeRelocation.*`、`gn_cornell_probe_relocation.comp`、`cornellGpuRt` binding 14/15、`testProbeRelocation.cpp` | GPU：`moved=674/12672 maxLen=0.11400 clamp_violations=0 nan=0`，两次运行 offset 逐位一致，class{active:11998,relocated:674}；CPU v1-lite 仍 `moved=693`；默认路径截图 `ebe115fd…` 不变；视觉 A/B 仍不判定 |
| B3c | DDGI Stage 2d：异步更新（CPU 调度 + GPU compute 软件路径 done；HW ray-query 变体 planned） | `docs/PROBE_BAKE_PLAN.md` §10.3 | **in-progress（2d 软件路径 done）** | `GN_3DprobeUpdate.*`、`gn_cornell_probe_update.comp`、`cornellRayLib.glsl`、`cornellGpuRt.*`、`main.cpp`；§10.7「2d 实测」 | 剩余：`_rq` 变体（需硬件）、脏表/压实（v2）；inactive 跳过与切光 age 归零已实测（§10.7） |
| B4a | 引擎 RT GI S2–S5（SSGI ray query binding 6/7/8、shader 分支、回退、gpuCheck） | `docs/ENGINE_RT_GI_PLAN.md:61-65` | planned（S1 已完成） | `src/graphics/_Vulkan/vkRayQuery.cpp`；`docs/ENGINE_RT_GI_PLAN.md` | S2 描述符/回退 → S3 shader → S4 逐像素回退 → S5 gpuCheck；需真机 |
| B4b | G5 探针动态化（当前 8×4×8 SH9、默认关、仅延迟路径） | `docs/GI_PLAN.md`；`M-gi-probes-3` | planned | `src/graphics/GN_3Dprobe.*`；`_Vulkan/vkProbe.cpp` | 与 B3 不同实现；先决定是否复用 DDGI 数据；默认路径不变 |
| B5a | imoScene 阴影首帧 GPU page fault（真机 amdgpu reset） | `docs/CHANGELOG.md:1-10` | **not-reproducible（2026-11-23 真机复核，见 §8 B9）** | 阴影默认开；真机 RADV/780M 上 `IMO_SCENE_SHADOWS=1` **3/3 次**均 480+ 帧、无 fault/reset，`=0` 同；原「卡住」为首帧提示文本造成的误判 | 复算：`IMO_SCENE_SHADOWS=1 timeout 12 ./build/imoScene 2>&1 | grep 已渲染`；若要保留历史结论，需在**当时那台**设备复现 |
| B5b | HDR 双 tonemap（PBR/延迟各一次 + composite 再一次） | `M-lighting-state-1` | **done（核销：已无双 tonemap）** | `gn3d_pbr.frag`/`gn3d_deferred_light.frag` 无 Reinhard/ACES/pow(1/2.2)；唯一 tonemap 在 `gn3d_composite.frag:33-96` | 无需修改；若回归再查 composite |
| B5c | `vkFrame2d.cpp:115` 延迟纹理上传 TODO（pass 内暂存被临时禁用） | 代码 TODO | **done（注释过时：代码已处理）** | `vkFrame2d.cpp:115-128`：`vkWaitForFences` 后遍历 `s_pendingUploads` 调 `DataFill`，再清计数 | 注释与代码不一致；只需删/改 TODO，不改行为 |
| B5d | SHADER_AUDIT 残余 P2/P3（S-25 光照代码重复、S-26 FXAA 不含 bloom、S-27 夜间天空 overcast 代理） | `docs/SHADER_AUDIT_2026-11-03.md:360-400` | **done（S-25/S-26/S-27 code; 视觉待真机）** | S-26：`gn3d_composite.frag:53-58`；S-27：`gn3d_skybox.frag`；S-25：`brdfLighting.glsl` + `sceneLighting.glsl`（全部重复函数抽出、UBO 名统一；pbr 525→363、deferred 606→445）；`make shaders` 47/47、引擎截图 `00550dcf…` 不变 | S-26/S-27 视觉待真机 |
| B5e | `DEFECT_ANALYSIS` 残余复核 | `docs/DEFECT_ANALYSIS.md` | **done（审计，2026-11-21）** | `docs/DEFECT_AUDIT_2026-11-21.md` | 报告只枚举 5 条（22 条计数无 item 化，无法复核）：R1 逐字节 `memcpy(this)` UB 仍成立（无复现，重写 cFRAME_RC 属独立批次）；R4 strcpy TODO 已清零；R3 警告实数 23250 |
| B5f | `TODO: verify buffer size`（起点 36 处，实际口径非 75） | `src/core/GN_String.cpp`、`src/ui/*`、`src/platform/_Sys/GN_Sys.cpp` | **done（36→0，2026-11-21）** | `docs/BUFFER_SIZE_AUDIT.md`；新增 `GN_StrCopyToLimit` 并用于 8 处调用方缓冲拷贝；5 处重叠 `strcpy`→`memmove`；7 处转显式契约注释；`make unit` 420/0 | 残留风险：已加 `_s` 安全变体（`GN_StrUnSplit_s`/`GN_StrExtractFromMultiItem_s`/`GN_StrToSysFilePath_s`/`GT_GetAppCurPath_s`/`GT_GetAppFileName_s`/`GN_GetDeviceInfoCpu_s`），旧 ABI 保留 |
| B5g | `DEFECT_AUDIT` 遗留项处置（2026-11-21）：cFRAME_RC 逐字节拷贝→逐字段（消除 UB 与 `zSound` 双重释放）、8 处 `strcpy` 加固、71 处裸 `strcpy`/`strcat` 台账、全量警告定量 | `docs/DEFECT_AUDIT_2026-11-21.md` §5 | **done（2026-11-21）** | `src/core/GN_FrameCopy.cpp`、`GN_Frame.cpp:101/:113`、`tests/unit/testFrameCopy.cpp`（4 例）；`docs/STRCPY_AUDIT_2026-11-21.md`（71 处：B5g 8 + B5h 12 + B5i 2 行加固；B5i 后：现存 65 / 契约 3 / 已核 45 / 非代码 2）；`make unit` = **438/0**；全量 `make -B` 警告 **24600** 条（`build/full_build_warn_20261121.log`；B5e 旧记 23250 为更早树口径） | 仍成立项已改判：DA-M1 已修复；DA-L1 已定量；DA-M2 planned、DA-M3 blocked |
| B5h | 契约型 `strcpy`/`strcat` 加固：ZIP 路径 6、2D 场景路径 2、图像名 64B 2、媒体路径 1、UDP 回声 1 | `docs/DEFECT_AUDIT_2026-11-21.md` §7 / `docs/STRCPY_AUDIT_2026-11-21.md` §2 | **done（2026-11-21）** | `docs/STRCPY_AUDIT_2026-11-21.md` §1；现存裸调用 67、契约 5；`make unit` 438/0；`make -B` 警告 24600（`build/full_build_warn_20261121b.log`） | 残留 5 契约型只差 ABI 级改造（DA-M1r） |
| B5i | 图像名转换容量化：`GT_ConvertImageNameForLoad_s` + `cLENS_FLARE::zDataLoad` 3 参重载（场景装载传 `sizeof(file_path)`） | `docs/STRCPY_AUDIT_2026-11-21.md` §1 | **done（2026-11-21）** | `src/graphics/GN_2Dscene.cpp:599`（`_s`）、`:616`（旧 ABI 转调）、`src/core/GN_Frame.cpp:2837`（3 参重载）；现存裸调用 65、契约 3；`make unit` **443/0**（438+5 例 `testImageNameConvert`）；全量 `make -B` 警告 24600（`build/full_build_warn_20261121c.log`） | 剩余 3 处仅旧 ABI，等 ABI 决策（DA-M1r） |
| B5j | `GN_Format.cpp` 按功能拆分（3419 行 → 8 个文件）：Pix 566 / Scale 662 / ImgFile 1039 / Audio 380 / Data 235 / Plugin 117 / Values 86 / Zip 358；内部宏与 `GT_ERR_FileCreateFail` 收敛到 `GN_FormatInternal.h` | `docs/ENGINE_COORDINATION_2026-11-21.md` | **done（2026-11-21 19:32）** | `src/core/GN_Format*.cpp`；`make` ✅、`make unit` **443/0**；`lintCodeModularity` 9 文件 **0 违规 0 告警**；独立产物 `LIB_STEM=GN_mini_coord` 全量链接 EXIT=0 | 公共头未改；`GN_Format.cpp` 已删除 |
| B5k | `GN_Frame.cpp` 按类拆分（2899 行 → 4 文件）：Rc 688 / Launcher 721 / Font 778 / LensFlare 724；共享宏 + `GT_ResetTransf2D`/`GT_AmEvsStartEndValueSwap` 收敛到 `GN_FrameInternal.h` | 本批（用户「拆分」指令） | **done（2026-11-21 20:05）** | `src/core/GN_Frame*.cpp`；私有产物 `LIB_STEM=GN_mini_coord`：lib 构建 EXIT=0、`make unit` **443/0**；lint 6 文件 **0 违规 0 告警**；公共头未改，`GN_Frame.cpp` 已删除 | 待共享窗口跑 plain `make` + `make unit` 收口 |
| B5l | 多着色器 P2a 骨架（不动绘制/画面）：canonical set0/set1/set2 绑定表 + 程序注册表（CPU 可测部分） | `docs/MULTILAYER_RENDER_PIPELINE_PLAN.md` §3.4/§5 P2 | **done（2026-11-21 22:30，CPU 部分）** | 新增 `src/graphics/_Vulkan/vkBindingLayout.cpp`、`vkShaderProgram.cpp`（+ 私有头）；`GN_Vk.h` 仅加声明；单测 `tests/unit/testBindingLayout.cpp`（绑定表完整性/版本/阴影例外/set2 变体/程序注册幂等） | `make`/`make unit` 全绿、lint 0、零绘制改动；真实 `VkDescriptorSetLayout` 创建需 GPU，无 /dev/dri 时只验 CPU 部分并注明；实跑：`make` exit=0、`make unit` **451/0**（443 基线 + 8 新例）、lint **0/0**（5 文件）；新增 `vkBindingLayout.h/.cpp`、`vkShaderProgram.h/.cpp`、`tests/unit/testBindingLayout.cpp`；**未改**任何绘制/着色器/画面路径（`git status` 仅新增文件）；GPU 部分（真实 `VkDescriptorSetLayout`、按程序建管线）未验（无 `/dev/dri`） |
| B5m | P3 CPU 层队列：sRENDER_QUEUE 入队校验/层掩码过滤/层内稳定排序/层计数 | docs/MULTILAYER_RENDER_PIPELINE_PLAN.md §3.5/§5 P3 | **done（2026-11-21 22:52，CPU 部分）** | 新增 src/graphics/GN_RenderQueue.h/.cpp + tests/unit/testRenderQueue.cpp（4 例）：submitRenderItem/collectRenderLayer/countRenderLayer/resetRenderQueue；实跑 make exit=0、make unit 459/0、lint 0/0；绘制接线（vkRenderQueue/逐层提交）待 P3b，真机验证待 GPU |
| B5n | P5 CPU：外部 SPIR-V 最小校验 + 程序层入口绑定/封口（失败不半注册） | docs/MULTILAYER_RENDER_PIPELINE_PLAN.md §3.7/§5 P5 | **done（2026-11-21 23:05，CPU 部分）** | 新增 src/graphics/GN_Spirv.h/.cpp（validateSpirvModule：魔数/对齐/header/版本/bound）+ vkShaderProgram 扩展（BindProgramEntry/SealShaderProgram/ShaderProgramState，层掩码校验、push constant≤128、绑定失败不改注册表、Seal 用 ValidateShaderProgramDesc 原子转 READY）+ tests/unit/testSpirv.cpp（4 例）；make exit=0、unit 463/0、lint 0/0；真实外部程序建管线（GPU 侧）未验，blocked 命令：amdgpu 机上 `make gpuCheck` + 自建外部程序冒烟 |
| E1 | CSM 阴影「退休缓冲同帧 UAF」（另一会话 tests/imoScene 报告） | `docs/ENGINE_COORDINATION_2026-11-21.md` §3 | **done（2026-11-21：真机 CSM 3/3 + 默认启动通过）** | `src/graphics/_Vulkan/vk3dPasses.cpp:26-31`（阴影 pass 每级重置共享 3D 缓冲偏移）；`docs/CHANGELOG.md` 2026-11-21 CSM 条目（首帧 amdgpu page fault） | 需真机复现；待报方补最小复现、帧内时序与影响面；证据补充（报方 19:42）：`vk3dPasses.cpp:32`（Begin 内释放）、`vk3dShutdown.cpp:42-60`（serial+2 余量）、`:64`（扩容）、`vkFrame2d.cpp:116`（fence）；复现：GPU 机 `IMO_SCENE_SHADOWS=1 timeout 20 ./build/imoScene`（可加 `IMO_SCENE_NO_TEXTURES=1 IMO_SCENE_MAX_MESHES=1` 缩小）→ amdgpu page fault/GPU reset；影响：4 级 CSM 全不可用（默认关），不阻断 imoScene 会话；卡 dbaa1b21 done（session-9735389c）：E1 退休释放移到 GN_VkBeginFrame（vkFrame2d.cpp:117-119），pass 内不再释放（vk3dPasses.cpp:30-32）；E2 每级 UBO/描述符（kShadowUBOSlotStride、GN_VkShadowPassBegin(...,cascadeIndex)）；E3 sIMAGE vkWrapU/vkWrapV/vkSrgb + ImageLoadFromMem(srgb,wrapU,wrapV)（GN_Vk.h:41-42/486-492）；共享 make ✅、unit 443/0 |
| E2 | 4 级阴影共用 64B UBO（同上报告） | 同上 | **code-done（真机不崩；4 级矩阵视觉 A/B 未判定）** | 待报方补锚点（`GN_VkShadowPassBegin` / shadow UBO 结构） | 独立 shadow UBO 或按级偏移；需 GPU 验证；证据补充（报方 19:42）：`vk3dInit.cpp:700-718`（64B 单槽）、`vk3dPasses.cpp:63/80`（每级都 map/绑偏移 0）；静态证据已足；影响：4 级阴影全用最后一级矩阵（不阻断 imoScene 会话）；卡 dbaa1b21 done（session-9735389c）：E1 退休释放移到 GN_VkBeginFrame（vkFrame2d.cpp:117-119），pass 内不再释放（vk3dPasses.cpp:30-32）；E2 每级 UBO/描述符（kShadowUBOSlotStride、GN_VkShadowPassBegin(...,cascadeIndex)）；E3 sIMAGE vkWrapU/vkWrapV/vkSrgb + ImageLoadFromMem(srgb,wrapU,wrapV)（GN_Vk.h:41-42/486-492）；共享 make ✅、unit 443/0 |
| E3 | `sIMAGE` 贴图 sRGB/wrap 需引擎侧支持（同上报告） | 同上 | **code-done（demo 已切引擎 API；视觉 A/B 未判定）** | `src/graphics/_Vulkan/vkImageData.cpp`、`GN_Vk.h`（现 `sIMAGE` 无 sRGB/wrap 字段；报方已回滚其 _Vulkan 改动） | 引擎加字段+采样设置；改共享头须公告并全量重验；证据补充（报方 19:42）：`vkImage.cpp:11`、`GN_Vk.h:41/484`、`vkImageIo.cpp:156`、`vkImageData.cpp:87/374`、`gn3d_pbr.frag:292`；复现：`IMO_SCENE_SRGB=1` vs `=0`；GLB 解析 UV 越界（角色 U=1.665、房间 U/V=5.54/3.87）⇒ wrap 应 REPEAT；影响：glTF 贴图偏色/拉边，报方在 tests/imoScene 用公开字段+替换 sampler 绕过；卡 dbaa1b21 done（session-9735389c）：E1 退休释放移到 GN_VkBeginFrame（vkFrame2d.cpp:117-119），pass 内不再释放（vk3dPasses.cpp:30-32）；E2 每级 UBO/描述符（kShadowUBOSlotStride、GN_VkShadowPassBegin(...,cascadeIndex)）；E3 sIMAGE vkWrapU/vkWrapV/vkSrgb + ImageLoadFromMem(srgb,wrapU,wrapV)（GN_Vk.h:41-42/486-492）；共享 make ✅、unit 443/0；demo 绕过已删（lead2 21:01，diff 126 行仅 `tests/imoScene`）；lead 独立验：`make imoScene` exit=0、`make unit` 443/0 |
| F1 | resize 后布料/贴花/降水管线未重建（沿用旧管线，真机 1/3 观测到） | 本次真机验证（G1） | **confirmed 误报（非缺陷，2026-11-21）** | `src/graphics/_Vulkan/GN_Vk.cpp:1015-1026`（recreate* 返回 false 才告警；同段 1006-1025 覆盖 debug/material/skin/particle/cloth/decal/precip/skin-ssbo）；`vkClothDraw.cpp:261`、`vkDecal.cpp:245`、`vkPrecipitation.cpp:304` 为 init/resize 共用重建入口 | 待查：启动早期 resize 时序/初始化前提是否导致 false；是否只影响首帧渲染（viewport 旧 extent）；复核：`recreateClothDrawPipeline`/`recreateDecalPipeline`/`recreatePrecipitationPipeline` 在 pipelineLayout 未建时 return false 才告警（`vkClothDraw.cpp:262-264`、`vkDecal.cpp:246-247`），首次绘制惰性建管线并按当前 extent，不错位；与项目记忆 M-known-defects-7 一致（已 restamp） |
| DA-M2 | 全局可变状态 61 个 → Context 封装（DA-M2） | `docs/DEFECT_ANALYSIS.md` §1 | **done（CPU 可验部分收口：1a 17 + 1b 4 + 2a 15 + 2b 1 + 批 3/4 共 21 项；只余 4 个因 ABI/平台分支保留）** | `rg -n '^[A-Za-z_][A-Za-z0-9_:<>]*[ \*]+(GT_|z)[A-Za-z0-9_]+(\[[^]]*\])?\s*(=|;)' src -g '*.cpp' \| wc -l` → 61；`extern` 123 条 | 先定 Context 边界与迁移顺序；无 GPU 可做，但属重构批次，不在缺陷修复批次内；2026-11-21 审计：61 个全局分布 = `GN_Input.cpp` 20、`GN_UiFBrowser.cpp` 16、`GN_Init.cpp` 6、`GN_Sys.cpp` 4、`GN_Sound.cpp` 3、其余 12（每文件 1–2）；**批 1 目标 = `GN_Input` 的 20 个 → `sINPUT_CONTEXT`（行为不变 + 单测）；批 2 = `GN_UiFBrowser` 16；批 3 = 初始化/Sys/音频；批 4 = 其余**；**批 1a（2026-11-21 23:12）**：`GN_Input.cpp` 17 个文件内全局收敛进 `sINPUT_CONTEXT`（字段 camelCase：aInputRecList/aInputIt/aIme…/aMouse…/aTouchMappingMKey/aLastZoomLen/aInputLastPort/aInputTestTime/aWsMappingToKey），`gInputContext` 单实例，行为不变；IME 3 个（GT_ImeOn/Recv/Char）仍被 `GN_UiListTree`/`GN_UiFBrowser`/`GN_UiTextIn` 以 ad-hoc extern 引用，留批 1b；实跑 `make` exit=0、`make unit` 463/0、lint 0/0；剩余全局 44；**批 1b（2026-11-21 23:22）**：IME 3（GT_ImeOn/Recv/Char）+ GT_ImeCount 收敛为 `GN::sIME_CONTEXT` + `GN::GetImeContext()`（GN_Input.h 声明、GN_Input.cpp 定义），`GN_UiTextIn`/`GN_UiListTree`/`GN_UiFBrowser`/`GN_UiPopDlgRun` 的 ad-hoc extern 删除、改为访问器；`make` exit=0、`make unit` 463/0；lint：GN_Input 0/0，UI 3 文件为**历史超限**（UiTextIn 1129 行、UiFBrowser 1966 行）非本轮引入；剩余全局 41；**批 2a（2026-11-21 23:40）**：`GN_UiFBrowser.cpp` 15 个文件内全局 → `sUI_FB_CONTEXT`（含 token-paste 宏 `GT_FbIcon##V##*` 改为 `gFbContext.aIcon##V##*`）；`GT_FilePreviewGenList` 依赖本地类 `GT_cFILE_PREVIEW_GEN`，留 2b；make exit=0、unit 463/0；文件历史超限 1966 行未变（非本轮）；**批 2b（2026-11-22 00:18）**：`GT_FilePreviewGenList`（唯一未收的文件级全局）→ `sUI_FB_PREVIEW_CONTEXT{aPreviewGenList}` + 单实例 `gFbPreviewContext`（插在 `GT_cFILE_PREVIEW_GEN` 定义后；因依赖本地类型，不能并入 `sUI_FB_CONTEXT`），改 2 处引用；全仓已无 `GT_FilePreviewGenList` 引用；实测 `make` exit=0（0 error / 245 warning）、`make unit` **463/0**；lint：本文件 **1972 有效行（本批净增 3）**，仍属历史超限（BACKLOG 记 1966 为批 2a 口径），naming/comments 0 告警；**剩余全局 25**（原口径 26 含 1 条误命中：`src/core/GN_AmCtrl.cpp` 的 `else zNowtime = zRuntime;` 是局部赋值，非文件级全局）；**批 3 + 批 4（2026-11-22 00:5x）**：一次收完余下 21 项，全部为「文件级全局 → 文件内上下文 + static 单实例」，引用点同文件内原位替换（跨 TU 的改用访问器）——`GN_Init`：`GT_UserModuleFileName/GT_UserModule`→`sUserModuleContext`，`GT_SysDir/GT_SysFile/GT_AppDir/GT_AppFile`→`sPathDirContext`（原 `char[]` + `char*` 别名对，构造函数里 `aSysFile = aSysDir + 7`、`aAppFile = aAppDir` 保持原语义）；`GN_Sys`：`GT_StrPasteBuffer`→`sClipboardContext`、`GT_UsThreadTable`→`sUserThreadContext`；`GN_Sound`：`GT_AlDevice/GT_Context`→`sSoundAlContext`；`GN_2D`：`GT_ScissorRect[32]/GT_ScissorRectLv`→`sScissorContext`；`GN_Parallel`：`GT_IdleTasks[2]`→`sParallelContext`；`GN_2Dtype`：`GT_ImageLoadLastTypeName`→`sImageLoadContext`；`GN_File`：`GT_RunLogOpenMode`→`sFileLogContext`；`GN_NetTrans`：`GT_ExternNoticeSoc/GT_ExternNoticePort`→`sNetTransContext`；`GN_FormatScale`：`GT_ImgRowEndID`→`sFormatScaleContext`；`GN_Input` 补收被审计正则漏掉的 8 个多词类型全局（`GT_InputParameSizes`/`RecStartTime`/`PlayStartTime`/`LastOnMouseTime`/`MouseInterval`/`DitherRange`/`Touch0DownTime`/`IO_MappingKeys`）→ `sINPUT_CONTEXT` 新字段；跨 TU 两个组用访问器收敛：`GN_UiPopDlgInner.h` 的 `GT_SimMenuTextOffsetPos/GT_FullSelectPathDlg`→`sPopDlgContext`+`GN_UiPopDlgContext()`（`GN_UI.cpp` 因此新增 `#include "GN_UiPopDlgInner.h"`），`GN_NetBase.h` 的 `GT_ADDR_LEN/GT_ADDR0_LEN`→`sNetAddrContext`+`GN_NetAddrContext()`。**保留 4 个并说明理由**：`GT_MainWindow`（`GN_Main.h:47` `GN_MAIN_API` 公共 ABI，三个测试程序都在用）、`GT_SoundInitError`（仅 `_WIN32_WCE` 分支编译，本机无该工具链不可验证）、`GT_CurAppDir/GT_CurAppDirW (+Len/LenW)`（`GT_CurAppDirW` 被 `GN_File.cpp:879` 的 Android 分支 extern 引用，成对使用）、`GT_TimeFreq`（`LARGE_INTEGER`，`_WIN32_` 分支）。实测 `make` exit=0、`make unit` **463/0**；lint：`GN_NetTrans`(1162)/`GN_Sys`(1389)/`GN_UiFBrowser`(1972) 为历史多功能超限（本批净增数行），naming/comments **0 告警**。**口径订正**：原审计正则漏多词类型（`unsigned int/short/char`、`LARGE_INTEGER`），故 61 为低估；原口径现余 5 行 = 4 个保留项 + 1 条误命中 |
| DA-M3 | UI `cFILE_BROWSER`/`cUI_SCENE` Vulkan 适配（图标纹理合并被显式跳过） | `docs/DEFECT_AUDIT_2026-11-21.md` §1 DA-M3 | **blocked（缺样本+GPU）** | `src/ui/GN_UiFBrowser.cpp:586-587`（跳过合并）、`:1459`；`assets/fileicon/` 不存在 | 补 `assets/fileicon/file_icon(.ini)` 样本并在 GPU 机恢复合并；无样本/无设备不猜修 |
| DA-M1r | 裸 `strcpy`/`strcat` 契约残留 3 处（`GN_String` 旧 ABI：`GN_StrUnSplit` ×2、`GN_StrExtractFromMultiItem`） | `docs/STRCPY_AUDIT_2026-11-21.md` §2 | **blocked（需 ABI 决策）** | 同上文档逐处表（现存裸调用 65 处；B5g/B5h/B5i 已加固 22 行，含 6 处改形） | 改成容量版签名即 ABI 破坏；需产品层决定是否保留旧接口（当前保留） | 改 `_s` 变体 API 或动态分配，会波及存盘/路径契约，需单独立项 |
| B6 | S2b 中景 billboard 云层 | `docs/3D_FEATURE_PLAN.md` §0 #1 | **obsolete** | `tests/mmdViewer/mmdViewer.cpp:542` 写明 C 键停用、L5/立方体云接管 | 不再实现；在计划文档标注被取代，避免误排期 |
| B7 | 备份 | 全局规则 | **partial（压缩+校验完成，待移外盘；2026-11-21 复测）** | 外接 btrfs 默认 `ro`；`udisksctl unmount -b /dev/nvme1n1p1 && udisksctl mount -b /dev/nvme1n1p1 -o rw` 后 mount 显示 rw，但沙箱内写 `/run/media` 仍 EROFS；`danger-full-access` 调用等审批超时（90s/570s，无文件落盘） | 已在 `.local/backups/GnVk-20261121T151448Z.tar.zst`（213157746 B，sha256 `0cc6128c…70a2593`）完成 `tar --zstd` + `zstd -t` + 解包 `rsync --checksum`（0 差异，1817/1817 文件）；**仍差移到 `<外盘>/dsh-backups/GnVk/`**（需要一次外盘写权限批准或用户手工 mv） |
| B7b | 备份复测（CSM 收尾，2026-11-21 22:03） | 全局规则 | **partial（异盘未完成）** | `.local/backups/GnVk-20261121T153906Z.tar.zst`（213143276 B，sha256 `d11557cd…633e88`）：`zstd -t` 通过；解包（`tar -xpf`）后 `rsync -a --checksum --dry-run` **0 行差异**、文件数 1861/1861；归档内 `.local`/`build` 均为 0 条。外盘 `/run/media/ruicat/9294E1A394E18A55`（NTFS rw 挂载点）与 `/run/media/ruicat/3ce0adc2-…`（btrfs）实测 `mkdir` 均报「只读文件系统」EROFS | 需 `sudo mount -o remount,rw <挂载点>`（或 `udisksctl mount -o rw`）后 `mv` 到 `<外盘>/dsh-backups/GnVk/`；同盘副本不算异盘备份；**2026-11-22 复核**：同一归档 `tar --zstd -tvf` 实测 **1875 个普通文件 + 320 目录**，本节「1861/1861」口径有误，以 1875 为准 |

## 3. 依赖与批次顺序

```
B1 (done)
  └─ B2 (CPU, 可单测)
  └─ B3a (shader/CPU 参数，需 GPU 验证)
        └─ B3b (依赖 depth/offset)
              └─ B3c (依赖双缓冲)
  └─ B4a/B4b (GPU)
  └─ B5a (真机) / B5b-B5f (可分批)
```

- **无 GPU 可做**：B2、B5b 的代码审计、B5c 的复现/单测、B5d 的纯代码重复抽取、B5e/B5f 的静态审计。
- **需 GPU/真机**：B3a–c 的像素/帧时间、B4a/b、B5a。
- 每批完成后：`make unit` + 相关 lint；GPU 项另加 `make gpuCheck` 或桌面截图；失败不标 done。

## 4. 剩余项处置（2026-11-21，round 11 状态）

| ID | 处置 | 原因 / 复算入口 |
|---|---|---|
| B3c-2 | **done（软件路径，2026-11-21）** | `gn_cornell_probe_update.comp` + 原地混合 + barrier；parity/age/收敛/inactive/切光见 §10.7「2d 实测」；仅 HW ray-query 变体与脏表压实待做 |
| B3b-full | **done（GPU 6 轴，2026-11-21）** | `gn_cornell_probe_relocation.comp` + `CORNELL_PROBE_RELOCATION_GPU=1`；证据见 §10.7；视觉收益仍未判定，RT 重测仍待硬件 |
| B4a/B4b | **硬件/驱动阻塞** | 需要稳定的硬件 ray query/GPU；本机无 `/dev/dri`，llvmpipe `gpuCheck` core dump |
| B5a | **硬件阻塞** | imoScene 阴影首帧 amdgpu page fault；仅真机可复现，没有设备不猜修 |
| B2b-GPU | **硬件阻塞** | `CollectGarbage` 的 `vkDestroyPipeline` 销毁时机需真机 + 校验层 |
| UI 文件超限 | **3/3 已拆；`GN_UiPopDlgRun.cpp` 登记 no-split 债** | `GN_UiListTree`→658+522、`GN_UI`→771+753、`GN_UiPopDlg`→121+486+779+1127+546+167；三批 lint 0 违规，仅 Run 1036 有效行 UNSPLITTABLE 告警（单个 `cSIM_DIALOG::UpdateRun` 状态机，需独立批次重构才能压到 <1000）；Lead 复核 `make` 0、`make shaders` 47、`make unit` 422/0 |
| B5d-S25 | **done（2026-11-21）** | `brdfLighting.glsl`（纯数学）+ `sceneLighting.glsl`（UBO 依赖函数）；两 frag 从 1131 行降到 808 行 + 192 行公共片段；引擎截图 `00550dcf…` 逐字节不变 |
| B5e/B5f | **done（2026-11-21）** | B5f：36→0（`docs/BUFFER_SIZE_AUDIT.md`）；B5e：5 条逐条复核（`docs/DEFECT_AUDIT_2026-11-21.md`），仅 R1 仍成立且无复现，登记技术债 |
| B7 | **环境阻塞** | 外接盘 ro；需用户重挂 rw 后执行 `tar --zstd` + 校验 |

> 结论：本轮接管把可无 GPU 验证的契约/缺陷处理到可交付状态；剩余项要么需要专用 compute/RT 批次，
> 要么需要真机/可写备份盘。没有把未跑的项目标成 done。

## 6. 接管收口（2026-11-21，round 12）

- **cornellBox 默认切运行时 DDGI（round 22）**：设 `CORNELL_PROBES` 且未显式给 `CORNELL_PROBE_ASYNC` 时，
  探针路径默认走异步更新（64/帧、spp 4、hysteresis 0.9）；`CORNELL_PROBE_ASYNC=0` 退回静态 GPV4，
  截图仍 `ebe115fd…`。A/B：静态 vs 默认运行时 RMSE 0.898 / MAE 0.513 / max 7；2 帧 ages{0:12544,1:128}。
  relocation 仍 opt-in（`CORNELL_PROBE_RELOCATION_GPU=1`）。
- **round 13（新会话，2026-11-21 12:3x）**：B3c-2 软件路径实现并验收（parity 逐位一致、age 递增、
  DC 收敛 1.3e-4、默认路径逐字节不变）；新增共用 `cornellRayLib.glsl`（`gn_cornell_rt.comp`
  改 include 后截图逐字节一致）；`make shaders`=46、`make unit`=415。剩余：`_rq` 变体、inactive 跳过、
  切光 age 归零 A/B、真机视觉。
- **B3b-full**：GPU 6 轴 relocation 完成并验收（`moved=674/12672`、`max=0.114`、无 NaN/clamp 越界、
  两次运行逐位一致）；视觉收益仍不判定。
- **B5d-S25（完成）**：7 个纯数学函数 → `brdfLighting.glsl`；统一 UBO 实例/字段名后，
  其余 6 个函数 → `sceneLighting.glsl`；pbr 525→363、deferred 606→445；引擎截图 `00550dcf…` 逐字节一致。
- **B5e（审计完成）**：`docs/DEFECT_AUDIT_2026-11-21.md` 逐条复核 5 条残余；仅 R1 `memcpy(this)` UB
  仍成立（无复现，重写 `cFRAME_RC` 拷贝赋值属独立批次），其余已处置/口径不可复核/描述过时。
- **测试补齐（2026-11-21）**：新增 `tests/unit/testStringSafety.cpp`（容量/截断/UTF-8 字节语义 7 例）、
  `tests/unit/testShaderContract.cpp`（S-25 与 Cornell 共用 GLSL 防重复契约 5 例）；扩 `testProbeUpdate.cpp`
  （budget>count、alpha 单调）与 `testProbeRelocation.cpp`（偏移随 score 缩放）；`make unit` 420→**434/0**。
- **B5f（收口）**：`TODO: verify buffer size` **36→0**（29 处加固/核销 + 7 处转显式契约注释）；
  新增 `GN_StrCopyToLimit` 并接入 8 处调用方缓冲拷贝；`make unit` 420/0；详见 `docs/BUFFER_SIZE_AUDIT.md`。
- **B5g（DEFECT_AUDIT 遗留项处置，2026-11-21）**：`cFRAME_RC` 的 `memcpy(this)`/逐字节覆写改成逐字段复制
  （`src/core/GN_FrameCopy.cpp`），并把 `zSound` 排除出复制（原字节拷贝让两个对象共用一个 OpenAL 发声源，
  析构时双重释放）；新增 `tests/unit/testFrameCopy.cpp` 4 例，`make unit` 434→**438/0**。
  8 处裸 `strcpy` 加固（逐处行号见 `docs/STRCPY_AUDIT_2026-11-21.md` §1）；
  71 处裸 `strcpy`/`strcat` 逐处登记（B5h 后：加固 20 行 / 契约 5 / 已核 45 / 非代码 2）；
  全量 `make -B` 警告 **24600** 条（`build/full_build_warn_20261121.log`）。
  `docs/DEFECT_ANALYSIS.md` 重写为可复核台账（5 条逐条 + 17 条计数退役）。
- **B5h（契约型 strcpy/strcat 加固，2026-11-21）**：12 处固定 384/64 字节缓冲改为「容量守卫或截断拷贝」——
  ZIP 路径 6 处（`GT_TMP_ZIP_ADD_DIR_NAME_MAKE` 与 `AddFile`/`find_str`/`_dir+len` 守卫）、
  2D 场景路径 2 处（超长报错退出 / 忽略导出路径）、图像名 64B 契约 2 处（截断 + `num` 与实际字节一致）、
  媒体打开路径 1 处、UDP 默认回声 1 处（等长临时缓冲，发送字节数不变）。
  本轮触碰 12 处（其中 10 处保留调用加守卫、2 处改成非 `strcpy` 形态）；连同 B5g 的 2 处，共 4 处改形。
  现存裸调用 **67**、契约残留 **5**。
  `make unit` **438/0**；全量 `make -B` 警告 **24600** 条（`build/full_build_warn_20261121b.log`）。
- **B5i（图像名转换容量化，2026-11-21）**：新增 `GT_ConvertImageNameForLoad_s(out, cap, name, len)`（按容量截断，
  再处理末尾 `"0000"`/`".png"`），旧 ABI `GT_ConvertImageNameForLoad` 转为容量版包装；
  场景装载传 `sizeof(file_path)` 余量、镜头光环改为 3 参 `zDataLoad(rpos, path, cap)`。
  现存裸调用 **65**、契约残留 **3**（仅 `GN_String` 旧 ABI）；新增 `tests/unit/testImageNameConvert.cpp` 5 例（含哨兵越界断言），
  `make unit` **443/0**；全量 `make -B` 警告 **24600**（`build/full_build_warn_20261121c.log`）。
- **B5j（`GN_Format` 拆分，2026-11-21 19:32）**：`GN_Format.cpp`（3419 行）按功能拆成 8 个文件（Pix/Scale/ImgFile/Audio/Data/Plugin/Values/Zip），公共头未改；
  `GN_FormatInternal.h` 收敛插件链宏与 `GT_ERR_FileCreateFail` extern；`make` ✅、`make unit` **443/0**、lint 9 文件 **0 违规 0 告警**；
  独立产物 `LIB_STEM=GN_mini_coord BUILD_DIR=build/obj_coord` 全量链接 EXIT=0（验证后已清理）；并发写域约定见 `docs/ENGINE_COORDINATION_2026-11-21.md`。
- **B5k（`GN_Frame` 拆分，2026-11-21 20:05）**：`GN_Frame.cpp`（2899 行）按类拆成 4 个文件（GN_FrameRc/Launcher/Font/LensFlare.cpp，
  各 688/721/778/724 物理行），共享宏与两个 helper 收敛到 `GN_FrameInternal.h`；公共头未改；
  私有产物验证：lib 构建 EXIT=0、`make unit` **443/0**、lint 6 文件 0 违规 0 告警；plain `make` 待卡窗口。
- **技术债（更新，2026-11-21）**：3 个超限 UI 文件**已全部拆分**——`GN_UiPopDlg.cpp` 由并发会话拆成
  121 + `GN_UiPopDlgMenu.cpp` 486 + `GN_UiPopDlgSetup.cpp` 779 + `GN_UiPopDlgRun.cpp` 1127 + `GN_UiPopDlgIO.cpp` 546
  + `GN_UiPopDlgInner.h` 167（B5f 加固保留、函数定义数 93=93、build/unit 通过）；其中 Run 文件 1036 有效行，
  头部声明 `code-modularity: no-split`（单个 `cSIM_DIALOG::UpdateRun` 状态机），lint 记 1 告警/技术债。
  另两个：`GN_UiListTree.cpp` 1171 → 658 + `GN_UiListTreeNodes.cpp` 518；
  `GN_UI.cpp` 1519 → 771 + `GN_UiDataIO.cpp` 753；三批均 `make` 成功、`make unit` 422/0。
  `GN_UiPopDlg` 拆分由并发会话在 Lead 复核期间完成（文件生成时间晚于首轮复核），已逐项核对：
  函数定义数 93=93、B5f 的 `GN_StrCopyToLimit` 加固保留（Inner.h 内辅助 + 5 处调用）、lint 仅 Run 一处告警。
  **存量债**：`GN_Sys.cpp` 1383 有效行超 1000（原有），本次等行迁移（`_s` 变体移到 `GN_SysSafe.cpp`），未增长。
  属独立批次，登记技术债。
- **备份**：压缩包与自校验已在 `.local/backups/` 完成（1817 文件、rsync 0 差异、zstd -t OK），
  但落外盘仍被拒（沙箱对 /run/media 返回 EROFS，danger-full-access 审批无应答），属**未完成**。

- **已完成并验证**：B1（多层渲染 P0，9 单测）、B2a（spec/key，单测）、B2b API（GC，无设备 no-op）、
  B3a/B3b-lite（Cornell 代码 + llvmpipe 出图，视觉未判定）、B3c-1（调度/alpha/混合 4 单测）、
  B5b/B5c、B5d-S26/S27。最终 `make unit` = **413/0**，`make shaders` = **45/45**。
- **未完成**：B3c-2 GPU compute、B3b-full、B4a/b、B5d-S25、B5e/B5f、B2b-GPU 验证、B5a、B7。
- **判定**：这些不是“未处理”，而是已逐项给出处置（专用批次 / 硬件阻塞 / 技术债 / 环境阻塞）；
  其中 B3c-2 与 B3b-full 仍需实现，不能算完成。

## 5. 复算命令

```bash
cd /home/ruicat/下载/GnVk/GnVk
make unit                                   # 当前 413/0（B3c-1 后）
bash ~/.dsh/skills/code-modularity/scripts/lintCodeModularity.sh \
  src/graphics/GN_RenderPipeline.h src/graphics/GN_RenderPipeline.cpp \
  tests/unit/testRenderPipeline.cpp --naming --comments
grep -rn "TODO\|FIXME" src tests | wc -l     # 全仓 TODO/FIXME（口径随代码变化）
grep -rn "TODO: verify buffer size" src | wc -l # B5f 口径：36 → 0（2026-11-21 收口）

# B3c-2 异步探针更新（lavapipe 可跑；两次回读 hash 应一致）
LD_LIBRARY_PATH=. VK_ICD_FILENAMES=$PWD/.local/probes/lvp_icd.json CORNELL_GPU_RT=1 \
  CORNELL_RT_TARGET=1000000 CORNELL_PROBE_ASYNC=1 CORNELL_PROBE_UPDATE_BUDGET=0 \
  CORNELL_PROBE_ASYNC_VERIFY=1 CORNELL_PROBE_ASYNC_FRAMES=1 \
  CORNELL_PROBE_ASYNC_DUMP=/tmp/probe_oneshot ./build/cornellBox
LD_LIBRARY_PATH=. VK_ICD_FILENAMES=$PWD/.local/probes/lvp_icd.json CORNELL_GPU_RT=1 \
  CORNELL_RT_TARGET=1000000 CORNELL_PROBE_ASYNC=1 CORNELL_PROBE_UPDATE_BUDGET=64 \
  CORNELL_PROBE_ASYNC_VERIFY=1 CORNELL_PROBE_ASYNC_FRAMES=198 \
  CORNELL_PROBE_ASYNC_DUMP=/tmp/probe_budget ./build/cornellBox   # 约 1–2 分钟
```

## 7. 引擎接入验收（面向游戏的优化必须真正接入引擎）

> 用户口径（2026-11-21）：「引擎是面向游戏的优化的实现都需要真正接入引擎的」。
> 含义：游戏向的优化/能力必须落在引擎默认路径与公开 API 上；`tests/imoScene` 的 env 开关、公开字段直写、替换 sampler 等只能作为临时调试/绕行，不能成为唯一入口。

| ID | 要求 | 当前状态 | 验收判据 |
|---|---|---|---|
| G1 | CSM 4 级阴影在引擎内默认可用：E1（退休缓冲同帧 UAF）+ E2（4 级 shadow UBO）修好后，**默认运行**（不带 `IMO_SCENE_SHADOWS`）即走引擎 CSM；demo 环境变量降级为调试开关 | **done（2026-11-21 22:05）**：E1/E2 code + 真机 CSM 3/3 + 默认启动均通过；demo 默认开、`IMO_SCENE_SHADOWS=0` 关（patch sha `e79af1a9…80d0`，demo diff sha `861d08f1…2e2b`） | 默认启动无 amdgpu page fault/GPU reset；4 级矩阵 A/B 正确；不依赖 env 才生效；真机验证受限：本机无 `/dev/dri`；AliyunHost SSH 公钥被拒；llvmpipe 下 `IMO_SCENE_SHADOWS=0/1` 均在 present/swapchain 段错误（baseline 同崩）⇒ 需用户在 amdgpu 桌面跑 3 次；待命补丁：`.local/patches/imoScene-csm-default-on.patch`（4505 B，sha256 `e79af1a9…f7809d0`，仅 `tests/imoScene/{imoSceneMain,imoSceneShadow,README}`；`git apply --check` 通过、**未应用**；当前 `aEnabled(false)` + `IMO_SCENE_SHADOWS=1` 开启）；**真机信号 1/3（2026-11-21 21:4x，用户 amdgpu 桌面）**：`IMO_SCENE_SHADOWS=1 timeout 20 ./build/imoScene` 达 `阴影: 开` → `首帧: 录制完成`，20s timeout 正常退出；无 amdgpu page fault / ring timeout / GPU reset；仅 resize 后布料/贴花/降水未重建的既有警告（见 F1）。按约定需 3/3 才 apply 待命 patch；**3/3 通过（run2/3 日志 `.local/tmp/csm_run{2,3}.log`，fault/reset 命中 0）**；待命 patch 已 apply：默认 `aEnabled(true)`、`IMO_SCENE_SHADOWS=0` 关闭（`imoSceneShadow.cpp:43/49-51`）；apply 后 demo diff 198 行 sha256 `861d08f1…2e2b`，patch sha 不变；Lead 独立验 `make imoScene` exit=0、`make unit` 443/0、`tests/imoScene` lint 0/0；**剩余**：用户默认启动一次确认日志 `阴影: 开（IMO_SCENE_SHADOWS=0 关闭）`；默认启动日志 `/home/ruicat/下载/GnVk/GnVk/.local/tmp/csm_default.log`（3830 B，sha256 `d2ac04c5…72e04`；第 48/51 行 `阴影: 开（IMO_SCENE_SHADOWS=0 关闭）` / `首帧: 录制完成`，fault grep 0）；run2/3 日志 sha `cbed1241…58bcb`/`f2b77ebe…d53d5`；run1 派生摘录 `csm_run1_key.log`（607 B，sha `2ae578b9…42105`，无 exit code） |
| G1b | 室内 GI（`IMO_SCENE_INDOOR_GI`）应由引擎环境光/IBL 数据决定，而非 demo env 二选一 | §7.1 审计 | **done（2026-11-21 22:44，CPU+API；视觉 A/B 待真机）** | `tests/imoScene/imoSceneMain.cpp:277-293`（env 选 SH9/天空 IBL）；引擎侧无环境光选择 API | 设计环境光/IBL 选择 API（或资产字段），demo 降为调试开关；无 GPU 可做接口+单测；实装：新增 src/graphics/GN_Environment.h/.cpp（buildEnvironmentGi 室内 SH9/参数钳制 + validateEnvironmentLighting）、GN_Vk.h 声明、_Vulkan/vkEnvironment.cpp 实现 GN_VkSetEnvironmentLighting（INDOOR 写 SH9；SKY 恢复烘焙 UBO）；demo 改调引擎 API 并删本地 applyRoomAmbient（tests/imoScene/imoSceneMain.cpp、imoSceneRoom.cpp/.h），IMO_SCENE_INDOOR_GI 降为调试覆盖；实跑 make exit=0、make unit 455/0、make imoScene exit=0、lint 0/0（7 文件）；真机视觉 A/B 仍待 GPU |
| G2 | `sIMAGE` sRGB/wrap 成为引擎正式 API：引擎侧字段 + 载入参数（或新 API）、采样器按 wrap 创建、PBR shader 按 sRGB decode；glTF 载入把 sampler 值传入引擎 | E3 code-done：`sIMAGE.vkWrapU/vkWrapV/vkSrgb` + `ImageLoadFromMem(...,srgb,wrapU,wrapV)`；引擎 3D 贴图路径（vkTexture）本就用 SRGB+wrap；demo 绕过已删并改调 `ImageLoadFromMem`（lead2 21:01），lead 独立验证 `make imoScene` exit=0 / `make unit` 443/0 | imoScene 删除该绕过并改调引擎 API；默认运行 REPEAT/sRGB 观感正确；绕过锚点 `tests/imoScene/imoSceneUpload.cpp:22/37/168`（报方 20:20 给出） |
| G3 | 其他演示层开关审计：`IMO_SCENE_*` 逐个分类「调试开关」或「引擎能力」；引擎能力必须有默认路径 | **done（2026-11-21 21:20 审计）**：21 个 `IMO_SCENE_*` 已分类，见 §7.1 | 产出清单；不存在「只有 demo 才能启用的引擎优化」 |
| G4 | 收口命令与证据：plain `make`、`make unit`、默认运行；GPU 机 `gpuCheck`/真机 | 共享 `make` ✅ / `make unit` 443/0 已跑（20:55）；真机项待 GPU | 命令、输出与主机名写入本文件与协作文档 |

## 7.1 G3 开关审计（2026-11-21，读 `tests/imoScene/**`）

> 审计口径：21 个 `IMO_SCENE_*` 环境变量（`rg -o 'getenv("IMO_SCENE_[A-Z0-9_]+' tests/imoScene | sort -u` 口径一致，含重复引用后共 21 个不同名字）。
> 分类规则：引擎能力若只能靠 demo env 开 → 必须回到引擎默认路径/API；纯调试/数据开关允许保留在 demo。

| 类别 | 开关 | 处置 |
|---|---|---|
| **引擎能力被 demo 开关替代** | `IMO_SCENE_SHADOWS`（CSM 4 级）、`IMO_SCENE_INDOOR_GI`（室内 SH9 vs 天空 IBL） | CSM → G1（真机 3 次无 fault 后默认开、env 降为调试）；INDOOR_GI → **新增 G1b**：环境光/IBL 选择应由引擎数据/API 决定，不应由 demo env 二选一 |
| 调试/检查（允许保留） | `IMO_SCENE_NO_TEXTURES`、`IMO_SCENE_FORCE_OPAQUE`、`IMO_SCENE_ALBEDO`、`IMO_SCENE_SHOW`、`IMO_SCENE_HIDE`、`IMO_SCENE_MAX_MESHES`、`IMO_SCENE_LIST`、`IMO_SCENE_INSPECT`、`IMO_SCENE_SRGB`（E3 A/B）、`IMO_SCENE_SHADOW_INTENSITY` | 保留；README 标注为调试开关 |
| 数据/资产选择（demo 数据，允许保留） | `IMO_SCENE_MODEL`、`IMO_SCENE_ROOM`、`IMO_SCENE_NO_ROOM`、`IMO_SCENE_LAMPS`、`IMO_SCENE_AMBIENT`、`IMO_SCENE_EV` | 保留；属演示数据装载，不是引擎能力 |
| demo-only 效果（不得冒充引擎能力） | `IMO_SCENE_BLOB`、`IMO_SCENE_BLOB_SHADOW`、`IMO_SCENE_BLOB_RADIUS` | 保留为 imoScene 演示特性；引擎侧不承诺 blob shadow 为通用能力 |

**结论**：21 个开关中只有 2 个是「引擎能力被 demo 开关替代」（CSM、室内 GI），其中 CSM 已有 G1 待真机；室内 GI 需新增 G1b（引擎侧环境光/IBL 选择 API）。blob shadow 是 demo 专有效果，需在文档里明确不算引擎能力。其余 18 个为调试/数据开关，保留。

## 8. 遗留项收敛台账（2026-11-21 23:1x，goal-6fde2e21 轮次 3）

> 口径：每项只有两种终态——**done**（有实跑证据）或 **blocked**（有阻塞原因、所需硬件与复算命令）。GPU/真机项在无 `/dev/dri` 的主机上不得标 done。

| 项 | 收敛状态 | 证据 | 复算 / 阻塞条件 |
|---|---|---|---|
| B1 多层渲染 P0 | done | `GN_RenderPipeline` + 8 例；`make unit` 463/0 | `make unit` |
| B2 P1a/P1b | done（API）；GPU 销毁时机 blocked | 特殊化常量/键版本/GC API 已实现；真实设备销毁未验 | GPU 机：`make gpuCheck` + 帧边界回收冒烟 |
| B3a/B3b 视觉 A/B | blocked（视觉） | code done；cornell 截图对比未做 | GPU 机：`make gpuCheck` + 同机位截图 A/B |
| B3c 软件路径 | done；HW ray-query blocked | 软件 compute 已验；HW 变体未做 | GPU 机（ray query）：`make gpuCheck` |
| B4a/B4b | blocked（GPU） | SSGI ray query / 探针动态化 planned | GPU 机：`make gpuCheck` + SSGI 场景 |
| B5a imoScene 阴影 fault | blocked（GPU） | 需真机复现 | amdgpu 桌面：`IMO_SCENE_SHADOWS=1 timeout 20 ./build/imoScene` |
| B5b–B5n | done | 见 §2 各行（拆分/加固/审计/P2a/P3/P5） | `make unit`、lint |
| E1 | done | 真机 CSM 3/3 + 默认启动 | 日志 `.local/tmp/csm_*.log` |
| E2/E3 | code-done；视觉 A/B blocked | 引擎 API + demo 迁移完成 | GPU 机：截图 A/B（4 级阴影矩阵、sRGB/wrap） |
| F1 | done（误报） | `M-known-defects-34` verified | 无需动作 |
| G1/G1b/G2/G3/G4 | done（CPU/API；G1b 视觉 blocked） | 真机 CSM 3/3；室内 GI 引擎 API；开关审计；收口命令 | `make`、`make unit`、`make imoScene` |
| B6 | obsolete | 云层已由 L5/立方体云接管 | 文档标注即可（本轮已完成） |
| B7/B7b 备份 | partial（**2026-11-23 已由 B7d 收口为 done**） | 同盘 staging 1875/1875、rsync 0 差异；外盘 ro。**2026-11-22 复核**：`tar --zstd -tvf .local/backups/GnVk-20261121T153906Z.tar.zst` 实测 **1875 个普通文件 + 320 目录 = 2195 条**，与 §2 B7b 的「1861/1861」不符（后者口径有误，以 1875 为准）；本会话沙箱为 workspace-write，写 `/run/media/**` 仍会被策略拒绝 | 重挂 rw 后：`udisksctl unmount -b /dev/nvme1n1p1 && udisksctl mount -b /dev/nvme1n1p1 -o rw`；再 `bash .dsh/backupProject.sh` |
| DA-M1r 3 处旧 ABI | blocked（待用户决策） | 需 ABI 决策（保持/升 ABI/逐处迁移） | 用户答复后执行 |
| B7c | 备份复测（DA-M2 收口后，2026-11-22 00:48） | **partial（同盘 staging 完成，异盘 blocked）** | `.local/backups/GnVk-20261121T164820Z.tar.zst`（202846826 B，sha256 `5c4d0205…`）：`tar --zstd` exit=0、`zstd -t` exit=0、解包后 `rsync -a --checksum --dry-run` **0 行差异**、文件数 **2102/2102**、归档 2422 条（含 .git）。异盘 `/run/media/ruicat/3ce0adc2-…`（dev 78 ≠ 项目 dev 45）实测 `touch` 报「只读文件系统」（EROFS，非沙箱拒绝） | 用户执行 `udisksctl unmount -b /dev/nvme1n1p1 && udisksctl mount -b /dev/nvme1n1p1 -o rw` 后：`mv .local/backups/GnVk-20261121T164820Z.tar.zst* /run/media/ruicat/3ce0adc2-3284-475f-9780-5ed762f474cd/dsh-backups/GnVk/` |
| B10 | 绕序口径冲突 | **closed-doc-fixed（2026-11-24，用户确认画面正常）** | 实测（2026-11-23 真机，两次独立实验）：PBR pass 把**世界 CCW** 三角形判为背面、世界 CW 才可见（与 `kQuadIndexBA=(0,2,1)` 同口径）。而 `docs/AXIS_UNIFICATION_PLAN.md:62-68` 写「世界正面 CCW」；`src/graphics/GN_3Dasset.cpp:164-167` 的 glTF 导入**原样拷贝索引、不翻转**（glTF 原生 CCW）。两者只有一种能成立：要么真实场景对资产做了镜像变换（引擎自洽，本次把测试改成 CW 是对的），要么 imoScene 里角色/房间**内外翻转**（引擎 frontFace 应改 CW，测试应还原 CCW）。本轮**只把测试对齐引擎现状，未动引擎与文档** | **已定论（2026-11-24）**：用户提供 imoScene 实拍截图——角色正脸/衣物/室内均正常，无内外翻转 ⇒ 引擎与现有资产自洽，**引擎不动**。可执行规则已补进 `docs/AXIS_UNIFICATION_PLAN.md`（§1 表后「2026-11-23 真机复核」块）：在 Y 翻转投影 + 正高度视口 + `frontFace=CCW` 下，要写成「自相机看是 CW」的顶点顺序才可见；`kQuadIndexBA` 与本次立方体修正即此口径。**残余疑问**（不阻塞）：glTF 未翻转却仍正确（资产本身 CW / 转换链另有镜像 / 双面材质差异）未定论，澄清前禁止改 `descPBR.aFrontFace` |
| B9 | imoScene 首帧「卡住」复核（2026-11-23 23:4x，真机 RADV） | **误报：不是卡死** | 进程实测 `Sl`、8 秒 CPU 占 51.2%、9 线程全在 poll/futex（无 Vulkan 死等）；加帧计数后 `IMO_SCENE_SHADOWS=0` 14s 渲染 **720 帧（~51fps）**、`=1`（阴影开）同样 **720 帧**；阴影开 3/3 次运行均 **480+ 帧、0 error/fault/reset**。日志里「此后卡住 = present/swapchain」是首帧后**无条件打印的提示文本**，不是检测到的挂起 | 复算：`IMO_SCENE_SHADOWS=1 timeout 12 ./build/imoScene 2>&1 | grep 已渲染`（应持续出现「已渲染 N 帧」） |
| B8 | gpuCheck 真机回归（2026-11-23 22:5x，AMD Radeon 780M / RADV PHOENIX，Vulkan 1.4.363） | **101 项：100 PASS / 1 FAIL**（进场时 97 PASS / 4 FAIL） | 修 3 项：`cull.draw.origin`/`cull.draw.plusX`/`batch.draw` —— 根因是**测试立方体绕序与 PBR pass 有效正面相反**（实测：世界 CCW 的 `(0,1,2)/(0,2,3)` 被判背面、整块不可见；改成与 `kQuadIndexBA` 同口径后 3 项转绿，证据：全图最亮像素 0.000→0.231）。**余 1 项 `cull.debugLine` 未解**：`GN_VkDrawDebugLines` 确被调用（pipeline 非空、samples=4、extent=640x480）、`vkMapMemory` res=0、场景 UBO 实测 `m0=0.250 m5=-0.333 m15=1.000`（投影正确）、重编 47 个着色器后仍 0 像素（全图扫描 `redPixels=0`、`bestR=0.050`=clear 色） | 复算：`make gpuCheck`（本机 → 100/1）；`make unit` 463/0。**debugLine 追加排除（2026-11-23 23:3x）**：换成 `TRIANGLE_LIST` + 覆盖全屏的大三角形、以及把管线换成已知可用的 `pipelineLayout3D`/`descriptorSet3D`，仍 0 像素 ⇒ 问题不在线光栅/LINE_LIST，而在该管线自身（管线描述与顶点布局已逐项打印核对：stride=28、attr0 off0 fmt106、attr1 off12 fmt109、topo/blend/samples/extent 全部匹配，SPIR-V 反汇编确认 set0/binding0 UBO 与 loc0/1 属性正确）。待查需要校验层或 RenderDoc（本机 `/usr/share/vulkan/explicit_layer.d` 只有 gamescope/mesa-device-select，无 Khronos 校验层） |
| B7d | 异盘备份完成（2026-11-23 22:38，DA-M2 收口后；用户已把外盘 remount rw） | **done（异盘）** | `/run/media/ruicat/3ce0adc2-3284-475f-9780-5ed762f474cd/dsh-backups/GnVk/GnVk-20261123T144321Z.tar.zst`（**最终版**：202881834 B，归档 2435 条，sha256 `bd0583a0…f9070d`，对应提交 `c0b453c`；同批前一份 `GnVk-20261123T143728Z` 亦通过校验并保留）：`zstd -t` exit=0、异盘侧 `sha256sum -c` **成功**、解包后 `rsync -a --checksum --dry-run` **0 行差异**、**2106/2106** 文件；源设备号 45 ≠ 目标设备号 87（异盘成立）。同盘 staging `GnVk-20261121T164820Z` 保留未删 | 复算：`zstd -t <file>`；`(cd <dir> && sha256sum -c *.sha256)`；**披露**：最终归档生成于本行台账改写之前（差 1 个 doc-only 提交，无源码差异），下一轮备份会包含本行。**保留策略（2026-11-23 执行）**：按全局规则「备份只保留最近 3 份」，异盘 76 → **3**（13G → 600M）、同盘 staging 13 → **3**（2.6G → 601M），删除清单见 `.local/tmp/backup_prune_20261123T144200Z.log`（166 条 = 83 归档 + 83 sha256）。**副作用**：文档里引用的 `GnVk-20261121T102533Z`（B5i 恢复点）、`GnVk-20261121T151448Z`（批 1b 事故恢复点）等旧归档已不存在，这些历史状态改用 git 历史找回（`918a573` 及其前后提交）。**2026-11-23 23:19 补份**（GPU 修复后）：`GnVk-20261123T151913Z.tar.zst`（202914250 B，sha256 `a7b90aa3…`，对应提交 `5828ab6`；`zstd -t` 通过、`sha256sum -c` 式确认、解包 `rsync --checksum` **0 差异**、**2128/2128** 文件；异盘保留 3 份） |
| DA-M2 61 全局状态→Context | **done（CPU 可验部分，2026-11-22）**：批 1a/1b/2a/2b/3/4 全收；**保留 4 个**有明确理由（见 §2 行） | `make` exit=0、`make unit` 463/0；lint naming/comments 0 告警；`GN_NetTrans`/`GN_Sys`/`GN_UiFBrowser` 历史超限 | 保留项复算：`GN_Main.h:47`（公共 ABI）、`GN_Sound.cpp:43`（WCE 分支）、`GN_Sys.cpp:111`（Android extern 对）、`GN_Sys.cpp:1087`（`LARGE_INTEGER`）；多词类型盲区已在 §2 记为口径订正 |
| DA-M3 UI Vulkan | blocked（环境+GPU） | 缺 Vulkan UI samples 且无 GPU | 提供 samples/GPU 后评估 |

**CPU 可验剩余**：仅 DA-M2（分批迁移）。其余全部 done 或 blocked（GPU/ABI/环境）。

> 单页总结（进度/blocked/下一步）：`docs/REMAINING_WORK_SUMMARY_2026-11-21.md`。
