# Changelog

## 2026-10-30 — 正面绕序修正 + MMD 法线 + 降水/云/贴花拿到真矩阵（gpuCheck 99/99，291 cases）

- **世界 pass 的 `frontFace` 修正为 CCW**（`GN_Space.cpp`、`vk3dInit.cpp`、`vk3dInitScene.cpp`、`vkMaterial.cpp`）：
  旧 U2b 把「Y 翻转 ⇒ CW」写反了。正高度视口 + 投影 Y 翻转下，世界 CCW 内容在帧缓冲仍是 CCW，
  含 Y 翻转的 3D pass 必须 CCW；只有不翻 Y 的阴影 / identity 用例用 CW。
  证据：llvmpipe（CPU Vulkan）`make gpuCheck` 旧规则 **78 通过 / 18 失败**，修正后 **99/99**。
- **MMD 模型法线反**（`src/mmd/mmdMeshBuilder.cpp`）：镜像 X 会反转绕序，而原始 PMD/PMX 的绕序与顶点法线一致，
  只镜像不反绕序 ⇒ 模型内外翻转、光照像法线反了。两个 builder 现在镜像后再 `reverseTriangleWinding`。
  实测 `mode.pmd`：原始 agree=6189/disagree=40；修复前镜像后 40/6189；修复后 6189/40。
  新增单测 `mmdPmdMirrorKeepsWindingConsistentWithNormals`。
- **降水/云/贴花画不出来的根因**（`tests/mmdViewer/mmdViewer.cpp`）：调用点写成
  `drawPrecipitation(&viewProjection, …)` / `drawClouds(&viewProjection, …)` / `drawDecals(&viewProjection)`，
  而这三个函数收的是 `const sMAT44&`；指针被 `sMAT44(bool)` 隐式转成 `bool` 再构造成**单位矩阵**，
  于是世界坐标被当作裁剪空间坐标：绝大多数粒子被裁掉、剩下的画在错误位置（看起来「往上走」、不在包围盒里）。
  修正为传矩阵本身；并把 `sMAT44(bool)` 改成 `explicit`，让同类错误在编译期报错。
- **V 键粒子包围盒只画雨**（`mmdViewer.cpp`）：改为雨、雪两套系统各画最近的 16 个。
- **查看器启动天气开关**：`MMD_VIEWER_WEATHER=snow|rain|fog|storm|cloudy|clear`（便于脚本化复现）。
- **降水密度与分层**：池容量 61440/81920 → **16384/24576**、发射率 360000/150000 → **60000/30000**
  （池容量才是屏幕密度上限；原先过密）。同时修发射器满池逻辑（`GN_3Dparticle.cpp`）：
  旧实现池满时把待发射积压钳到 1，存活数被寿命抽干到近空再瞬间灌满，表现为**分层/空白间隔**；
  现在保留 0.1 s 额度，槽位一释放就补齐。单测 `particleFullPoolStaysSteady` 锁定
  （稳态 min=max=容量）。
- **gpuCheck 新增回归**：`precipitation.snowCurtain`（81920 颗、65 m 外）、
  `precipitation.orientation`（世界 -Y 必须落在屏幕下半）、`precipitation.screenAgreement`
  （CPU 投影与渲染像素质心一致，实测 expect=actual=(255,222)）。

## 2026-10-29 — 竖直方向一致性修正 + 四元数自由视角 + 包围盒/坐标轴（95/95，275 cases）

- **降水/贴花竖直方向**（`_Vulkan/vkPrecipitation.cpp`、`_Vulkan/vkDecal.cpp`）：两条管线的视口原来是
  正高度，与场景（PBR 用 `y=H, height=-H` 的负高度视口）相反 ⇒ 雨丝沿速度方向朝上、贴花投到视线上方。
  现在统一为负高度视口，降水与场景同向。
- **地面法线**（`tests/mmdViewer/mmdViewer.cpp`）：地面顶点法线改为 `(0,-1,0)`——本引擎竖直方向是镜像的
  （MMD 左手系模型恰好抵消），朝上的地面要按 -Y 传，否则光照把地面当背面而发黑。
- **自由视角改用四元数**（`tests/mmdViewer/mmdViewer.cpp`）：`gFreeRotation`（`GNg::sQUAT`）累积视角，
  鼠标水平位移绕**世界 +Y**、垂直位移绕**相机右轴**，`pitchQuat * yawQuat * 当前朝向` 左乘后归一化，
  前向/右向/上向由 `QuatRotateVec3` 旋转基向量得到（不做欧拉角累加，无万向锁/roll 漂移）；
  W/S/A/D 沿朝向平移、Q/E 沿世界竖直升降、Shift 加速、左/右键拖动转视角。
- **包围盒 + 坐标轴可视化**（新增，G 键切换）：`GN_VkDrawDebugLines` 画原点坐标轴
  （+X 红 / +Y 绿 / +Z 蓝，长度 0.6×模型高）与模型世界 AABB 的 12 条棱（黄色线框）。
- 验收：`make unit` = 275 cases 0 failed；`make gpuCheck` = 95/95 PASS；`build/mmdViewer` 编译通过。

## 2026-10-29 — 降水形状 v2 + 地面 + 贴花系统（94/94，275 cases）

