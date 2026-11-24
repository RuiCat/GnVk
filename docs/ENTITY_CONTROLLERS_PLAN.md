# 控制器实现规划：引擎侧接线 + 游戏侧底层控制

> 状态：**P0–P6 全部完成（2026-11-24）**。规划本身写于 2026-11-24。
> 前置阅读：`ENGINE_ARCHITECTURE.md`、`GI_PLAN.md` §5、`MULTILAYER_RENDER_PIPELINE_PLAN.md`。

## 1. 现状审计（先看事实）

六个模块里，**引擎侧四个已经写好、有单测、但没有任何运行时消费者**；
**游戏侧（输入/时钟/角色运动）则是完全空白**：

| 层 | 现有类型 | 规模 | 单测 | 谁在用 |
|----|---------|------|------|--------|
| 网格 | `sMESH` / `cMODEL` | 739 + 152 行 | 间接（gpuCheck 各站点） | mmdViewer / imoScene / gpuCheck |
| 骨骼 | `sJOINT` / `sSKELETON` | 57 + 48 行 | 6 用例 | **无** |
| 蒙皮 | `sSKINNED_VERTEX` / `skinVertices` / `packPalette` | 113 + 51 行 | 有 | 仅 `gpuCheckSkin`（CPU 对拍） |
| 动画 | `sANIM_CLIP` / `cANIM_PLAYER` | 223 + 89 行 | 6 用例 | **无** |
| 场景装配 | `sASSET_SCENE::build` | — | 5 用例 | **无** |
| 物理 | `cPHYSICS_WORLD` / `sRIGID_BODY` | 8 个 .cpp | 70 用例 | **无** |

七处断层（这是规划真正要解决的问题）：

1. **链路断在运行时**：`clip → local pose → palette → LBS → 绘制` 只在单测里逐段验过，
   从未有一个 demo 跑通整条。mmdViewer 加载的 PMX 是**静态** `cMODEL` 直接 `DrawPBRSorted`。
2. **物理与渲染零连接**：`cPHYSICS_WORLD` 的 `sRIGID_BODY.aPos/aRot` 不会写到任何
   `cMODEL::aTransform`，也没有 body↔entity 的映射表。
3. **每帧顺序散落在 demo 里**：mmdViewer 的 `AppDisplay2D` 有 **66 行**手写编排
   （近/远平面 → CSM 拟合 → 4 级 shadow pass → 3D pass → skybox → 云 → 地面 → 模型 →
   贴花 → 降水 → gizmo → 2D 叠层）。换一个场景就要再抄一遍。
4. **CSM 拟合是 demo 私有函数**：`mmdViewer.cpp:333 updateShadowCascades()`。
   渲染规则不该住在示例里。
5. **仓库里没有任何 3D 模型资产**（关键约束，见 §5.1）：
   - `mmdViewer` 默认读 `tests/うさうさ/うさうさ.pmx`，该路径在 `.gitignore` 里，**不在仓库内**；
     缺模型时 `loadModel()` 返回 false，`gLoaded=false`，整帧只画背景（所以 headless 冒烟能过）。
   - `imoScene` 的 GLB 由 `make imoSceneAsset` 从本机 FBX 转换，同样不入库。
6. **输入是原始键码，没有动作映射（Action Mapping）**：两个 demo 里共 **42 处**直接读全局输入
   （`GN::Key1[GK_*]` 37 处 + `GN::Mouse[]` 5 处）。分布：`Key1` — mmdViewer 20、
   cornellBox 14、各相机文件各 1；`Mouse` — mmdViewer 2、imoScene 2、cornellBox 1。
   后果：键位改一次要翻遍游戏逻辑；且 `Key1[]` 只有“当前按住”，
   “本帧刚按下”（边沿检测）要在每个使用点自己存上一帧状态。
7. **没有游戏时钟，也没有角色控制器**：
   - 固定步长累加器只存在 `cPHYSICS_WORLD` 内部（`aAccumulator`）；游戏逻辑（状态机、
     计时器、AI）、动画、输入边沿各自拿 `AppRun` 的 `dt`，来源不统一。
   - 物理只有刚体（`sRIGID_BODY`），**没有胶囊+地面检测+跳跃+斜坡**的角色运动层；
     全仓 `grep CharacterController/IsGrounded` 为空。

> 结论：两条线要分开做。
> **引擎侧**（网格/动画/骨骼/物理）的价值是“接线 + 固化顺序”，算法已写完（Skeleton/Anim/
> Physics 求解），不重写；**游戏侧**（输入/时钟/角色运动/场景）是真正的空白，
> 新代码但不涉新算法，且优先级更高。

## 2. 两层控制器：引擎侧接线 + 游戏侧底层控制

