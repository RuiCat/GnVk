# 3D 能力扩展规划：空间划分 / 物理 / 粒子 / 动画 / 骨骼 / 布料 / 通用管线

> 状态：P0 契约冻结 + P1a（管线/着色器缓存库）+ P1b（**11 条既有管线全部接入缓存**）+ P4（空间划分）+
> P5 v1（球/盒刚体物理）+ P5b 全部六项（胶囊、warm starting、CCD v1、DebugDraw、关节/约束、静态三角网）已落地；
> P6 布料 XPBD、P7 3D 粒子、P3 动画、P2 骨骼（世界矩阵/调色板）、P4→P8 的 CPU 侧（cMODEL 世界 AABB + 视锥剔除）
> 与 P1c 材质实例 CPU 侧（描述/校验/去重键）已落地；
> P2 glTF 导入（需 `cgltf` 第三方头）、P1c 的 GPU 描述符集创建与 P8 渲染接入/P9 未开始。
> 基线：C++11、Vulkan 1.4、Makefile 单一构建入口；现有 3D 为静态网格 + Blinn-Phong/PBR + 阴影/延迟/天空盒。
> 约束：不破坏现有 `GN_VkDraw3D`/`GN_VkDrawPBR`/`cMODEL` API；新文件 camelCase，单功能 <500 有效行、多功能 <1000 有效行。
> 2D 侧现有的 `GN_2DArea` 碰撞与 `cFRAME_LAUNCHER` 动画图筐发射器**保持不变**，新系统只服务 3D。
>
> **本轮验收证据**（2026-10-27，`make -j8 check`）：单元测试 **200 cases, 0 failed, 0 assertions failed**
> （基线 67 + P1a/P1b 22 + P4 11 + P5 70 + P6 6 + P7 6 + P3 6 + P2 6 + P8-CPU 3 + P1c-CPU 3）；
> `lintCodeModularity src/graphics tests/unit --naming --comments`
> 仅剩既有 `GN_2Dmath.cpp` 超限与既有 `gn3d_pbr_frag_spv.h` 告警，新增/改动文件 0 违规 0 告警。
> 物理文件有效行：`GN_Physics.cpp` 498（只剩 2 行余量，再加逻辑必须迁出）、`GN_PhysicsSolver.cpp` 265、
> `GN_PhysicsJoint.cpp` 380、`GN_PhysicsMesh.cpp` 212、`GN_PhysicsCcd.cpp` 112、`GN_PhysicsDebug.cpp` 108、
> `GN_PhysicsBody.cpp` 196、`GN_PhysicsCollision.cpp` 376、`GN_PhysicsCollisionCapsule.cpp` 319；
> 布料 `GN_3Dcloth.cpp` 234、粒子 `GN_3Dparticle.cpp` 170、动画 `GN_3Danim.cpp` 190、骨骼 `GN_3Dskeleton.cpp` 43、
> 剔除 `GN_3Dcull.cpp` 61、材质 `GN_Material.cpp` 82（均 <500）。
> **重要边界**：本机无 GPU（`/dev/dri` 不存在，`vulkaninfo` 报 no valid GPUs）⇒ Vulkan 创建路径
> （`cSHADER_CACHE` / `cPIPELINE_CACHE` 的 `vkCreate*` 调用）**只做了编译验证，未做运行时验证**；
> P1b 已接入的 11 条管线必须在有 GPU 的桌面用**重新设计的测试体系**对照后才能算验收通过（原 `samples/` + `run.sh` 已移除，见 CHANGELOG 2026-10-27）。

## 1. 现状与差距

| 能力 | 现状 | 差距 |
|---|---|---|
| 空间划分 | 无 3D 空间索引；3D 逐模型线性绘制 | 无 BVH/AABB 树、无视锥剔除、无 raycast |
| 物理 | 无物理引擎；只有 2D 相交测试 | 无刚体、积分、接触求解、3D 碰撞 |
| 粒子 | 仅 2D `cFRAME_LAUNCHER`（`GN_Frame.h:238`） | 无 3D 粒子发射器/实例化渲染 |
| 动画 | 仅 2D `cFRAME`/`sAM_CTRL`（`GN_Frame.h:64`、`GN_AmCtrl.h:51`） | 无 3D clip/混合/状态机/骨骼 |
| 骨骼 | 无 | 无关节层级、蒙皮、glTF |
| 布料 | 无 | 无 XPBD/动态网格 |
| 管线 | 每种 pass 手写 `VkPipeline`（`GN_Vk.h:197-278`） | 无统一描述/缓存/材质变体 |

## 2. 目标架构

