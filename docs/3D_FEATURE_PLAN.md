# 3D 能力扩展规划：空间划分 / 物理 / 粒子 / 动画 / 骨骼 / 布料 / 通用管线

> 状态：P0 契约冻结 + P1a（管线/着色器缓存库）+ P1b（**11 条既有管线全部接入缓存**）+ P4（空间划分）+
> P5 v1（球/盒刚体物理）+ P5b 前两项（胶囊形状、warm starting）已落地（2026-10-27）；
> P1c、P5 剩余能力（CCD/关节/静态三角网/DebugDraw）与 P2+ 未开始。
> 基线：C++11、Vulkan 1.4、Makefile 单一构建入口；现有 3D 为静态网格 + Blinn-Phong/PBR + 阴影/延迟/天空盒。
> 约束：不破坏现有 `GN_VkDraw3D`/`GN_VkDrawPBR`/`cMODEL` API；新文件 camelCase，单功能 <500 有效行、多功能 <1000 有效行。
> 2D 侧现有的 `GN_2DArea` 碰撞与 `cFRAME_LAUNCHER` 动画图筐发射器**保持不变**，新系统只服务 3D。
>
> **本轮验收证据**（2026-10-27，`make -j8 check`）：单元测试 **152 cases, 0 failed, 0 assertions failed**
> （基线 67 + P1a/P1b 22 + P4 11 + P5 52）；`lintCodeModularity src/graphics tests/unit --naming --comments`
> 仅剩既有 `GN_2Dmath.cpp` 超限与既有 `gn3d_pbr_frag_spv.h` 告警，新增/改动文件 0 违规 0 告警。
> 物理新增文件有效行：`GN_Physics.cpp` 476、`GN_PhysicsSolver.cpp` 265、`GN_PhysicsBody.cpp` 196、
> `GN_PhysicsCollision.cpp` 376、`GN_PhysicsCollisionCapsule.cpp` 319（均 <500）。
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
| P1c | ⏳ 未开始 | GPU 材质实例（管线 + 描述符集 + UBO/纹理） |
| P4 | ✅ 完成 | `GN_Spatial.cpp`（AABB/射线/视锥）、`GN_SpatialBVH.cpp`、`GN_SpatialTree.cpp`、`testSpatial`（11 用例）；有效行 219/167/241/499 |
| P5 | ✅ v1 + 胶囊 + warm start | `GN_Physics.h` 契约；`GN_Physics.cpp`（世界/宽相/休眠/射线/warm start 缓存，476）、`GN_PhysicsCollision.cpp`（dispatch，376）、`GN_PhysicsCollisionCapsule.cpp`（球/胶囊/盒-胶囊，319）、`GN_PhysicsBody.cpp`（构造/惯量/AABB，196）、`GN_PhysicsSolver.cpp`（265）；`testPhysics` 16 + `testPhysicsCollision` 10 + `testPhysicsSolver` 14 + `testPhysicsCapsule` 12 = 52 用例 |
| P5 剩余 | ⏳ | CCD、关节/约束、`AddStaticMesh` 三角网、`DebugDraw` 线框；P5b。**已知限制**：6 层及以上堆叠持续下沉（3s 内顶层下沉约 1m），约 10s 内坍塌；5 层以内稳定入睡 |
| P2/P3/P6–P9 | ⏳ 未开始 | —— |

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
  实测 3/4/5 层堆叠入睡步数 91/塌/塌 → **29/29/63 步**，5 层堆叠 5s 内稳定入睡（`physicsTallStackStable`）。
  `RemoveBody` 清空缓存（句柄复用）。缓存为空时与冷启动逐位一致（`testWarmStartEmptyCacheMatchesColdStart`）。
- **仍不稳**：6 层及以上堆叠持续下沉（3s 内顶层下沉约 1m、约 10s 坍塌）——位置修正每步只消 20% 超容差穿透，
  深堆叠累积穿透追不上；需在 P5b 下一项（CCD/约束）或专门求解器迭代里处理，当前不冒充已解决。

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

### 3.3 P5b 的契约演进（胶囊 / warm starting，2026-10-27）

**头文件变更**（`GN_Physics.h`，均为新增，不破坏既有 4 参数调用）：
1. `SolveVelocities(..., uint32_t iterations, bool warmStart = false)`；默认 false 时求解器仍清零后迭代。
2. `cPHYSICS_WORLD` 私有区新增 `sWARM_CONTACT` 缓存、刚体对索引与 `seedWarmStart`/`cacheContacts`；
   缓存不跨 `RemoveBody` / `Clear` 保值（句柄复用会清空）。
3. `sCONTACT_CONSTRAINT::aAccNormal/aAccTangent` 语义改为"世界可预填的累计冲量"；字段与布局未变。

**已知边界**：warm start 只重放**标量**冲量，切线方向按本步相对速度重算 ⇒ 高速滑动/旋转接触不保留历史切向
（v1 近似）；接触点匹配半径 5cm，超出视为新接触（冷启动）；休眠体的接触不入缓存。

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