“控制器”在本仓库已有先例：`cCAMERA::ApplyInput(const sCAMERA_INPUT &input, float dt, ...)`
（`GN_3DcameraControl.cpp`）——**吃一个意图结构、不读全局输入**、只做它自己那一件事。
新层全部沿用这个形状。

必须分开看的两层：

| 层 | 面向 | 内容 | 使用者 |
|----|------|------|--------|
| **引擎侧接线**（§2.1–2.4） | 把已写好的算法接到渲染 | 网格/动画/骨骼/物理四个控制器 | 引擎内部、`cENTITY` |
| **游戏侧底层控制**（§2.5） | 做游戏的人直接调 | 输入映射 / 游戏时钟 / 角色运动 / 场景 | 游戏逻辑 |

引擎侧四个控制器**不是平级对象**，而是一条主链 + 一个旁支：

```
        ┌──────────────┐
        │ 物理控制器    │──(world transform)──┐
        └──────────────┘                     │
                                             ├──→ 网格控制器 ──→ 绘制
        ┌──────────────┐   local pose   ┌────┴──────────────┐
        │ 动画控制器    │───────────────→│ 骨骼控制器         │
        └──────────────┘                │ (palette)          │
                                        └────────────────────┘
```

固定更新顺序 **物理 → 动画 → 骨骼 → 网格**。理由：

- 物理最先：角色根位移/朝向是本帧动画的参数（速度→walk 强度、落地→受击），
  必须在本帧动画采样之前定下来。
- 骨骼在动画之后：调色板需要本帧最终局部姿势。
- 网格最后：LOD 选级与透明排序需要相机距离，而相机在同帧已更新。

### 2.1 `cMESH_CONTROLLER`

| 项 | 内容 |
|----|------|
| 拥有 | `cMODEL*`（非拥有引用）、LOD 选级策略、剔除视锥、透明排序开关 |
| 输入 | `sMAT44 world`（来自物理或父层级）、`const sMAT44* palette`（可选，蒙皮用）、相机位置 |
| 输出 | 绘制调用序列（内部调 `DrawPBRWithLod` / `DrawPBRSorted` / `DrawShadow`） |
| 职责 | ① GPU 资源生命周期（`UploadAllToGPU`/`ReleaseGPU` 的幂等化）② LOD 选级（按屏幕占比 + 误差阈值）③ 视锥剔除 ④ 透明两趟排序 ⑤ 蒙皮网格的 LBS（有 palette 时） |
| **不做** | 不碰 `GN_VkBegin/End*Pass`（渲染编排归 `cRENDER_GRAPH`，见 §3） |

现状差距：LOD 选级目前是调用方自己算 `levels[]` 再传 `DrawPBRWithLod`，
控制器要把"距离+误差→级别"固化下来。

### 2.2 `cANIM_CONTROLLER`

| 项 | 内容 |
|----|------|
| 拥有 | `cANIM_PLAYER`、clip 表（名→`sANIM_CLIP*`）、状态机（状态＝clip 名 + 转换规则） |
| 输入 | `dt`、参数包（`speed`、`verticalSpeed`、`trigger` 标志） |
| 输出 | 每节点局部姿势 `vector<sANIM_TRANSFORM>` + 事件（脚步、动画结束、命中窗口） |
| 职责 | ① clip 注册 ② 参数→状态转换（含 crossFade 时长）③ 时间驱动 ④ 事件派发 ⑤ 单次/循环控制 |
| **不做** | 不算世界矩阵、不算调色板（那是骨骼控制器） |

现状差距：`cANIM_PLAYER` 只有 `play/crossFade`，**没有状态机**——调用方得自己判断
"现在该播哪段"。这是本控制器的主要增量。

### 2.3 `cSKELETON_CONTROLLER`

| 项 | 内容 |
|----|------|
| 拥有 | `sSKELETON`、调色板缓冲（`vector<sMAT44>`）、GPU `sSKIN_UBO` 上传状态 |
| 输入 | 局部姿势（来自动画控制器，或直接给定姿势）、根变换 |
| 输出 | `palette`（CPU）+ 已上传的 GPU 关节 UBO |
| 职责 | ① `computeWorld`/`computePalette` ② `packPalette`（128 上限截断）③ 上传到 `GN_VkDraw3DSkinned`/`SkinnedSsbo` ④ 按名查关节、求 attach 世界矩阵（挂点武器/特效）⑤ 骨骼调试线框 |
| **不做** | 不采样 clip、不做顶点混合（顶点混合归网格控制器） |

现状差距：**这条是最大的空白**——`computePalette → packPalette → 上传` 的
GPU 侧从未在运行时跑过。注意 `SKIN_MAX_JOINTS = 128`，而 MMD 模型常超 128，
需要"UBO 路径（≤128）/ SSBO 路径（>128）"自动分流——SSBO 管线已有
（`vkSkinSsbo.cpp`），但没有选路逻辑。

### 2.4 `cPHYSICS_CONTROLLER`