```
src/graphics/
  GN_Shader.h/.cpp           SPIR-V 模块加载/缓存/变体
  GN_Pipeline.h/.cpp         管线描述、布局、缓存（图形/计算）
  GN_Material.h/.cpp         材质实例：管线 + 描述符集 + UBO/纹理
  GN_Spatial.h/.cpp          AABB、BVH、动态 AABB 树、视锥、raycast
  GN_Physics.h/.cpp          物理世界/刚体/碰撞体/查询
  GN_PhysicsCollision.cpp    形状相交与接触流形
  GN_PhysicsSolver.cpp       顺序冲量求解、积分、休眠
  GN_3Dasset.h/.cpp          glTF 2.0（节点/网格/皮肤/动画）
  GN_3Dskeleton.h/.cpp       关节层级/绑定姿势/逆绑定/调色板
  GN_3Danim.h/.cpp           clip、采样、混合、状态机、事件
  GN_3Dskin.h/.cpp           蒙皮网格资源与绘制
  GN_3Dcloth.h/.cpp          XPBD 布料求解器
  GN_3Dparticle.h/.cpp       3D 粒子发射器/粒子池/实例数据
  _Vulkan/vkPipeline.cpp     管线缓存实现
  _Vulkan/vkSkin.cpp         蒙皮描述符/关节调色板上传/绘制
  _Vulkan/vkCloth.cpp        布料动态缓冲上传/绘制；后续 compute
  _Vulkan/vkParticle.cpp     粒子实例缓冲/绘制
  _Vulkan/vkDebugDraw.cpp    物理/空间划分线框调试
（测试程序待重新设计；原 samples/ 已移除）
  计划中的示例：skinned / cloth / physics / particles
tests/unit/
  testSkeleton.cpp / testAnimation.cpp / testCloth.cpp
  testSpatial.cpp / testPhysics.cpp / testParticle.cpp / testPipeline.cpp
assets/models/*.glb
assets/shaders/
  gn3d_skinned.vert/frag
  gn3d_cloth.vert/frag
  gn3d_particle.vert/frag
  gn3d_debug.vert/frag
```

### 2.1 通用着色器管线（所有新系统的地基）

不引入 SPIRV-Cross；先用**显式管线描述**：

```cpp
enum ePIPELINE_KIND { ePIPE_GRAPHICS, ePIPE_COMPUTE };

struct sVERTEX_ATTR { uint32_t location, binding, offset; VkFormat format; };
struct sVERTEX_LAYOUT { sVERTEX_ATTR attrs[8]; uint32_t count, stride; };

struct sPIPELINE_DESC {
    ePIPELINE_KIND kind;
    const char *vsPath, *fsPath, *csPath;
    sVERTEX_LAYOUT vertexLayout;
    VkRenderPass renderPass; uint32_t subpass;
    VkPipelineLayout layout;
    bool depthTest, depthWrite, blend, cullBack;
    VkPrimitiveTopology topology;
};

class cPIPELINE_CACHE {
public:
    VkPipeline GetOrCreate(const sPIPELINE_DESC &desc); // 哈希命中复用
    void Release();
};
```

- 哈希键 = 描述字段 + SPIR-V 内容哈希；同 pass 只创建一次。
- `sMATERIAL` = 管线句柄 + 描述符集 + UBO/纹理；蒙皮/布料/粒子/调试线框全部走它。
- 阶段 1 先把现有 PBR/前向管线包进缓存，旧 `GN_Vk.pipeline*` 保留到迁移完成。

**P1a 已落地（2026-10-27）**：`GN_Shader.h/.cpp`（SPIR-V 读取/内容哈希/魔数 + `cSHADER_CACHE` 模块去重）、
`GN_Pipeline.h/.cpp`（描述/顶点布局/`ValidatePipelineDesc`/`ComputePipelineKey`/`PipelineDescEqual`）、
`_Vulkan/vkShader.cpp`、`_Vulkan/vkPipeline.cpp`（图形/计算创建 + `cPIPELINE_CACHE`）、`tests/unit/testPipeline.cpp`（13 用例）。
相对草案的描述字段做了必要扩充：`aSamples`（MSAA）、`aDepthCompare`、`aFrontFace`、`aColorAttachmentCount`、
`ePIPELINE_BLEND`（既有 2D 的 additive/multiply 变体需要）、`aDynamicViewportScissor` —— 草案只写
`depthTest/depthWrite/blend` 布尔，无法表达既有管线的混合/采样/附件差异。
**尚未做**：P1b（把既有 `pipeline3D`/`pipelinePBR`/阴影/延迟/天空盒/2D 创建点改为走缓存）需要 GPU 桌面验证；
P1c（GPU 材质实例 `sMATERIAL_RES`）未开始；现有 `GN3d::sMATERIAL` 是不含 GPU 句柄的 CPU 材质，两者不冲突。

### 2.2 空间划分

```cpp
struct sAABB { GNg::sVEC3 min, max; };

class cBVH {                       // 静态几何：地形/建筑/合并网格
public:
    void Build(const sAABB *boxes, uint32_t count);
    void QueryFrustum(const sFRUSTUM &f, std::vector<uint32_t> &out) const;
    bool Raycast(const sVEC3 &origin, const sVEC3 &dir, float maxT, sRAY_HIT &hit) const;
};

class cDYNAMIC_AABB_TREE {         // 动态刚体/粒子发射器 broadphase
public:
    uint32_t Insert(const sAABB &box, void *user);
    void Update(uint32_t handle, const sAABB &box);
    void Remove(uint32_t handle);
    void Query(const sAABB &box, std::vector<void*> &out) const;
};

struct sFRUSTUM { GNg::sVEC4 planes[6]; }; // 从 viewProj 提取
```

- 渲染：`cMODEL::Draw` 前先做视锥剔除，可见列表交给渲染器；静态大网格用 BVH，动态对象用 AABB 树。
- 物理：动态 AABB 树做 broadphase；BVH 做静态场景查询/raycast。
- 拾取/编辑器：统一 `Raycast` 接口；2D 的 `GN_2DArea` 不动。
- 不引入 octree；AABB 树对动态物体更友好，BVH 对静态场景更高效。