- **降水形状 v2**（`_Vulkan/vkPrecipitation.cpp` + `precipitation.vert/frag`）：不再是实心长方形面片——
  顶点带 uv（横向 0..1、纵向 0=头/1=尾），片元按 `aStreakScale` 选形状：
  雨（>1.5）细芯 + 头部亮/尾部淡的雨丝，雪（<=1.5）径向软斑；雨丝取向改为**粒子速度方向**
  （有风会自然倾斜），静止粒子才回退屏幕竖直。顶点缓冲改为 4 槽（帧奇偶 × 帧内调用序号），
  同帧可分别提交雨和雪（各自的 streak），查看器不再需要合并粒子。
- **地面**（`tests/mmdViewer`）：模型世界 AABB 最低点铺一块 400 m 正方形，走既有 PBR 管线
  （不新增管线/着色器），先于模型绘制，作为贴花与降水的深度基准。
- **贴花系统**（新）：`src/graphics/GN_3Ddecal.h/.cpp` 纯 CPU 池——固定容量环形写（池满覆盖最老）、
  `aLife<=0` 为永久贴花、最后 30% 寿命线性淡出、`gatherDecalRenderItems` 转渲染条目；
  `_Vulkan/vkDecal.cpp` + `gn3d_decal.vert/frag`——世界 XZ 平面四边形，`SPLASH_RING` 画环、
  其它画软圆斑，alpha 混合 / 深度测试开（LESS_OR_EQUAL）/ 深度写关；`GN_VkDrawDecals` /
  `GN_VkDecalDestroy`（契约写入 `GN_Vk.h`，`gn3dShutdown` 接线）。
  查看器按雨强在角色周围地面生成水渍/环（220 条/s、单帧上限 64、寿命 0.5–1.1 s）。
- 测试：`testDecal` 7 用例（容量/参数校验、寿命回收、永久贴花、池满覆盖最老、末段渐隐、渲染条目转换）；
  `gpuCheck` 新增 4 条（`decal.outsidePass/empty/draw/pixels`，俯视相机 + 真实透视矩阵）。
- 验收（Lead 实跑）：`make unit` = **275 cases 0 failed**；`make gpuCheck` = **94/94 PASS**；
  lint 10 文件 0 违规 0 告警；`build/mmdViewer` 编译通过。
- 已知限制：贴花只贴水平地面（不做任意表面投影/法线朝向）；雪不生成地面水痕；地面是单色 quad，
  未做纹理/法线贴图；贴花池 512，超出覆盖最老。
- 修正（2026-10-29，用户目视反馈驱动）：
  ① **地面整片不可见**——地面四边形的绕序（0,2,1 / 0,3,2）与管线的背面剔除方向相反，被整块剔除，
  角色看起来浮在空中；已改为 0,1,2 / 0,2,3，并新增 `ground.placement` 回归检查（在 60° 透视相机下
  统计画面中列最亮行，断言落在下半屏，实测 63% → PASS）。
  ② 地面改走**前向 3D 管线**（同一条已验证路径），颜色提亮到 (0.42,0.43,0.40)。
  ③ 云层 `coverage×0.8`、`opacity=0.85`：暴风雨下三层云盘不再叠成一整块灰"天花板"（此前容易误看成地面翻到天上）。
  ④ 查看器新增**自由视角**（F 切换）：W/S/A/D 平移、Q/E 升降、Shift 加速、左键拖动转视角；轨道视角保持不变。
  ⑤ 查看器启动日志新增模型世界 AABB 与地面高度打印（`AABB y=[-0.081, 17.946]`，地面 y=-0.10）；
  地面高度取模型 AABB 最低点，实测与脚底一致。

## 2026-10-29 — P11-S2b/S4/S5/S6 集成：云层 / 粒子池扩展 / 降水 / 查看器全天气（90/90）

- **S2b 中景云层**：新增 `_Vulkan/vkCloud.cpp` 与 `gn3d_cloud.vert/frag`；`sCLOUD_DRAW_PARAMS`
  在 3D pass 内提交 `aTilesPerAxis² × 3 层` 面向相机的 billboard 四边形（tile 网格按相机 xz 取模对齐、
  按 `aWind × aTimeSeconds` 漂移、近处按 `aNearFade` 淡出），片元用 5 层高频 FBM 程序化 alpha
  （`aCoverage` 截断、`aSharpness` 控边、每层不同 coverage 偏置与明暗 ⇒ 云层分层不糊）；
  参数非法 / 3D pass 外调用返回 false。
- **S4 粒子池扩展**：`GN_3Dparticle.h/.cpp` 的 `sPARTICLE_EMITTER_DESC` 增加 BOX 形状
  （`aBoxHalfExtent`）、尺寸范围（`aSizeMin/aSizeMax`，独立随机流）、风场
  （`aWind/aWindResponse`，`v += (wind - v) * min(1, response*dt)`）与
  `setRecycleBox` 相机跟随循环回收；旧描述不设新字段时行为逐位不变（`particleLegacyDescUnchanged`）。
- **S5 降水管线**：新增 `_Vulkan/vkPrecipitation.cpp` 与 `precipitation.vert/frag`；CPU 把活粒子
  展开成世界空间 billboard（雨滴沿世界 -Y 按 `aStreakScale` 拉长，雪 = 1），alpha 混合、
  深度测试开 / 深度写关；`GN_VkDrawPrecipitation` 复用单块动态顶点缓冲。