| 项 | 内容 |
|----|------|
| 拥有 | `cPHYSICS_WORLD`、`entityId ↔ bodyHandle` 映射、碰撞回调表 |
| 输入 | `dt`、实体注册/注销 |
| 输出 | 每实体的世界变换（写到网格控制器）、碰撞事件 |
| 职责 | ① `Step(dt)` ② 变换同步（body→entity，含插值）③ raycast 封装 ④ 触发器/接触事件派发 ⑤ 调试线框 |
| **不做** | 不做布娃娃（骨骼↔刚体耦合）；不做角色运动（见 §2.5.3 `cCHARACTER`） |

现状差距：**变换同步完全不存在**。另外 `Step` 用固定步长+累加器，
若一帧内跑多个子步，渲染需要**插值**（否则 60Hz 物理 + 144Hz 渲染会抖）——
这需要一个 `sPHYSICS_SNAPSHOT {prev, curr, alpha}`，现在没有。

### 2.5 游戏侧底层控制（做游戏时直接调的四件东西）

前三件是**现在完全不存在、且做任何游戏都会立刻撞上**的。

#### 2.5.1 `cINPUT_MAP` — 动作映射（替掉 42 处直读全局输入）

现状：`GN::Key1[]` 只有“当前是否按住”；游戏要的“本帧刚按下/刚松开”（边沿）得在
每个使用点自己存上一帧快照。

```cpp
typedef uint16_t eACTION;  // 游戏自定义编号，引擎不预设语义
enum eINPUT_EDGE { eINPUT_DOWN, eINPUT_HOLD, eINPUT_UP };

class cINPUT_MAP {
  public:
    // 绑定：一个动作可绑多个键（键盘 + 手柄/鼠标）；key 用现有 GK_* 编号
    void Bind(eACTION action, uint16_t key);
    void Unbind(eACTION action, uint16_t key = 0xFFFF);
    void LoadDefaultWASD();          // 便利函数：WASD+空格+Shift+鼠标，不强制

    // 每帧开头调一次：采样全局输入、算边沿。必须在游戏逻辑之前。
    void Update(float dt);

    bool Down(eACTION) const;        // 持续按住
    bool Pressed(eACTION) const;     // 本帧刚按下（边沿）
    bool Released(eACTION) const;    // 本帧刚松开
    bool DoubleTapped(eACTION) const;// 双击（复现现成的 GN_SetMouseDoubleClick 思路）
    float Axis1D(eACTION neg, eACTION pos) const;  // -1/0/+1
    GNg::sVEC2 Axis2D(eACTION negX, eACTION posX, eACTION negY, eACTION posY) const;

    // 把鼠标位移/滚轮/方向动作打包成相机意图（复用已有结构，不重造）
    GN3d::sCAMERA_INPUT CameraInput(float lookScale = 1.0f) const;
};
```

要点：
- **键位与语义分离**。游戏写 `map.Pressed(eACT_JUMP)`，改键只需改 `Bind`。
- **边沿只在 `Update` 里算一次**，不在每个使用点重复。
- `eACTION` 是 `uint16_t` 而**不是枚举**：引擎不预设游戏有哪些动作（否则引擎要随每个游戏改）。
- 不做上下文栈（UI 输入拦截等）——v1 用一个 `bool enabled` 全局屏蔽即可。

#### 2.5.2 `cGAME_CLOCK` — 固定步长时钟（替掉每个调用方自己算 dt）

现状：只有 `cPHYSICS_WORLD` 内部有 `aAccumulator`。游戏逻辑/动画/计时器各拿各的 dt，
一旦物理内部会跑多子步，其他层就跟它对不上。

```cpp
class cGAME_CLOCK {
  public:
    void SetFixedStep(float step, uint32_t maxSubSteps);  // 默认 1/60、4（与物理同参）
    void SetTimeScale(float scale);       // 慢动作/暂停（0 = 冻结）
    void Tick(float realDt);              // 每帧一次，内部累加

    uint32_t FixedSteps() const;          // 本帧该跑几次 FixedUpdate
    float FixedStepSize() const;
    float Alpha() const;                  // 插值因子 [0,1)，给渲染用
    float TotalTime() const;              // 已累计的游戏时间（不受 timeScale 影响可用于 UI）
    bool Paused() const;
};
```

约定每帧三段：`FixedUpdate`（跑 `FixedSteps()` 次，物理/AI/状态机）→
`Update(dt)`（每帧一次，输入采样/相机跟随）→ `Render(Alpha())`（插值）。
`Alpha()` 直接接 §2.4 的 `sPHYSICS_SNAPSHOT` 插值。

#### 2.5.3 `cCHARACTER` — 胶囊角色运动（最刚需、最缺）

物理只有刚体。做游戏最先撞到的就是“人要能走、能跳、不能卡在台阶上”。