### 2.3 物理模拟

**后端决策：自研最小刚体物理（推荐）**，接口允许后续换 Bullet/Jolt；不直接引 Jolt（要求 C++17，与工程 C++11 冲突）。

```cpp
enum eBODY_TYPE { eBODY_STATIC, eBODY_DYNAMIC, eBODY_KINEMATIC };
enum eSHAPE_TYPE { eSHAPE_SPHERE, eSHAPE_BOX, eSHAPE_CAPSULE };

struct sRIGID_BODY {
    eBODY_TYPE type; eSHAPE_TYPE shape;
    float mass, invMass;
    GNg::sVEC3 pos; GNg::sQUAT rot;
    GNg::sVEC3 linVel, angVel;
    GNg::sMAT44 invInertiaLocal, invInertiaWorld;
    float friction, restitution;
    bool sleeping;
    sAABB worldAABB;
};

class cPHYSICS_WORLD {
public:
    void SetGravity(const GNg::sVEC3 &g);
    uint32_t AddBody(const sRIGID_BODY &body);
    void AddStaticMesh(const sMESH &mesh);      // 静态三角网格/BVH
    void Step(float dt);                        // 固定 1/60，内部子步
    bool Raycast(const GNg::sVEC3 &o, const GNg::sVEC3 &d, float maxT, sRAY_HIT &hit);
    void DebugDraw(cDEBUG_DRAW &draw) const;
};
```

- 形状 v1：球/盒/胶囊；凸包 v2。
- 碰撞：球-球、球-盒、盒-盒（SAT）、胶囊-盒/球；接触流形 + 顺序冲量（warm start、摩擦、恢复系数）。
- 积分：半隐式 Euler；固定步长 + 累加器；休眠/唤醒。
- 约束 v2：距离、铰链、固定；关节驱动可选。
- 碰撞查询与空间划分共用 AABB 树；静态网格用 BVH 三角查询。
- 现有 2D 碰撞不并入 3D 物理。

### 2.4 布料（物理子模块，XPBD）

```cpp
struct sCLOTH_DESC {
    int nx, ny; float spacing, mass;
    float compliance; int substeps;
    GNg::sVEC3 gravity, wind;
    bool pinCorners;
};

class cCLOTH {
public:
    bool Create(const sCLOTH_DESC &d);
    void Step(float dt);
    void Upload();
    void Draw(const sMATERIAL &m);
    void AddCollider(const sSPHERE3D &s); // 接刚体/骨骼碰撞体
};
```

- 距离约束：结构/剪切/弯曲 + 固定点；XPBD 子步求解，位置修正后更新速度。
- 碰撞：球/胶囊；阶段 6 接 `cPHYSICS_WORLD` 的刚体与骨骼碰撞体。
- 渲染：双面、无背面剔除；动态顶点/法线缓冲；GPU compute 放 P9。

### 2.5 粒子模拟

```cpp
struct sPARTICLE {
    GNg::sVEC3 pos, vel;
    float age, life, size;
    GNg::sVEC4 color;
};

struct sEMITTER_DESC {
    float rate; int maxParticles;
    float lifeMin, lifeMax;
    GNg::sVEC3 dir, spread; float speedMin, speedMax;
    GNg::sVEC3 gravity; float drag;
    bool additive; const sIMAGE *texture;
};

class cPARTICLE_SYSTEM {
public:
    void Create(const sEMITTER_DESC &d);
    void Update(float dt);       // CPU 池 + 环形复用
    void Upload();               // 实例缓冲
    void Draw(const sMATERIAL &m);
};
```

- 渲染：billboard 实例化，深度测试开、写关；加法/Alpha 混合；按深度排序可选。
- 阶段 7 CPU 粒子（~10k）；P9 可换 compute + SSBO。
- 2D `cFRAME_LAUNCHER` 继续用于 UI/2D 特效，不迁移。

### 2.6 骨骼与蒙皮

```cpp
struct sJOINT {
    int32_t parent;
    GNg::sMAT44 inverseBind;
    GNg::sVEC3 bindT; GNg::sQUAT bindR; GNg::sVEC3 bindS;
};

struct sSKELETON {
    std::vector<sJOINT> joints;
    void ComputePalette(const std::vector<GNg::sMAT44> &world,
                        GNg::sMAT44 *out) const; // world * inverseBind
};

struct sSKIN { uint16_t joint[4]; float weight[4]; };
```

- 关节调色板走 UBO（128 关节 × 64B = 8KB），兼容现有仅 UBO 的描述符；超出分批 draw。
- 新顶点 `VkVertex3DSkinned`（11 float + 4×uint16 + 4×float）；旧 `VkVertex3D` 不变。
- glTF：关节=节点，`inverseBindMatrices` 直接读；按 primitive 拆 `sMESH`。

### 2.7 动画系统

```cpp
struct sANIM_CHANNEL {
    uint32_t node; enum ePATH { T, R, S } path;
    std::vector<float> times;
    std::vector<GNg::sVEC3> vecValues;
    std::vector<GNg::sQUAT> quatValues;
};

struct sANIM_CLIP {
    std::vector<sANIM_CHANNEL> channels;
    float duration;
    void Sample(float t, /* out T/R/S */ ...) const;
};

struct sANIM_STATE {
    const sANIM_CLIP *clip; float time, speed; bool loop;
    const sANIM_CLIP *next; float blend; // CrossFade
};
```