- **S6 查看器集成**：`tests/mmdViewer/mmdViewer.cpp` 的 `applyWeather()` 增加雾
  （`GN_VkSetFog`：颜色 + density/heightFalloff/baseHeight，maxOpacity=0.9）；3D pass 内
  天空盒之后按 `weather.output()` 画云层（layerHeight 300 m、tileSize 256、8×8、nearFade 50）；
  创建雨/雪两套 `GN3d::cPARTICLE_SYSTEM`（BOX ±(16,10,16) 跟随相机，雨 1800/s、雪 900/s，
  风与尺寸随天气），每帧 `update(dt)` 后只在 3D pass 内提交一次 `GN_VkDrawPrecipitation`：
  雨雪强度同时 >0.05 时合并存活粒子（streak 取 6 折中），否则只提交强度更大的一类（雨 12、雪 1）。
  1..6 键语义不变，切换天气后云量/雾浓度/降水强度随过渡曲线变化。
- 验收（Lead 实跑）：`make gpuCheck` = **90/90 PASS**（AMD 780M / RADV，云与降水检查通过）；
  `tests/mmdViewer/mmdViewer.cpp` 带 `-DGN_USING_VULKAN_GLFW_LINUX` 的 `g++ -fsyntax-only` 通过；
  lint（写域）0 违规 0 告警。
- 已知限制：降水 v1 只有一块顶点缓冲，同一帧多次调用会互相覆盖 ⇒ 查看器每帧只提交一次；
  延迟光照路径的雾仍未做像素级验收（S3 遗留）；查看器全天气仅为桌面目视，未纳入自动化截图。
- 查看器后续修正（用户目视反馈驱动）：① 远平面下限提到 `CLOUD_LAYER_HEIGHT + 2*CLOUD_TILE_SIZE`
  （默认模型高度 18 m 时原 farZ≈232 m，300 m 云层被整片裁掉）；② 雨/雪初版粒径 0.05–0.10 m、
  alpha 0.55 在 15 m 外不足 2 px 且与天空对比过低（实测「看不见」）⇒ 雨加粗到 0.10–0.20 m / alpha 0.85，
  雪 0.16–0.32 m / alpha 0.95；③ 云层从单层改为 3 层云盘并把噪声频率提高约 3 倍（原单层低频 FBM 糊成一片）。
- 查看器降水体积修正（用户反馈「雨点不在角色身上」）：原回收盒以相机为心、半径 ±16 m，而查看器相机
  绕在角色 34 m 外 ⇒ 雨只存在于镜头附近、落不到角色上。现以**相机与角色中点为心**、xz 半径
  `max(26, 距离/2+8)`，生成盒固定 ±40×18×40，雨率 1800→3600/s、雪 900→1500/s（容量 6144/8192）。
- 新增两条 GPU 检查（88→90）：`precipitation.perspective`（真实 lookAt+透视矩阵下原点粒子落在画面中心）
  与 `precipitation.overGeometry`（先画不透明 PBR 四边形、再画更近的粒子 ⇒ 中心变红，证明半透明降水
  不会被深度测试剔掉）——两条都用于回答「到底有没有画、会不会被角色挡住」。

## 2026-10-29 — P11-S3 雾：独立 Fog UBO 接入三条 3D 片元管线（80/80）

- 新增 `_Vulkan/vkAtmosphere.cpp`：`sFOG_UBO`（color + density/heightFalloff/baseHeight/maxOpacity）
  的 host-visible UBO，`init3DAtmosphere` 在 `GN_VkInit3D` 最先创建，`GN_VkSetFog` 写入并钳制非法值；
  `density == 0`（默认）时雾量恒 0，既有画面不变。
- 描述符接线：前向 binding 3、PBR binding 6、延迟光照 binding 8（与 §3.6/§3.7 冻结槽位表一致）；
  `GN_VkShutdown3D` 增加 `destroyAtmosphere()`。
- 着色器：`gn3d.frag`（Blinn-Phong）、`gn3d_pbr.frag`、`gn3d_deferred_light.frag` 增加 FogUBO 与
  指数高度雾公式 `mix(fogColor, color, 1 - fogAmount)`，各自重新编译 `.spv`。
- 测试：`gpuCheck` 新增 3 条——`fog.pbr`（红色雾下中心像素转红）、`fog.pbrReset`（密度归零后
  恢复 ±0.03）、`fog.forward`（Blinn-Phong 路径同样受雾影响）。
- 验收：`make unit` = **262 cases 0 failed**；`make gpuCheck` = **80/80 PASS**（AMD 780M / RADV）。
- 边界：延迟路径的 binding 8 已接线并由描述符布局校验，但 G-Buffer 全链路的雾像素验收未做
  （v1 只验证前向与 PBR 两条路径）；雾颜色按线性空间混合，不做大气散射/太阳方向调制。

## 2026-10-29 — P10 动态 LOD：屏幕误差选级 + 分级绘制（77/77）