```cpp
struct sCHARACTER_DESC {
    float aRadius = 0.3f;
    float aHeight = 1.7f;        // 胶囊总高（含两端半球）
    float aMaxSlopeDeg = 45.0f;  // 超过就当墙
    float aStepHeight = 0.35f;   // 台阶自动抬升
    float aWalkSpeed = 3.5f;
    float aSprintSpeed = 6.0f;
    float aJumpSpeed = 5.0f;
    float aGravityScale = 1.0f;
};

class cCHARACTER {
  public:
    bool Init(const sCHARACTER_DESC &desc, const cPHYSICS_WORLD *world);
    void SetWishDir(const GNg::sVEC3 &worldDir); // 水平意图，长度 0..1
    void Jump();                                  // 仅着地时生效
    void Teleport(const GNg::sVEC3 &pos);

    // 固定步长里调（FixedUpdate），内部做：重力 → 水平位移 → 胶囊扫掠 →
    // 解穿透 → 斜坡投影 → 台阶抬升 → 地面检测
    void Move(float fixedDt);

    bool IsGrounded() const;
    GNg::sVEC3 Position() const;
    GNg::sVEC3 Velocity() const;
};
```

关键取舍（写清楚，免得实现时反复摇摆）：

| 方案 | 结论 | 理由 |
|------|------|------|
| 刚体 + 摩擦力 | **不采用** | 角色被卡在台阶/斜坡边缘、站立时持续微滞是常见现象，需要大量调参 |
| **运动学胶囊 + 扫掠解穿透** | **采用** | 可控、可重现；只复用 `cPHYSICS_WORLD` 的静态三角网与 `Raycast`，不向世界加刚体 |
| 布娃娃/骨骼耦合 | 不做 | 与 §7 一致 |

复用现有的：`cPHYSICS_WORLD::Raycast`（地面检测）、`AddStaticMesh`（碰撞几何）、
`GN_PhysicsCollisionCapsule.cpp`（胶囊基础运算）。

#### 2.5.4 场景/实体注册（最小集，不是 ECS）

```cpp
class cSCENE {
  public:
    cENTITY *Spawn(const sENTITY_DESC &desc);
    void Destroy(cENTITY *e);       // 延迟到帧末，避免迭代中释放
    void ForEachVisible(const sFRUSTUM &f, fn); // 剔除入口，接 §2.1 的网格控制器
    void Update(float dt, const cINPUT_MAP &input);
};
```

存的是指针数组 + 帧末回收表，不引入组件池、不做序列化。

## 3. 每帧编排：谁来写那 66 行

控制器只管"自己的那一段"，**不负责 pass 顺序**。建议新增 `cRENDER_FRAME`
（薄封装，不叫渲染图，避免过度设计）：

```cpp
// 伪代码：把 mmdViewer 里手写的顺序固化下来
struct sFRAME_INPUT {
    const GNg::sMAT44 *viewProj;
    const GNg::sVEC3 *eye;
    const GN3d::sLIGHT_UBO *light;
    float aspect, nearZ, farZ;
};

class cRENDER_FRAME {
  public:
    void Begin(const sFRAME_INPUT &in);          // 算 VP、拟合 CSM、写 light UBO
    void ShadowPass(std::span<cMESH_CONTROLLER*> casters);  // 4 级 cascade
    void GeometryPass(std::span<cMESH_CONTROLLER*> scene);  // skybox→不透明→蒙皮→透明→贴花
    void End();                                  // End3DPass + 合成 + Begin2DPass
};
```

**验收标准**：mmdViewer 的 `AppDisplay2D` 从 66 行降到 ~8 行
（构造 input + 三次调用 + 自己的 HUD）。

## 4. 接口草案（关键签名）

```cpp
// GN_Entity.h
enum eENTITY_FLAGS { eENTITY_VISIBLE = 1, eENTITY_CAST_SHADOW = 2, eENTITY_SKINNED = 4 };

struct sENTITY_DESC {
    GN3d::cMODEL *aModel;               // 既有的网格容器
    const GN3d::sSKELETON *aSkeleton;   // 可空（静态模型）
    uint32_t aFlags;
    uint32_t aBody;                     // physics handle，INVALID = 无物理
};

class cENTITY {
  public:
    bool Init(const sENTITY_DESC &desc);
    void Update(float dt);         // ①物理同步(只读) ②动画 ③骨骼 ④网格
    void DrawGeometry();           // 由 cRENDER_FRAME 调用
    void DrawShadow();
    const GNg::sMAT44 &WorldTransform() const;
};

// GN_Player.h —— 游戏侧：把“输入意图”变成“角色运动”（不碰渲染）
struct sPLAYER_DESC {
    const cINPUT_MAP *aInput;
    cCHARACTER *aCharacter;
    GN3d::cCAMERA *aCamera;        // 可空；非空则相机跟随角色
    float aWalkSpeed = 3.5f;
};

class cPLAYER_CONTROLLER {
  public:
    void FixedUpdate(float fixedDt); // 读 aInput → wishDir → aCharacter->Move → Jump
    void Update(float dt, float alpha); // 相机跟随（用插值后的位置）
};
```