- 插值：glTF `STEP/LINEAR/CUBICSPLINE`；旋转 slerp。
- 混合：节点级 T/R/S；后续可加 1D/2D blend space。
- 状态机：`Play/Stop/CrossFade` + 事件（脚步/攻击判定）。
- 2D `sAM_CTRL` 保留；3D 动画独立，不强行统一两套时间轴。

## 3. 阶段与验收

| 阶段 | 内容 | 产物 | 验收 |
|---|---|---|---|
| P0（2d） | 契约冻结：结构体/API/文件；测试骨架 | 本规划 + 头文件草案 | 评审通过；旧 API 不变 |
| P1（5d） | 通用管线/材质/SPIR-V 缓存 | `GN_Shader`/`GN_Pipeline`/`GN_Material`、`vkPipeline.cpp` | `make check`；PBR/前向改走缓存；`testPipeline` |
| P2（10d） | glTF + 骨骼 + 蒙皮 | `GN_3Dasset`/`GN_3Dskeleton`/`GN_3Dskin`、`11_skinned` | 静态/带骨骼 glTF 可见；palette 单测；sample 11 |
| P3（7d） | 动画 clip/采样/混合/状态机 | `GN_3Danim`、蒙皮动画 | 循环/CrossFade；`testAnimation` |
| P4（5d） | 空间划分：BVH/AABB 树/视锥/raycast | `GN_Spatial`、`testSpatial` | 剔除结果与暴力测试一致；raycast 命中 |
| P5（15d） | 刚体物理：形状/接触/冲量/休眠 | `GN_Physics*`、`13_physics` | 下落/堆叠/碰撞稳定；`testPhysics` |
| P6（8d） | 布料 XPBD + 刚体/骨骼碰撞 | `GN_3Dcloth`、`12_cloth` | 下落/pinned/碰撞稳定；`testCloth` |
| P7（6d） | 3D 粒子：发射器/实例化/混合 | `GN_3Dparticle`、`14_particles` | 10k 粒子可交互；`testParticle` |
| P8（7d） | 集成：视锥剔除接入渲染、调试绘制、示例/文档/ASan | 可见列表、`vkDebugDraw` | `make check/asan`；4 个示例桌面运行 |
| P9（7d，可选） | GPU 蒙皮/GPU 布料/GPU 粒子、SSBO、批处理 | compute 路径 | 性能对比；CPU 回退可用 |

依赖：P1 → P2/P4/P5/P7；P4 ↔ P5（broadphase）；P5 → P6；P2 → P3；P8 在 P2–P7 后。总约 **72 人日**（约 15 周单人），不含资产制作，建议预留 20% 缓冲。

### 3.1 进度（2026-10-27）