- 新增 `GN_3Dlod.h/.cpp`：`sLOD_VIEW`、`lodScreenError`（`S = err * H / (2*d*tan(fov/2))`，
  d = 相机到世界 AABB 最近距离，不可判定返回 +INF）、`selectLod`（由粗到细取最粗可接受级，
  变粗需 0.8× 阈值滞后）、`selectLodForModel`（按模型 AABB 批量选级）。纯 CPU、无 Vulkan 依赖。
- `GN_3Dmodel.h/.cpp`：新增 `sMESH_LOD`（顶点/索引/`aMaxError`）与 `sMESH::aLods/aLodCount/aBoundRadius`；
  `sMESH::DrawPBRLevel`（越界/无数据退回 LOD0）、`cMODEL::DrawPBRWithLod`；`DrawPBR` 改为
  `DrawPBRLevel(0)`（单级路径逐位一致）。
- `GN_Vk.h` 新增诊断钩子 `GN_VkLast3DIndexCount()`（`GN_VkDraw3DPBR` 记录最近一次 indexCount）。
- 测试：`testLod` 6 用例（误差公式、10/100/1000 m 选 0/1/2、滞后振荡 ≤2 次、相机盒内/非法视口、
  单级回归、模型批量选级）；`gpuCheck` 4 条（`lod.select` 0/1、`lod.indexCount` 36/6、
  `lod.singleLevel` 36、`lod.pixels` 两级中心像素一致）。
- 验收：`make unit` = **262 cases 0 failed**；`make gpuCheck` = **77/77 PASS**（AMD 780M / RADV）；
  lint 10 文件 0 违规 0 告警。
- 与草案偏差：选级显式接收世界 AABB；`aLods` 只存 CPU 数据（PBR 每次绘制动态上传，无需每级 VkBuffer）；
  验收距离改为按公式推导（原 2/20/200 m 与 1.0 px 阈值不自洽）。

## 2026-10-29 — P9 收尾：GPU 布料 / SSBO 蒙皮 / 绘制项批处理（73/73）

- **P9-a GPU 布料**：`GN_3DclothGPU.h/.cpp`（`sCLOTH_GPU_VERTEX` 32B、`sCLOTH_GPU_DESC`、
  `validateClothGpuDesc`、`stepClothCpu` 的 Verlet + Jacobi CPU 回退）；
  `_Vulkan/vkClothCompute.cpp`（A/B 双 host-visible SSBO 乒乓：积分 A→B、每轮约束翻转、
  结果不在 A 时 `vkCmdCopyBuffer` 回 A；单命令缓冲 + fence）；
  `_Vulkan/vkClothDraw.cpp`（顶点 pulling 网格 + Create 时生成索引缓冲）；
  `gn3d_cloth.comp/vert/frag`；`GN_VkClothCreate/Upload/Simulate/Data/Draw/Destroy`。
- **P9-b SSBO 蒙皮**：`_Vulkan/vkSkinSsbo.cpp` + `gn3d_skinned_ssbo.vert/frag`；
  `GN_VkDraw3DSkinnedSsbo`：调色板走 `mat4 palette[]` SSBO（4096 关节），3 个环形槽位，
  旧 128 关节 UBO 路径原样保留。
- **P9-c 批处理**：`GN_3Dbatch.h/.cpp`：`sortBatchItems`（pipelineKey→materialKey→depth→order
  稳定排序）+ `countBatchSwitches`；纯 CPU，不重构既有绘制路径。
- 接线：`GN_VkInit3D` 增加 `init3DSkinnedSsbo/init3DCloth`（失败只告警），
  `GN_VkShutdown3D` 增加 `destroySkinnedSsbo/destroyCloth`；描述符池 `STORAGE_BUFFER` 250→ 见 `GN_Vk.cpp`。
- 测试：`testClothGPU` 5 用例、`testBatch` 5 用例；`gpuCheck` 12 条新检查
  （`cloth.*` 5、`skin.ssbo.*` 4、`batch.*` 3）。
- 验收：`make unit` = **256 cases 0 failed**；`make gpuCheck` = **73/73 PASS**（AMD 780M / RADV，
  `cloth.compute` meanY 0 → -0.2153 且角点不动、`cloth.cpuMatch` 差 0、`cloth.draw` 中心
  `0.700,0.300,0.200`、`skin.ssbo.joint200` 像素 `0.572,0.254,0.127`、`batch.switches` 3 → 1）；
  新增文件 lint 0 违规 0 告警。
- 边界：布料只有结构距离约束（无剪切/弯曲/碰撞），每步 fence 等待；SSBO 蒙皮上限 4096 关节；
  批处理只排序不合并几何。CPU 回退（`stepParticlesCpu` / `stepClothCpu`）全部保留。

## 2026-10-29 — P9：GPU 粒子（compute 模拟 + SSBO + 实例化 billboard，61/61）

- 新增 `GN_3DparticleGPU.h/.cpp`：`sGPU_PARTICLE`（64B std430，四个 vec4）、`sPARTICLE_SIM_DESC`、
  144B `sPARTICLE_DRAW_PARAMS`（std140 镜像）、参数校验，以及 `stepParticlesCpu()` —— 与 compute
  同一公式的 CPU 回退与 GPU 对拍参考。