四个引擎侧控制器挂在 `cENTITY` 上，但**可以独立使用**（比如只有物理的箱子、
只有动画的展示台）。游戏侧（`cINPUT_MAP` / `cGAME_CLOCK` / `cCHARACTER` /
`cPLAYER_CONTROLLER`）不依赖 `cENTITY`，单独拿去做 UI 或展示场景也能用。

### 4.1 调用者视角：一帧到底写什么

目标是把一个游戏 demo 的主循环压到这么短：

```cpp
// AppRun / 固定段
clock.Tick(realDt);
input.Update(realDt);
for (uint32_t i = 0; i < clock.FixedSteps(); ++i) {
    player.FixedUpdate(clock.FixedStepSize());
    physics.Step(clock.FixedStepSize());
    scene.FixedUpdate(clock.FixedStepSize());   // 动画状态机 / AI
}
// 每帧段
player.Update(realDt, clock.Alpha());
scene.Update(realDt, input);
// 渲染段
frame.Begin({vp, eye, light, aspect, nearZ, farZ});
frame.ShadowPass(scene.ShadowCasters());
frame.GeometryPass(scene.Visible(cameraFrustum));
frame.End();
```

对比现状：mmdViewer 的 `AppDisplay2D` 66 行 + 全仓 tests 里 42 处直读全局输入
+ 手写 CSM 拟合。

## 5. 分阶段实施

每阶段独立可验收、可回滚，且**不阻塞渲染管线既有验收**（gpuCheck 109 项必须始终全绿）。

阶段整体顺序：**P0（游戏底层地基）→ P1（骨架）→ P2/P4 并行 → P3 → P6 → P5**。

### 5.1 前置约束：测试资产必须程序生成

仓库里没有模型文件（§1 第 5 条），所以 P2/P3 的验收**不能依赖外部资产**。
现有可用的两条程序化路径：

| 路径 | 现成基础 | 用途 |
|------|---------|------|
| 生成 PMX | `tests/unit/testMmdWriter.h`（单测已在用） | 可控关节数/层级的骨架，验证 128 关节分流 |
| 内嵌 glTF | `sASSET::loadFromMemory`（`testTexture.cpp` 已有最小 glTF 用例） | 带 skin 的完整资产，验证 palette 链路 |

因此 P1 要顺带做一个 **`tests/fixtures/` 生成器**（C++，构建时生成到 `.local/fixtures/`）：
一根 3 节骨骼 + 一个 8 顶点立方体蒙皮。用例和截图都跑它。

### P0 — 游戏底层控制地基（2–3 天，**建议最先做**）— ✅ 已完成（2026-11-24）

性价比最高的一段：不依赖任何渲染改动，立刻能减少两个 demo 里的重复代码。

> 落地情况：`GN::cINPUT_MAP`（`src/input/GN_InputMap.h/.cpp`）、`GN::cGAME_CLOCK`
> （`src/core/GN_GameClock.h/.cpp`）、两张绑定表（`tests/*/*Actions.cpp`）已进仓库；
> `make unit` 527/0（新增 32 例）、`make lint` 的 `tests/lint/input.sh` 实测示例内
> 全局输入数组直读 **0 处**（迁移前 42 处）。详见 `docs/CHANGELOG.md` 2026-11-24 条目。
> 差异说明：验收要求的"mmdViewer 按键逐项对测"在无 GPU/无模型环境下落到**绑定表级**
> （每个键 → 该动作成立，`testDemoBindings.cpp` 覆盖全表），真机手动点按待有显示环境补跑。

- `cINPUT_MAP`：动作绑定 + 逐帧边沿检测 + `CameraInput()`。
- `cGAME_CLOCK`：固定步长累加器 + `Alpha()` + `timeScale`。
- mmdViewer / imoScene 全部 `Key1[GK_*]` 改走动作映射（**行为不变**）。
- **验收**：`tests/unit/testInputMap.cpp`：多键绑同一动作、边沿只在首帧为真、
  `Unbind` 后失效、`Axis1D` 对向抵消、未绑定动作恒 false、`Update(dt)` 为 0/负/NaN 不崩。
  `tests/unit/testGameClock.cpp`：60Hz 物理 + 144Hz 渲染的累加器余量、`maxSubSteps` 限幅（
  防止掉帧后一次性跑几百步）、`timeScale=0` 冻结、`Alpha()` 单调且∈[0,1)。
- **验收**：mmdViewer 所有按键功能逐项对测（用脚本驱动 `GN::Key1` 模拟，而不是人工点），
  确保替换后行为一致。