| 阶段 | 状态 | 产出 / 证据 |
|---|---|---|
| P0 | ✅ 完成 | 冻结 `GN_Spatial.h`、`GN_Shader.h`、`GN_Pipeline.h`（契约注释含语义与边界），接入 `GN.h` |
| P1a | ✅ 完成（库+单测） | `GN_Shader.*`、`GN_Pipeline.*`、`_Vulkan/vkShader.cpp`、`_Vulkan/vkPipeline.cpp`、`testPipeline`（17 用例）；有效行 39/303/62/262/497 |
| P1b | ✅ 完成（11 条管线全部接入） | 2D 四条混合 + 3D 前向/PBR/阴影 + 合成 + 天空盒 + G-Buffer(3 附件) + 延迟光照；`GN_VkContext::aPipelineCache` 统一持有，`GN_VkShutdown` 先 Release 再销毁 renderPass。**GPU 运行时未验证** |
| P1b 契约补充 | ✅ | `aDepthBiasEnable/Constant/Slope`（阴影 1.25/1.75）；`aColorAttachmentCount==0` = 深度专用（无 FS、subpass 0、颜色混合 0 附件） |
| P1c（CPU 侧） | ✅ 描述/校验/去重键（Vulkan 描述符集留 P8） | `GN_Material.h`（35）/`GN_Material.cpp`（82）：`sMATERIAL_DESC`/`sMATERIAL_RES`、`ValidateMaterialDesc`、FNV-1a 64 位内容键、`MaterialDescEqual`、`cMATERIAL_CACHE` 去重表（句柄槽 `aDescriptorSet` 留 P8）；`testMaterial` 3 用例 |
| P1c（GPU 侧） | ⏳ 未开始 | VkDescriptorSet 创建/写入、UBO/纹理绑定、与管线布局合并（GPU 桌面验证） |
| P4 | ✅ 完成 | `GN_Spatial.cpp`（AABB/射线/视锥）、`GN_SpatialBVH.cpp`、`GN_SpatialTree.cpp`、`testSpatial`（11 用例）；有效行 219/167/241/499 |
| P5 | ✅ v1 + 胶囊 + warm start + CCD + DebugDraw + 关节 + 三角网 | `GN_Physics.h` 契约；`GN_Physics.cpp`（世界/宽相/休眠/射线/warm start 缓存，迭代 30 次，498）、`GN_PhysicsCollision.cpp`（dispatch，376）、`GN_PhysicsCollisionCapsule.cpp`（球/胶囊/盒-胶囊，319）、`GN_PhysicsBody.cpp`（构造/惯量/AABB，196）、`GN_PhysicsJoint.cpp`（距离/铰链/固定，380）、`GN_PhysicsMesh.cpp`（三角网+BVH，212）、`GN_PhysicsCcd.cpp`（扫掠 TOI，112）、`GN_PhysicsDebug.cpp`（线框/接触标记，108）、`GN_PhysicsSolver.cpp`（265）；`testPhysics` 19 + `testPhysicsCollision` 10 + `testPhysicsSolver` 14 + `testPhysicsCapsule` 12 + `testPhysicsDebug` 5 + `testPhysicsJoint` 6 + `testPhysicsMesh` 4 = 70 用例 |
| P5 剩余 | ✅ 完成 | P5b 六项全部落地。**已知限制**：7 层及以上堆叠坍塌（30/40 次速度迭代都复现）；盒对静态三角网 v1 未实现（球/胶囊可用）；三角网接触不参与 warm start |
| P6 | ✅ CPU 模拟（GPU 上传/绘制留 P8/P9） | `GN_3Dcloth.h`（58）/`GN_3Dcloth.cpp`（234）：规则网格、XPBD 距离约束（结构/剪切/弯曲）、球体碰撞、法线重建；`testCloth` 6 用例（参数/拓扑、固定角+下垂+侧风、球碰撞、确定性、Reset、法线） |
| P7 | ✅ CPU 模拟（实例上传/绘制留 P8） | `GN_3Dparticle.h`（59）/`GN_3Dparticle.cpp`（170）：固定池、按速率发射、圆锥随机方向（固定 xorshift 种子）、重力/阻尼、寿命回收、颜色插值；`testParticle` 6 用例（参数、发射/寿终、容量、重力、确定性、Reset） |
| P3 | ✅ CPU（glTF 导入留 P2，GPU 蒙皮留 P8/P9） | `GN_3Danim.h`（56）/`GN_3Danim.cpp`（190）：通道采样 STEP/LINEAR + 旋转 slerp、clip duration、`cANIM_PLAYER` 的 loop/crossFade/非循环停住；`testAnimation` 6 用例（采样/STEP/slerp、未覆盖节点、loop、crossFade、非循环停住） |
| P2（骨骼部分） | ✅ CPU（glTF 导入待第三方头） | `GN_3Dskeleton.h`（21）/`GN_3Dskeleton.cpp`（43）：关节父链世界矩阵、palette = world*inverseBind、坏父索引按根降级；`testSkeleton` 6 用例（空、单根、父链、绑定姿势恒等、坏父、长度不符） |
| P8（CPU 侧） | ✅ cMODEL 世界 AABB + 视锥剔除（渲染接入待 GPU） | `GN_3Dcull.cpp`（61）：顶点×变换求世界 AABB、`isVisibleIn`、`cullModels` 可见列表（上限/空指针安全）；`testCull` 3 用例（AABB/变换、视锥可见性、可见列表上限） |
| P2（glTF 导入）/P1c/P8 渲染接入/P9 | ⏳ 未开始 | —— |

P4 的"剔除结果与暴力测试一致"验收已通过：BVH 512 盒 40 次 AABB 查询与暴力集合逐元素相等；射线 20 条与暴力最小 t 对照；
视锥 300 点/300 盒分别与"投影后 NDC 判定"和"8 角点全在外侧"暴力判据对照；动态树 350 步随机 Insert/Update/Remove
每 20 步与活跃槽位暴力重叠集合相等。**尚未接入渲染**（`cMODEL::Draw` 前的视锥剔除属 P8）。

P5 v1 的验收：球从 2m 下落停在地面（y=r±0.05，静止后入睡）、三层 1kg 盒堆叠 5s 不塌且全部入睡、
两球对撞分离、运动学盒推动态球、射线命中最近刚体、休眠/唤醒、确定性（两次运行位差 bitwise 相等）、
非法输入不崩溃。**修复记录**：位置修正原先按"接触点数 × 迭代次数"重复施加固定穿透（4×4≈3.2 倍），
三层堆叠会被顶散；改为按 `pointCount × iterations` 均摊后稳定（`GN_PhysicsSolver.cpp:296-301`）。

P5b 已验收部分（2026-10-27）：
- **胶囊**：球/盒/胶囊两两窄相（`CollideBoxCapsule` 用线段-盒三分法 + 面流形 4 采样）、胶囊惯量/世界 AABB、
  射线近似（沿轴 17 球采样，`GN_Physics.cpp:121` 注明误差）；`testPhysicsCapsule` 12 用例 + `testPhysics` 3 例。
- **warm starting**：`sCONTACT_CONSTRAINT::aAccNormal/aAccTangent` 由世界按"刚体对 + 接触点 5cm 就近匹配"
  从上一步缓存预填（`GN_Physics.cpp` `seedWarmStart`/`cacheContacts`），求解器先重放这些冲量再迭代；
  速度迭代由 20 提到 **30** 后，3/4/5/6 层堆叠入睡步数 **29/29/29/29**；同配置关闭 warm start 为
  **60/29/125/塌**。6 层堆叠 5s 内稳定入睡（`physicsTallStackStable`）。
  `RemoveBody` 清空缓存（句柄复用）。缓存为空时与冷启动逐位一致（`testWarmStartEmptyCacheMatchesColdStart`）。