- 新增 `_Vulkan/vkParticleCompute.cpp`：host-visible SSBO 持久映射 + compute 管线经 `aPipelineCache`
  （`ePIPE_COMPUTE`）+ 独立命令缓冲/fence 同步；`GN_VkParticleCreate/Upload/Simulate/Data/Destroy`。
- 新增 `_Vulkan/vkParticleDraw.cpp`：顶点 pulling 的实例化 billboard 管线（无顶点/索引缓冲，
  `gl_InstanceIndex` 直读 SSBO；深度测试开/写关、双面、alpha 混合）；`GN_VkParticleDraw`。
- 新增着色器 `gn3d_particle.comp`（local_size 64）、`gn3d_particle.vert/.frag`。
- 描述符池补 `STORAGE_BUFFER`（256）；`GN_VkInit3D` 接 `init3DParticle/init3DParticleDraw`，
  `GN_VkShutdown3D` 先 `destroyParticle` 再 `destroyParticleDraw`。
- 新增 `tests/unit/testParticleGPU.cpp`（6 用例：布局、模拟/绘制参数校验、CPU 步进/寿命/守卫）与
  `tests/gpu/gpuCheckParticle.cpp`（7 条：create/compute/cpuMatch/death/draw/invalid/perf）。
- 验收：`make unit` = **246 cases 0 failed**；`make gpuCheck` = **61/61 PASS**（AMD 780M / RADV，
  `particle.compute` v.y=-1.000000 / p.y=-0.100000，`particle.cpuMatch` 差 0，`particle.draw`
  中心像素 0.200,0.800,0.300）。
- 边界：不自动重生（只清 `aAlive`）；每步 fence 等待、不跨帧流水化（65536 粒子 GPU 0.26 ms/步
  vs CPU 0.10 ms/步）；无半透明排序；GPU 布料 / SSBO 蒙皮调色板 / 批处理仍未做。

## 2026-10-29 — P1c 纹理：glTF PNG/JPEG 解码 + GPU 上传（54/54）

- 新增 `GN_3Dtexture.h/.cpp`：PNG（libpng）、JPEG（libjpeg，`GN_HAS_JPEG` 可选）内存解码，
  data URI 解 base64，按文件头识别格式；损坏输入走 setjmp 回收，不崩溃。
- 新增 `_Vulkan/vkTexture.cpp`：RGBA8 → `R8G8B8A8_SRGB` VkImage（staging + 布局转换）+
  glTF sampler 枚举映射（REPEAT/CLAMP/MIRROR + NEAREST/LINEAR）；`GN_VkCreateTextureRGBA8/DestroyTexture`。
- `sASSET_TEXTURE` 追加 `aImageBytes`：导入期保存 bufferView（.glb/内嵌）与相对 `.gltf` 的外部图片字节；
  `importTextures` 接收 base dir（`loadFromFile` 传入，`loadFromMemory` 为空）。
- `ImageLoadFromMem` 改为复用 `GN3d::decodePngMemory`，删除重复的 libpng 内存解码。
- 新增 `tests/unit/testTexture.cpp`（7 用例：PNG/JPEG/data URI/asset 来源/glTF data URI/sampler/损坏输入）
  与 `tests/gpu/gpuCheckTexture.cpp`（5 条：解码、glTF、上传、采样像素、销毁重建）。
- 验收：`make unit` = **240 cases 0 failed**；`make gpuCheck` = **54/54 PASS**（AMD 780M / RADV）。
- 边界：无 mipmap/各向异性；JPEG 依赖系统 `libjpeg`（缺失时仅 PNG）；`loadFromMemory` 不解析外部图片文件。

## 2026-10-29 — P1c GPU / P2 GPU 蒙皮 / P8 渲染接入（真实 GPU 49/49）

- **P1c GPU**：`_Vulkan/vkMaterial.cpp` + `gn3d_material.vert/.frag`；材质实例固定 set0
  （binding0 UBO / binding1 组合采样器）描述符集创建/写入/归还，材质管线经 `aPipelineCache`；
  `GN_VkMaterialCreate/Destroy/Draw3DMaterial`；描述符池补 `FREE_DESCRIPTOR_SET_BIT`（销毁可真正归还）。
- **P2 GPU 蒙皮**：`_Vulkan/vkSkin.cpp`（3 槽 8KB 调色板环形 UBO）+ `_Vulkan/vkSkinDraw.cpp`
  （`sSKINNED_VERTEX` 68B 六属性、set0 = 场景 UBO + 调色板 UBO、`GN_VkDraw3DSkinned`）+
  `gn3d_skinned.vert/.frag`；LBS 在模型空间完成后乘 model/viewProj。
- **P8 渲染接入**：`GN3d::cullDrawModelsPBR`（可见列表 → `DrawPBR`，返回绘制数）+
  `_Vulkan/vkDebugDraw.cpp`（LINE_LIST、深度测试开/写关、`GN_VkDrawDebugLines`）+ `gn3d_debug.vert/.frag`。
- **验收**：`make gpuCheck` = **49/49 PASS**（AMD Radeon 780M / RADV，新增 18 条）；`make unit` =
  **233 cases 0 failed**；新增文件 lint 0 违规。