这一阶段完成后，“以后做游戏”就已经能直接拿到输入与时间两样东西。

### P1 — 骨架与回归基线（1–2 天）— ✅ 已完成（2026-11-24）

> 落地情况：`GN3d::cENTITY` + 四控制器（`src/graphics/GN_Entity.h/.cpp`）、
> `GNg::cRENDER_FRAME`（`_Vulkan/vkRenderFrame.*`）、`GN3d::sSHADOW_STATE`/`fitShadowState`、
> `tests/fixtures/` 程序化资产（`make fixtures`）都已进仓库；mmdViewer 走 `gEntity` +
> `cRENDER_FRAME`，imoScene 的 CSM 走 `sSHADOW_STATE`。
> 验收：`make unit` 544/0、`gpuCheck` 109/109、`smoke` 5/5，并且
> `make headlessCheck` 新增的 `frame.parity` 把同一帧用新/旧两条路径各渲一次，
> **逐字节相同**（diffBytes=0/1024000，同时断言非空画面）。
> 差异说明：真机 + 真实模型下的逐像素对比仍需外部资产环境，本轮以"同进程双路径比对"替代。

- 新增 `GN_Entity.{h,cpp}`：`cENTITY` + 四个控制器空壳（只转发到现有算法层）。
- 新增 `tests/fixtures/` 程序化骨架/蒙皮资产生成器。
- `mmdViewer` 改用 `cENTITY`（**行为不变**，仍静态绘制、不含骨骼）。
- `cRENDER_FRAME` 抽出编排，`updateShadowCascades` 从 demo 迁进引擎。
- **验收**：mmdViewer 在**有模型**的环境下画面与改造前逐像素一致（`MMD_CAPTURE=1`
  自动截图 + 与基线 PNG 比 RMSE=0）；本容器无模型，此条降级为“`make ci` 109/109 +
  无模型路径不崩”（必须在有模型的机器上补跑像素对比）。

### P2 — 骨骼控制器跑通 GPU 蒙皮（3–4 天，最大不确定项）— ✅ 已完成（2026-11-24）

> 落地情况：`selectSkinPalettePath`（UBO≤128 / SSBO≤4096 / 超限拒绝）+
> `cSKELETON_CONTROLLER::PalettePath()` + `GNg::GN_VkDrawSkinnedAuto`（`_Vulkan/vkSkinnedDispatch.*`）
> 已接通，`computePalette → 上传 → Draw3DSkinned/Ssbo` 第一次在运行时跑通。
> 验收：`make gpuCheck` **115/0**（新增 6 站），其中两条轮廓站用 fixtures 骨架渲一遍，
> 与 CPU `skinVertices` 投影轮廓逐采样点比对（7.5 万点、0 mismatch、3 px 死区）。
> **与本文验收口径的差异（重要）**：原文要求"蒙皮后顶点位置与 CPU 差 ≤1e-4"，
> 但蒙皮顶点输出没有 transform feedback/计算回写，唯一可观测的是像素，1e-4 世界单位
> 远小于 1 像素、物理上不可分辨；故改为"轮廓逐采样点一致 + 3 px 死区"，并补一条
> "绑定位姿 vs 转 60° 轮廓至少差 100 个采样点"的反向保证，防止两边一样错。
> 仍未做：真实 MMD/GLB 模型的画面确认（无资产）。

- `computePalette → packPalette → 上传 → Draw3DSkinned` 首次在运行时接通。
- 关节数分流：≤128 走 UBO 调色板，>128 走 SSBO（`vkSkinSsbo` 已有）。
- **验收**：新增 `tests/gpu` 站点 `entity.skinning`——用 fixtures 骨架，给定关节旋转，
  断言蒙皮后顶点位置与 CPU `skinVertices` 的差 ≤1e-4（已有 CPU 参考实现，
  `gpuCheckSkin` 已在对拍）。另断言 129 关节模型自动走 SSBO 且结果与 CPU 一致。
- **不做**：MMD 真实模型的效果确认（无资产，需外部提供）。

### P3 — 动画状态机（2 天）— ✅ 已完成（2026-11-24）

> 落地情况：`GN3d::cANIM_STATE_MACHINE`（`GN_AnimStateMachine.*`）+ `sANIM_PARAMS`，
> 已接进 `cANIM_CONTROLLER`（clip 表只有一份）与 `cENTITY::Update`（状态机优先于手工播放）。
> 验收：`tests/unit/testAnimStateMachine.cpp` 15 例（转换规则 / crossFade 权重 / 缺通道降级 /
> 非法状态名 / dt 边界 / 事件跨越 / 级联上限 / 并列优先级）+ GPU 站 `entity.animSwitch`
> （状态切换后覆盖像素变化 71.5%）。`make unit` 560/0、`make gpuCheck` 116/0。
> 顺带修：`cPHYSICS_WORLD::AddBody` 不保证新动态刚体醒着（脏 `aSleeping` ⇒ 永不积分）。
> 未做：混合树（blend space）、动画层遮罩、事件参数载荷、真实 MMD/glTF 动画资产。