- **CCD v1**：`setContinuousCollision(true)`（默认关闭）时按「上一位置 → 目标位置」线段扫掠静态/运动学刚体的
  世界 AABB（按本刚体包围半径扩展），在最早 TOI 处钳住并清零进入方向的法向速度（`GN_PhysicsCcd.cpp`，112 行）。
  `testPhysics` 3 例：120 m/s 小球不穿 0.1m 薄地面、90 m/s 胶囊被拦、常速下落仍正常入睡；
  对照组（不开 CCD）同速小球一步穿到地面顶面以下。近似边界见 §3.3。
- **DebugDraw（CPU 线段）**：`DebugDraw(sDEBUG_DRAW&)` 输出所有刚体形状线框 + 上一步接触点/法线；
  线数口径：球 3×24、盒 12、胶囊 2×24+4、每接触点 4（十字 + 0.2 长法线）；`GN_PhysicsDebug.cpp`（108）。
  `testPhysicsDebug` 5 用例：空世界清空、盒包围盒=半尺寸、球端点距心=半径、胶囊端圆位置/半径、接触标记与法线长度。
  v1 边界：胶囊线框只画圆柱端圆与侧线（半球顶不在线框里）；渲染器消费在 P8。
- **关节/约束（距离/铰链/固定）**：`AddJoint(sJOINT_DESC)` / `RemoveJoint` / `GetJointCount`；世界每子步构建
  运行时关节，速度层消去被约束方向的相对速度 + Baumgarte 误差项，位置层修正锚点/轴向误差
  （`GN_PhysicsJoint.cpp`，380）。静态/运动学/睡着的动态刚体在关节内按无限质量；`RemoveBody` 连带删除
  引用它的关节；v1 无关节 warm start。`testPhysicsJoint` 6 用例：非法输入/连带删除、距离保持、
  固定关节局部系相对位姿、铰链绕轴自由、铰链消去垂直角速度、确定性。
- **静态三角网（v1）**：`AddStaticMesh(const sMESH&)` 追加世界坐标三角形（按 VkVertex3D 布局取前 3 个 float），
  `cBVH` 宽相 + 球/胶囊窄相（球心/胶囊线段到三角形最近点），`GN_PhysicsMesh.cpp`（212）。
  边界：盒对三角网 v1 未实现（跳过）、三角网接触不参与 warm start、A 端为内部静态代理（不占刚体句柄）。
  `testPhysicsMesh` 4 用例：输入校验/退化剔除、球静止、横放胶囊静止、200 三角形 BVH 不漏。
- **仍不稳**：7 层及以上堆叠坍塌（30/40 次迭代都复现）。定位：每步强制唤醒的 6 层对照仍坍塌 ⇒ 排除休眠路径，
  是速度迭代预算/收敛不足；6 层在 20 次迭代下会下沉、30 次迭代后稳定（本项已把上限由 5 层提到 6 层）。

P6 v1 的验收（CPU）：5×5 软布四角固定后重力下垂、侧风把中间顶点吹离 z=0 平面、球体碰撞把顶点推到球面外、
两次模拟逐位一致、`reset()` 回到初始网格、法线有限且单位长度。**边界**：只固定四角（不支持任意点集/整行固定）、
风按加速度处理（不做面积/法线投影）、无自碰撞/摩擦、无 GPU 上传与绘制（P8/P9）。

P7 v1 的验收（CPU）：参数非法拒绝；`rate=100, dt=0.1` 发射 10 个；停发后按寿命全部回收；发射速率远大于容量时
存活数钳在 `maxParticles`；初速 0 的粒子在重力下 `v.y = g*dt` 且位置向下；同参数两次模拟池内容逐位一致；
`reset()` 清空存活粒子并保留容量/参数。**边界**：无纹理/混合模式/GPU 实例缓冲（`Upload`/`Draw` 留 P8）。

P3 v1 的验收（CPU）：LINEAR 平移中点/STEP 取前一帧、旋转最短弧 slerp（identity→90° 在 t=0.5 为 45°）、
未覆盖节点返回 false 且不改 out、loop 按 duration 回绕、crossFade 权重线性过渡并在结束后切到下一段、
非循环到末尾停住且采样停在最后一帧。**边界**：CUBICSPLINE 未实现；glTF clip 导入依赖 P2；无骨骼/蒙皮
（GPU 蒙皮留 P8/P9）。

P2 骨骼部分 v1 的验收（CPU）：单根世界平移 = 局部平移、两节父链叠加、绑定姿势下 palette ≈ identity、
坏父索引（父 >= 自身）按根降级不崩溃、局部姿势数量不匹配返回 false。**边界**：glTF 导入（节点/网格/皮肤/
动画）需引入 `cgltf` 并登记 `external/README.md`；蒙皮顶点混合与关节 UBO 留 P8/P9。

P8 CPU 侧 v1 的验收：`cMODEL::computeWorldAABB` 用顶点 × `aTransform` 得到世界包围盒（缩放/平移纳入）；
`sFRUSTUM::FromViewProj` 下原点/平面内模型可见、远处/相机后方不可见、`aVisible=false` 不可见；
`cullModels` 只写可见下标并尊重 `maxOut`/空指针。**边界**：尚未接入 `GN_VkDraw3D` 的绘制循环
（draw 次数下降与逐像素一致需要 GPU 桌面验证）。