- 只记录、未修（不在本轮写域）：`CreateCubeModel` 顶点数 36 但只写 24（世界 AABB 读脏）；
  `Ensure3DBufferCapacity` 同 pass 扩容会销毁在飞缓冲；`GN_VkInit3D` 在 `initialized=true` 之前调用 init。
  详见 `tests/gpu/README.md` 与 `docs/3D_FEATURE_PLAN.md` §3.5。
- P9（GPU 粒子/布料/SSBO）为可选阶段，本轮未启动；任务已排队。

## 2026-10-29 — P1b GPU 桌面验收通过（新的 tests/gpu 体系）

- 新增 `tests/gpu/` + `make gpuCheck`：11 条管线创建/缓存计数、2D 四混合、3D 前向/PBR/阴影/G-Buffer+延迟/合成/天空盒、缓存释放，共 31 条检查。
- 真实 GPU（AMD Radeon 780M / RADV PHOENIX，RGBA16F swapchain）**31/31 PASS**；lavapipe 软件预验同样 31/31。
- 回读路径：swapchain / shadow / G-Buffer 图像补 `VK_IMAGE_USAGE_TRANSFER_SRC_BIT`；验收程序按实际 swapchain 格式自建回读（不依赖 `GN_VkReadBack` 的固定 4 字节/像素）。
- 仅记录、未修（不属 P1b）：`GN_VkReadBack` 固定 4 字节/像素；`GN_VkDraw3D_GBuffer` 录制期更新已绑定描述符集；2D 批处理整帧单管线；PBR 负高度视口 vs API 1.0、HDR resolve 格式、swapchain `minImageCount` 等 Validation 项详见 `tests/gpu/README.md`。

## 2026-10-27 — 移除旧示例测试与旧资源（测试体系待重新设计）

- 删除 `samples/`（16 个示例测试，其中 05/06/07/09/11~16 长期为 TODO 占位页）与 `run.sh`；
  旧测试资源 `assets/ui`、`assets/axis`、`assets/fileicon`、`assets/effects`（62 文件）一并删除。
- `Makefile`：移除 `test` / `tests` 目标与示例编译段；`make check` = 构建 + 单元测试（`make unit` 保留，134 用例）。
- 保留：`assets/shaders/`（渲染必需）、`external/`（编译必需）、`docs/`、`tests/unit/`（本工程验收用例）。
- 后续测试代码（含 GPU 验收）按新体系重新设计，不再恢复 `run.sh`。

## 2026-10-27 — 恢复引擎默认资源（修复资源加载全失败）

- 根因：`701828d 清理原始例子` 把 `assets/` 下除 `shaders/` 外的资源整批删除，而引擎与测试仍按 `GN_SysDir()`（根目录 = `assets/`）加载：
  `samples/02/03` 用 `ui/bubbleB`，`GN_RunRec` 用 `ui/br`，`GN_UiBase` 用 `ui/bubble*`、`axis/2Dctrl_*`，`GN_UiFBrowser` 用 `fileicon/*`，`GN_Frame` 用光环贴图。
- 修复：从 `701828d^` 恢复 `assets/ui`（21）、`assets/axis`（4）、`assets/fileicon`（36）、`assets/effects`（1），共 62 个文件 / 约 0.9 MB。
- 修复 `GN_Frame.cpp` 默认光环路径 `eft/` → `effects/`（仓库里从未有过 `assets/eft/`，该默认值一直是坏的）。
- 说明：`samples/05/06/07/09/11..16` 仍是 TODO 占位页（不是资源加载失败）；资源类测试为 `./run.sh 02`、`03`。

## 2026-10-26 — 工程化阶段 0（止血）