- clip 表 + 参数→状态转换 + 事件。
- **验收**：`tests/unit/testAnimStateMachine.cpp`：转换规则、crossFade 期间权重、
  缺通道降级、非法状态名、dt 为 0/负/超大。
- **验收**：用 fixtures 造两段 clip（旋转 0° / 90°），`entity.animSwitch` 断言切换后
  关节世界矩阵朝预期方向变化 ≥ 阈值。

### P4 — 物理控制器 + 变换同步（3 天）— ✅ 已完成（2026-11-24）

> 落地情况：`cPHYSICS_CONTROLLER` 的快照插值（`Step(dt, alpha)` + `ApplyTransformInterpolated`）
> 与接触事件（实体对 / enter / exit / 睡着 latch）已进仓库；引擎侧补了
> `cPHYSICS_WORLD::GetContactInfo` 与 `GetLastSubStepCount`；新增场景验收
> `tests/physicsBoxes`（20 箱堆叠 + 球撞倒，**纯 CPU，不需要 GPU**，已并入 `make check`）。
> 验收：`make unit` 574/0、`make physicsBoxes` 6/6、`make gpuCheck` 116/0、`make smoke` 6/6，`make ci` 全绿。
> **与本文验收口径的差异**：原文要求"箱子最终 Y 单调下降"，实际做不到——求解器位置修正
> 会把箱子往上顶（本场景实测单帧最大上抬 2.6 mm）。改为「净位移向下 + 单帧上抬 ≤5 cm」。
> 另：CCD v1 只扫掠静态/运动学目标，动态-动态高能撞击（8 m/s × 2 kg）会让箱子插进地面 28 cm；
> 场景参数按实测取 3.5 m/s × 1 kg（穿透 ≤1 cm）。

- `entityId ↔ bodyHandle`、`Step`、快照插值、接触事件。
- **验收**：`tests/unit/testPhysicsSync.cpp`：固定步长下 60Hz 物理 + 144Hz 渲染的插值
  单调性；body 睡醒/移除后映射表不残留。
- **验收**：新示例 `tests/physicsBoxes`（20 个箱子落地堆叠 + 一个球撞倒），
  headless 跑 120 帧断言"箱子最终 Y 单调下降且不穿地"。

### P5 — 角色控制器 `cCHARACTER`（3–4 天）— ✅ 已完成（2026-11-24）

> 落地情况：`GN3d::cCHARACTER`（`GN_Character.*`）已进仓库并接进 `cENTITY::aCharacter`；
> 引擎侧补了公开窄相 `querySphereTriangle` / `queryCapsuleTriangle`（与刚体三角网接触
> 共用同一份数学）；新增场景验收 `tests/characterDemo`（纯 CPU，并入 `make check`）。
> 验收：`make unit` 591/0（新增 17 例）、`make characterDemo` 7/7、`make gpuCheck` 116/0、
> `make smoke` 7/7，`make ci` 全绿。计划里的六条判据全部有对应用例。
> **与本文验收口径的差异**：① 45° 坡上的"坡度读数"不是恒 45°（竖胶囊在坡面上是侧下方
> 半球接触，读数约 35°），用例改为"明显是斜面 + 脚底间隙 < 0.2 m"；
> ② 台阶抬升多了一条"落点必须站得住（坡度 ≤ maxSlope）"，否则 60° 坡会被一帧一帧蹭上去。
> 未做：角色推挤动态刚体、布娃娃、IK、root motion、相机跟随接线。

没有它做不了任何第三人称/第一人称游戏。

- 胶囊扫掠 vs 静态三角网（复用 `cPHYSICS_WORLD::Raycast` + `AddStaticMesh`），
  解穿透 → 斜坡投影 → 台阶抬升 → 地面检测。
- `cPLAYER_CONTROLLER`：`input → wishDir → Move → Jump`，相机跟随接 `cCAMERA::SetTarget`（已有）。
- **验收**：`tests/unit/testCharacter.cpp`（纯 CPU，不需 GPU）：
  平地行走速度 = `aWalkSpeed`、45° 斜坡可上而 60° 被阻、`aStepHeight` 台阶能过而更高不能、
  跳跃高度 ≈ `v²/2g`、贴墙下滑不卡死、`Teleport` 后不残留旧速度。
- **验收**：新示例 `tests/characterDemo`：一个胶囊在带台阶与斜坡的测试场地上跑，
  headless 跑 240 帧断言“始终未穿地、最终能到达终点平面”。

### P6 — 统一剔除 / LOD / 排序（2 天）— ✅ 已完成（2026-11-24）