P1c CPU 侧 v1 的验收：合法描述通过校验；管线键为 0、绑定数 >8、binding 重复/非递增、资源为空、种类非法、
阶段位为 0 都被拒绝；内容相同（含资源地址）键相同且逐字段相等，任一字段变化键与相等性都变化；
去重表对同描述返回同句柄、不同资源新建、非法描述拒绝、`Release` 清空。**边界**：VkDescriptorSet 创建/写入与
UBO/纹理实际绑定留 P8（需要 GPU）。

### 3.2 P1b 的契约演进与剩余缺口（2026-10-27 更新）

**已解决**（契约扩展，`GN_Pipeline.h` 已冻结）：
1. ~~没有固定 viewport/scissor~~ ⇒ 新增 `aDynamicViewport` / `aDynamicScissor` + `VkViewport aViewport` / `VkRect2D aScissor`
   （PBR 的 y=H、height=-H 负高度视口可精确表达）。
2. ~~无法表达"无深度状态"~~ ⇒ 新增 `aDepthStencil`（2D renderPass 无深度附件，必须 false）。
3. ~~2D alpha 因子无法复刻~~ ⇒ 新增 `aCustomBlend` + 四个 `VkBlendFactor`；既有 2D 用
   color=(SRC_ALPHA, ONE_MINUS_SRC_ALPHA)、alpha=(ONE, ZERO)，bright 只把 dst 改 ONE，multiply 用 (DST_COLOR, ZERO)。
4. ~~缓存生命周期~~ ⇒ `GN_VkContext::aPipelineCache`，`GN_VkShutdown` 先 `Release()` 再销毁 renderPass / device。

**仍存在的缺口**（只记录，未处理）：
1. 缓存键只有 64 位内容哈希，`cSHADER_CACHE`/`cPIPELINE_CACHE` 均无二次内容比较 ⇒ 哈希碰撞会把不同 SPIR-V/描述认成同一项
   （理论风险，实测无碰撞）。若要求"零风险"，需在条目里存字节/描述并二次比较，属头文件变更。
2. 深度专用（0 颜色附件）不强制 `aDepthStencil=true`（Vulkan 允许不提供深度状态）；当前只有阴影管线用这条路径，测试只覆盖 `true` 形态。
3. 深度专用描述若误设 `aCustomBlend=true`，因子仍参与键与校验（虽然用不到）。
4. 动态开关为 true 时 viewport/scissor 数值仍参与缓存键（站点统一留默认值；差异只多一次 miss，不影响正确性）。
5. `Raycast` 要求 `dir` 已归一化；平行轴判定依赖 IEEE `1/0 = ±inf`（构建不得开 `-ffast-math`）。

### 3.3 P5b 的契约演进（胶囊 / warm starting / CCD，2026-10-27）

**头文件变更**（`GN_Physics.h`，均为新增，不破坏既有 4 参数调用）：
1. `SolveVelocities(..., uint32_t iterations, bool warmStart = false)`；默认 false 时求解器仍清零后迭代。
2. `cPHYSICS_WORLD` 私有区新增 `sWARM_CONTACT` 缓存、刚体对索引与 `seedWarmStart`/`cacheContacts`；
   缓存不跨 `RemoveBody` / `Clear` 保值（句柄复用会清空）。
3. `sCONTACT_CONSTRAINT::aAccNormal/aAccTangent` 语义改为"世界可预填的累计冲量"；字段与布局未变。
4. 新增 `cPHYSICS_WORLD::setContinuousCollision(bool)`（默认 false）+ 私有 `resolveContinuousCollisions`；
   CCD 状态不进入 `sRIGID_BODY` 布局，旧代码（含手工构造刚体的单测）无需改字段初始化。
5. 新增 `sDEBUG_LINE` / `sDEBUG_DRAW`（纯 CPU 线段收集器）与 `cPHYSICS_WORLD::DebugDraw(sDEBUG_DRAW&)`；
   本层不含颜色/线宽/句柄，渲染器消费在 P8（`_Vulkan/vkDebugDraw.cpp`）。
6. 新增 `eJOINT_TYPE` / `sJOINT_DESC` / `sJOINT_CONSTRAINT`、`AddJoint` / `RemoveJoint` / `GetJointCount`
   与 `solveJointVelocities` / `solveJointPositions`；关节持久数据存世界私有区，`RemoveBody` 连带删除。
7. 新增 `AddStaticMesh(const sMESH&)`（头文件只前向声明 `sMESH`，不把 GN_3Dmodel.h 拉进物理头）与
   `GetStaticTriangleCount()`；三角网数据 + BVH 存世界私有区，接触 A 端为内部静态代理（不入 `aBodies`），
   `Clear()` 一并清理。
8. 新增 `GN_3Dcloth.h`（`sCLOTH_DESC` / `sCLOTH_COLLIDER` / `cCLOTH`）并接入 `GN.h`；纯 CPU 模拟，
   GPU 上传/绘制未做（P8/P9）。
9. 新增 `GN_3Dparticle.h`（`sPARTICLE` / `sPARTICLE_EMITTER_DESC` / `cPARTICLE_SYSTEM`）并接入 `GN.h`；
   纯 CPU 池模拟，实例上传/绘制未做（P8）。
10. 新增 `GN_3Danim.h`（`sANIM_CHANNEL` / `sANIM_TRANSFORM` / `sANIM_CLIP` / `cANIM_PLAYER`）并接入 `GN.h`；
    采样/混合/状态机纯 CPU，glTF 导入留 P2，GPU 蒙皮留 P8/P9。