- 修复 `samples/08_cube3d.cpp` 的 `Height()` 重复声明 `float h`（该文件此前无法编译）。
- Makefile：`make tests` 失败时非 0 退出并保留错误输出；`test/tests` 使用 `$(CXX)`；`clean` 清理 `build/test_*`；移除 freeglut 头依赖；`help` 与实际目标一致。
- `GN_VERSION` 更新为 `2026.07.04`；架构文档的版本改为引用 `GN.h`（单一来源）。
- 新增根 `LICENSE`（MIT）与 `external/README.md`（第三方来源/许可清单）。
- `GN_VkInit` 在无 Vulkan 物理设备 / 无可用队列族时输出错误并返回，不再空指针崩溃。
- 新增 `tests/unit/` 最小测试框架与 67 个纯逻辑用例；`make unit` / `make check` 可用。
- 新增 `make asan`：独立构建目录 + 独立库名，ASan/UBSan 跑同一套单测（67/67 无报告）。
- 修复 `GN_StrUnSplitToData` 的长度累加错误（原为赋值，导致缓冲区溢出）。
- 修复 `GN_StrCutSpaceFL` 对空串/全空白串的 `strlen-1` 越界读取。
- 修复 `GN_ValueUnPackHalf(0)` 返回 3e-5 而非 0。
- 修复 `GN_ValueFloatToHalf` 在 `e>125` 时的无符号移位回绕 UB（UBSan 实测命中）。
- 修复 `GN_ValueHalfToFloat` 在 `e!=0` 时的 150 位移 UB（UBSan 实测命中）。
- 新增程序化图像生成（矩形渐变、棋盘格）单测。
- 阶段 2 第一步：把 `VkCheck`/`FindMemType`/`CreateBuf`/`MakeImageView`/`LoadSPIRVFile` 抽到 `_Vulkan/vkDevice.cpp` + `vkInternal.h`（内部符号隐藏，公共动态符号不变）；`GN_Vk.cpp` 3690 → 3627 行。
- 阶段 2 第二步：把 PNG 加载/图集打包/保存/帧缓冲回读抽到 `_Vulkan/vkImageIo.cpp`（281 行）；`GN_Vk.cpp` 3627 → 3366 行；清理不再使用的 `<png.h>/<fstream>/<vector>` 包含。
- 阶段 2 第三步：把 `sIMAGE` 构造/析构/赋值/`Release`/`CopyOut`/`DataGet*` 抽到 `_Vulkan/vkImage.cpp`（187 行）；`GN_Vk.cpp` 3366 → 3204 行。
- 阶段 2 第四步：把 `ImageDraw`/`ImageDraw4P`/`ImageDrawRot`/`LineFrame2dDraw` 抽到 `_Vulkan/vkDraw2d.cpp`（177 行）；共享顶点批状态（`gVBuf`/`gVMax`/`gVCnt`/`s_lastDS`）与批处理辅助改为 `vkInternal.h` 内部符号（hidden）；`GN_Vk.cpp` 3204 → 3035 行。
- 阶段 2 第五步：把剩余 2D 包装（`ImageDrawDitherBlur`/`ImageDrawMP`/`ImageDrawFull`/`TextDraw`/`Draw2DLine`/AlphaKey/AlphaDirection）并入 `vkDraw2d.cpp`（232 行）；`GN_Vk.cpp` 3035 → 2984 行。
- 阶段 2 第六步：把 `GN_VkBeginFrame`/`GN_VkEndFrame`/`GN_VkSetScissor`、scissor 与顶点批状态、`BatchSwitch`/`FlushBatchIfScissorDirty`/`FlushVertexBatch`/`GrowVBufIfNeeded`、延迟上传队列抽到 `_Vulkan/vkFrame2d.cpp`（184 行）；相关状态以 hidden extern 暴露给 3D 代码；`GN_Vk.cpp` 2984 → 2819 行。
- 阶段 2 第七步：把 `GN_VkUpload3DMesh`/`GN_VkRelease3DMesh` 抽到 `_Vulkan/vk3dMesh.cpp`（80 行）；`GN_Vk.cpp` 2819 → 2751 行。
- 阶段 2 第八步：把阴影 pass（`GN_VkShadowPassBegin/End`、`GN_VkShadowDraw`）、`GN_VkCompositeToSwapchain`、`GN_VkDrawSkybox` 抽到 `_Vulkan/vk3dPasses.cpp`（170 行）；`s_in3DPass`/`s_inShadowPass`/`s_framebuffers3D`/`s_vtxOfs3D`/`s_idxOfs3D` 与 `Ensure3DBufferCapacity` 提升为内部符号；`GN_Vk.cpp` 2751 → 2597 行。
- 阶段 2 第九步：把 PBR pass（含 HDR 变体）与延迟渲染（G-Buffer / DeferredLighting）并入 `vk3dPasses.cpp`（403 行）；`GN_Vk.cpp` 2597 → 2370 行。
- 阶段 2 第十步：把前向 3D pass（`GN_VkBegin3DPass`/`GN_VkEnd3DPass`/`GN_VkDraw3D`/`GN_VkDraw3DPBR`）抽到 `_Vulkan/vk3dForward.cpp`（273 行）；`GN_Vk.cpp` 2370 → 2111 行。
- 阶段 2 第十一步：从 `GN_VkInit3D` 抽出渲染目标初始化（MSAA 颜色/深度、3D render pass、framebuffer）到 `_Vulkan/vk3dInit.cpp`（`init3DTargets`，107 行）；`GN_Vk.cpp` 2111 → 2018 行。
- 阶段 2 第十二步：从 `GN_VkInit3D` 抽出前向管线初始化（着色器模块、描述符/管线布局、图形管线、场景+光照 UBO、默认描述符集）到 `_Vulkan/vk3dInit.cpp`（`init3DForward`，shader 二进制由调用方传入）；`GN_Vk.cpp` 2018 → 1905 行。
- 阶段 2 第十三步：从 `GN_VkInit3D` 抽出 PBR 管线初始化（6 binding 描述符布局、管线布局、图形管线）到 `_Vulkan/vk3dInit.cpp`（`init3DPbr`）；`GN_Vk.cpp` 1905 → 1824 行。
- 阶段 2 第十四步：从 `GN_VkInit3D` 抽出阴影映射初始化（深度图/视图/采样器、render pass、framebuffer、描述符集、管线）到 `_Vulkan/vk3dInit.cpp`（`init3DShadow`）；`GN_Vk.cpp` 1824 → 1712 行。
- 阶段 2 第十五步：从 `GN_VkInit3D` 抽出 HDR 离屏目标与合成管线（HDR 图像/视图/采样器、HDR render pass/framebuffer、合成 render pass/管线/描述符集）到 `_Vulkan/vk3dInit.cpp`（`init3DHdr`）；`GN_Vk.cpp` 1712 → 1574 行。
- 阶段 2 第十六步：从 `GN_VkInit3D` 抽出天空盒初始化（程序化 Cubemap 6 面、IBL 描述符绑定、天空盒管线）到 `_Vulkan/vk3dInit.cpp`（`init3DSkybox`）；`GN_Vk.cpp` 1574 → 1370 行。
- 修复：天空盒此前只有初始化、**没有任何调用点**（sample 10 注释写"不需要 skybox"）；在 `samples/10_scene3d.cpp` 的 3D pass 内加入 `GN_VkDrawSkybox(&vp)`。
- 阶段 2 第十七步：从 `GN_VkInit3D` 抽出延迟渲染初始化到 `_Vulkan/vk3dInit.cpp`：`init3DGbuffer`（3 颜色附件 + 深度、render pass/framebuffer、G-Buffer 管线）与 `init3DDeferredLight`（8 binding 描述符、交换链 render pass/framebuffer、全屏三角形管线）；`GN_Vk.cpp` 1370 → 1178 行，`GN_VkInit3D` 仅剩 86 行。
- 阶段 2 第十八步：把 `GN_VkShutdown3D`、`Ensure3DBufferCapacity`、3D 缓冲偏移/容量状态抽到 `_Vulkan/vk3dShutdown.cpp`（135 行）；`GN_Vk.cpp` 1178 → 1054 行（有效代码 <1000，机检通过）。生成的 `gn3d_pbr_frag_spv.h` 按规则登记 `code-modularity: no-split` 技术债。
- 格式化：新增 `.clang-format`（4 空格 / K&R / 不重排长行），格式化 `src/` 下 100 个手写源文件（9 个生成的 `*_spv.h` 排除）。clang-format 会展开单行 `{...}` 块，格式化后 `GN_Vk.cpp`（1226 有效行）与 `vk3dInit.cpp`（1515 有效行）超限，因此拆分：`sIMAGE` 数据方法 → `vkImageData.cpp`（553 行）；HDR/天空盒/G-Buffer/延迟光照初始化 → `vk3dInitScene.cpp`（1022 行）。`GN_2Dscene.cpp` 与 `GN_String.cpp` 因格式化越过 1000 行，已还原并重放两处字符串修复。剩余 12 个 >1000 行文件在 HEAD 中已超限，登记为存量技术债。
- Headless 冒烟未实现：当前机器无 Vulkan 设备（`/dev/dri` 缺失、未装 lavapipe），留待环境可用后补。