> 落地情况：`GN3d::cSCENE`（`GN_Scene.*`）一次性完成剔除 → LOD → 排序；LOD 策略固化为
> `sLOD_POLICY`（像素误差 + 屏幕占比，且只会更精细）；`cMESH_CONTROLLER` 接策略并自动判定
> 透明材质。验收：`tests/unit/testScene.cpp` 7 例（含 **1000 实体剔除 vs 暴力判定**、
> 1000 实体稳定排序）、`make unit` 599/0、`gpuCheck` 116/0（`cull.*`/`lod.*` 全绿）、
> `smoke` 7/7、`ci` 全绿。
> **顺带修掉一个静默失效级缺陷**：`CreateCubeModel` 的 `aVertexCount` 写成 36（实际只填了
> 24 个顶点），`computeWorldAABB` 把 12 个未初始化顶点算进包围盒，得到 ±1e15 量级的 AABB ⇒
> 剔除/排序/LOD/阴影拟合全部失效。已修 + 加对拍用例。
> 未做：GPU 侧剔除（compute culling）、遮挡剔除/阴影剔除、跨帧持久化排序索引。

- 剔除从"调用方传数组"变成控制器内部；LOD 选级固化（屏幕占比 + 误差）。
- **验收**：`gpuCheck` 的 `cull.*`/`lod.*` 保持全绿；新增断言"1000 个实体的剔除结果
  与逐实体暴力判定一致"。

总计约 **16–20 天**（P0 2–3 + P1 1–2 + P2 3–4 + P3 2 + P4 3 + P5 3–4 + P6 2），
其中 P2 风险最高（GPU 蒙皮从未运行时验证过）。

依赖关系（编号即建议执行顺序）：

```
P0 (输入+时钟) ──┬─→ P1 (骨架) ─┬─→ P2 (GPU 蒙皮) ─→ P3 (动画状态机)
                 │                └─→ P4 (物理同步) ──（与 P2 可并行）
                 └─→ P5 (角色控制) ─→ P6 (剔除/LOD)
```

**如果只能做两件事**：P0 + P6。它们直接决定“能不能开始写游戏”；
P1–P5 是把画面表现与角色表现做完整。

## 6. 风险与边界

| 风险 | 说明 | 对策 |
|------|------|------|
| **无模型资产** | 仓库不含 PMX/GLB，本容器亦无 | P1 先做 fixtures 生成器（§5.1）；真实模型效果留待外部资产环境 |
| **GPU 蒙皮从未跑通** | 单测只验 CPU 侧；`vkSkinDraw` 只在 gpuCheck 里建过管线没真画蒙皮网格 | P2 单独成阶段，先跑最小立方体骨架再上更复杂形状 |
| **128 关节上限** | MMD 模型常见 200+ 关节 | 自动分流 SSBO；超 SSBO 上限再截断并告警（用 fixtures 造 129 关节验分流） |
| **改 demo 会动已验证画面** | mmdViewer 当前画面是人工确认过的 | P1 必须做到逐像素不变（先接线不改行为），再逐步开新功能 |
| **物理/渲染帧率不匹配抖动** | 固定步长 60Hz vs 渲染 144Hz | 快照插值（P4），不接受"看起来还行" |
| 布娃娃 / 骨骼↔刚体耦合 | 骨骼与刚体的双向约束 | **明确不在 v1**；`cPHYSICS_CONTROLLER` 只做刚体→网格单向同步，`cCHARACTER` 不加刚体 |
| **新层做出来没人用** | 只有两个 demo 就能抽出多层控制器，有过设计风险 | P0 先做（立刻替掉 42 处键码 + 统一 dt），用真实痛点验证；每阶段结束后重估某层是否只被单一调用方使用，是则合并 |
| 角色与刚体双重位移 | `cCHARACTER` 与刚体两套运动共存时会互相踩 | `cCHARACTER` 不向世界加刚体，只消费静态三角网；角色推挤动态刚体 v1 不做 |

## 7. 不做的事

- 不重写 `sSKELETON` / `cANIM_PLAYER` / `cPHYSICS_WORLD` 的求解逻辑（已验）。
- 不引入 ECS（两个 demo 撑不起，且现有 `cMODEL` 是面向对象的），`cSCENE` 只存指针数组。
- 不做序列化/关卡编辑器（做游戏早晚要，但不属底层控制）。
- 不引入输入上下文栈（UI 拦截用 `bool enabled` 先过）。
- 不做布娃娃、IK、动画压缩、网络同步。
- 不做角色与动态刚体的推挤（只做角色 vs 静态三角网）。
- 不做手柄/触屏映射：现有 `GK_*` 键码 + 触屏到鼠标的映射已存在，`cINPUT_MAP` 留了扩展位，
  但 v1 只接键盘 + 鼠标。
- 不动 Vulkan 层 pass 实现，只在上面加 `cRENDER_FRAME` 这层薄编排。