11. 新增 `GN_3Dskeleton.h`（`sJOINT` / `sSKIN` / `sSKELETON`）并接入 `GN.h`；世界矩阵与调色板纯 CPU，
    glTF 导入与 GPU 关节 UBO/蒙皮留 P2 后续/P8。
12. `cMODEL` 新增 `aWorldAABB` / `computeWorldAABB()` / `isVisibleIn()`；新增 `cullModels()` 可见列表
    （`GN_3Dcull.cpp`）。渲染循环接入仍待 P8 的 GPU 验证。
13. 新增 `GN_Material.h`（`sMATERIAL_BINDING` / `sMATERIAL_DESC` / `sMATERIAL_RES` / `cMATERIAL_CACHE`）并接入
    `GN.h`；CPU 侧校验/内容键/去重，VkDescriptorSet 创建与 UBO/纹理绑定留 P8。

**已知边界**：warm start 只重放**标量**冲量，切线方向按本步相对速度重算 ⇒ 高速滑动/旋转接触不保留历史切向
（v1 近似）；接触点匹配半径 5cm，超出视为新接触（冷启动）；休眠体的接触不入缓存。

**CCD 的近似**：目标用世界 AABB 按本刚体包围半径扩展 ⇒ 盒/胶囊的棱角会**提前**停下（保守，不漏判）；
运动学目标只按步末位置参与扫掠（其步内自身运动不扫掠）；命中后本步剩余位移丢弃、不施加恢复系数；
只处理动态且醒着的刚体；起点已在扩展盒内时不在此处理（交给接触求解）。

**关节的边界**：v1 不做关节 warm starting；固定/铰链要求添加时两锚点重合（否则第一步会被立即拉齐）；
铰链轴为零向量时退回 (0,1,0)；睡着的动态刚体在关节内按无限质量，不会被关节唤醒（与接触的唤醒规则不同）。

**三角网的边界**：只支持球/胶囊（盒 v1 跳过）；网格必须已是世界坐标（不读 `sMESH` 之外的变换）；
三角网接触不参与 warm start；`AddStaticMesh` 每次追加后立即重建 BVH（大网格应合并后一次添加）。

## 4. 关键决策（需拍板）

1. **物理后端**：自研最小刚体（推荐，C++11、无新依赖）vs Bullet（大依赖、构建变重）；Jolt 需 C++17，暂不选。
2. **空间划分**：动态 AABB 树 + 静态 BVH（推荐）；octree 不选。
3. **资产格式**：glTF 2.0 + `cgltf`（推荐）。
4. **布料/粒子**：CPU 先行（推荐），GPU compute 放 P9。
5. **管线描述**：显式 `sPIPELINE_DESC`（推荐），不引 SPIRV-Cross。
6. **骨骼上限**：128 关节 UBO（推荐），超出分批；P9 评估 SSBO。

## 5. 风险

- 自研物理工作量最大（P5），先冻结 v1 形状/求解器范围，不要一开始做凸包/CCD/关节。
- 描述符池按旧管线尺寸配置，新增材质/蒙皮/粒子需扩容；P1 一并调整 `descriptorPool`。
- 关节 UBO 每 draw 更新会阻塞；P2 用 per-frame 环形 UBO，P9 改 SSBO。
- 布料/粒子/蒙皮都依赖动态顶点/实例上传；复用 `Ensure3DBufferCapacity` 思路，避免每帧销毁重建。
- 无 GPU 的 CI 只能验证 CPU 数学/构建；渲染正确性由桌面 GPU 测试确认（旧 `run.sh` 已移除，测试待重新设计）。
- cgltf 为第三方头文件，需登记 `external/README.md` 并在 `make` 中只编译一次实现。

## 6. 建议的首个可交付

先做 **P1 + P4**：通用管线缓存 + 空间划分/视锥剔除。两者都不依赖资产格式，能立即改善渲染并给物理 broadphase 打基础；随后 P2/P3（骨骼/动画）与 P5/P6（物理/布料）可并行推进。

**执行结果（2026-10-27）**：P1 拆成 P1a/P1b/P1c 后，P1a + P1b + P4 已完成并可无 GPU 验证；P1b 覆盖全部 11 条既有管线
（2D 四条混合、3D 前向、PBR、阴影、合成、天空盒、G-Buffer(3 附件)、延迟光照），描述字段按站点逐项对齐旧 `VkPipelineCreateInfo`。
1. **桌面验收（必须先做，GPU 环境才能做）**：用重新设计的 GPU 测试逐项对照（2D 四混合、前向、PBR/场景、阴影/延迟/天空盒/G-Buffer）；旧 `run.sh` 已移除；
   失败时按站点回滚（每站点内注释给出 desc 与旧状态的对应关系）。GPU 不可用的环境无法代替这一步。
2. **P1c**：GPU 材质实例（管线 + 描述符集 + UBO/纹理），把 `GN3d::sMATERIAL`（CPU 属性）与 GPU 资源绑定分离。
3. **P4 接入渲染**（P8 的前半）：`cMODEL` 增加世界 AABB + 可见列表，`GN_VkDraw3D` 前做 `sFRUSTUM::IntersectsAABB` 剔除；
   验收：同一场景逐像素不变 + draw 次数下降。
4. P2/P3（glTF/骨骼/动画）与 P5/P6（物理/布料）依赖 P1c 的材质实例与 P4 的 broadphase，建议在桌面验收通过后再并行。