## 2026-07-04 — Vulkan 稳定性修复

### 核心修复

- **延迟纹理上传机制**: 渲染 pass 内 `DataFill`/`UploadImageToGPU` 不再 `vkQueueSubmit`，数据暂存到 `s_pendingUploads[256]`，由 `GN_VkBeginFrame` 在 pass 前处理。彻底解决 AMD RADV GPUVM fault。

- **PreFrame 回调**: 新增 `GT_pFuncPreFrame`，在 `GF_FrameCycleProc1` (渲染 pass 前) 调用，适用于需在 pass 外完成的 GPU 初始化。

- **UploadImageToGPU 重构**: 移除独立的 GPU 上传逻辑，委托给 `DataFill` 统一走延迟上传路径。

### 光照系统

- 延迟光照着色器: Blinn-Phong → Cook-Torrance BRDF (GGX + Smith + Fresnel)
- `AddDirectional`/`AddSpot` 新增 `enableShadow` 参数
- `cLIGHT::Clear()` 恢复 `shadowMapSize`
- `GN_VkBegin3DPass` 等 4 个函数从 `lightUBO` 读取环境光
- PBR shader IBL 镜面反射: `F0` → `FresnelSchlick(NdotV, F0)`

### 视频播放

- FFmpeg 8.x matroska: `AVFMT_FLAG_IGNIDX | AVFMT_FLAG_IGNDTS`
- `zReadPacket` EOF 循环恢复 + `Play()` IO 重置
- `GN_SetWindowPos` 实现 (glfwSetWindowSize + GN_VkResize)
- MKV 文件重封装修复 H264 解码警告

### UB 修复

- 严格别名: `GN_Base.h` (packHalf/unpackHalf), `GN_UiBase.cpp`
- `memcpy(this)` → placement new: `GN_Frame.cpp`, `GN_Array.h`

### 安全修复

- 98 处 `strcpy`/`strcat` → `strncpy` + 显式 null (12 文件)
- 25 处 Vulkan `VkCheck` 错误检查

### Vulkan 资源管理

- 默认纹理 (1×1 white) Image/ImageView/Memory 在 shutdown 正确销毁
- 顶点缓冲预分配 512K 顶点 (16MB)
- `vkQueueWaitIdle` → fence 同步 (3 处)

### 示例修复

- Sample4/5: OpenGL 纹理合并代码在 Vulkan 下跳过
- `ImgFmtGetByteNum(0)` 返回 4 (CR_DEFAULT_FORMAT)
- Sample12: 视频缩放适配窗口 + audio=false

### 代码清理

- samples/ 精简为仅 .cpp 源码 (259 文件删除, 80MB→380KB)
- assets/ 精简为仅 shaders/ 目录
- 调试 fprintf 清理
- .gitignore 添加 build/ 产物

### 构建

- Makefile: 合并重复 .o 规则 + DEPFLAGS 自动依赖
