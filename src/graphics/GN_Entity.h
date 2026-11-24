#pragma once
#ifndef _GN_ENTITY_H_
#define _GN_ENTITY_H_
// ╔══════════════════════════════════════════════════════════╗
// ║  GN_mini — 实体与四套控制器（P1：接线，不含新算法）      ║
// ╚══════════════════════════════════════════════════════════╝
//
// 契约（docs/ENTITY_CONTROLLERS_PLAN.md §2）：
//   * 四个控制器**不是平级对象**，而是一条主链 + 一个旁支：
//         物理 ──(world transform)──┐
//                                   ├──→ 网格 ──→ 绘制
//         动画 ──(local pose)──→ 骨骼 (palette)
//     固定更新顺序 **物理 → 动画 → 骨骼 → 网格**：根位移先定，姿势再算，调色板再算，
//     LOD/排序最后（它需要本帧相机距离）。
//   * P1 阶段全部是**转发壳**：算法（sSKELETON::computePalette / cANIM_PLAYER / 
//     cPHYSICS_WORLD::Step / sMESH 的绘制）早已实现并有单测，这里只做接线与状态持有，
//     不重写任何求解。GPU 蒙皮（P2）、动画状态机（P3）、物理插值（P4）留后续阶段。
//   * 所有权：控制器**不拥有** cMODEL / sSKELETON / sANIM_CLIP / cPHYSICS_WORLD，
//     只持非拥有指针。cENTITY 可析构时不做释放，避免双重 free。
//   * 本层不碰任何 Vulkan 调用（cMODEL::Draw* 除外，那条依赖是既有白名单），
//     因此可以纯 CPU 单测（tests/unit/testEntity.cpp）。
#include <GN.h> // 先定义 GN_API / GN_USING_3D 等模块宏（同 GN_ShadowCascade.h 的写法）
#include "GN_3Dmath.h"
#include "GN_3Dmodel.h"
#include "GN_3Danim.h"
#include "GN_3Dskeleton.h"
#include "GN_3DSkin.h"
#include "GN_3Dlod.h"
#include "GN_3Dcamera.h" // C1：sCAMERA_TARGET（BuildCameraTarget 的返回类型）
#include "GN_Physics.h"
#include "GN_Spatial.h"
#include "GN_Character.h" // P5：cENTITY 按值持有 cCHARACTER

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace GN3d {

class cANIM_STATE_MACHINE; // 定义在 GN_AnimStateMachine.h（本头只按指针持有，见下）

// ═══════════════════════════════════════════════════════════
//  cMESH_CONTROLLER — 网格：LOD 选级 / 视锥剔除 / 透明排序 / 绘制
// ═══════════════════════════════════════════════════════════
// 现状差距（P1 解决）：LOD 选级此前是调用方自己算 levels[] 再传给 DrawPBRWithLod；
// 现在「距离+误差→级别」的上一帧结果由控制器持有（selectLod 需要 prevLevel 防抖）。
class cMESH_CONTROLLER {
  public:
    cMESH_CONTROLLER();

    void SetModel(cMODEL *model); // 非拥有；模型换掉会重置 LOD 状态
    cMODEL *Model() const;

    // LOD 阈值（屏幕像素误差）。<= 0 表示用引擎默认（1.0 px）。
    void SetMaxScreenError(float pixels);
    float MaxScreenError() const;

    void SetCullEnabled(bool enabled); // 默认 true
    bool CullEnabled() const;

    // LOD 策略（P6）：像素误差 + 屏幕占比，见 GN_3Dlod.h 的 sLOD_POLICY。
    void SetLodPolicy(const sLOD_POLICY &policy);
    const sLOD_POLICY &LodPolicy() const;

    // 每帧一次（在绘制之前）：刷新世界 AABB → 视锥剔除 → 逐网格选级。
    // frustum 为空 = 不剔除。返回本帧是否可见（不可见时调用方跳过绘制）。
    bool Prepare(const sLOD_VIEW &view, const sFRUSTUM *frustum);

    // 模型里是否含有 BLEND 材质（场景级排序要把这类实体排到不透明之后）。
    // 判定口径与 cMODEL::DrawPBRSorted 一致：材质 aAlphaMode == eALPHA_BLEND。
    bool HasTransparentMaterial() const;
    // 覆盖自动判定（例如角色披风被强制标为透明，而材质本身是 MASK）。
    void SetTransparentOverride(bool enabled, bool transparent);

    bool Visible() const;
    uint32_t LodLevel(uint32_t meshIndex) const; // 上一帧选级；越界返回 0
    uint32_t LodCount() const; // 已算出的级数（= 网格数）

    // 绘制：透明排序开关由 SetSortTransparent 控制（默认 false，走 DrawPBRWithLod）。
    // v1 排序与 LOD 互斥：DrawPBRSorted 不接受 levels，二选一由调用方声明。
    void SetSortTransparent(bool enabled);
    bool SortTransparent() const;
    void Draw(const GNg::sVEC3 &cameraPos) const;
    void DrawShadow() const;

  private:
    cMODEL *aModel;
    sLOD_POLICY aPolicy; // P6：像素误差 + 屏幕占比
    float aMaxScreenErrorPx;
    bool aCull;
    bool aSortTransparent;
    bool aTransparentOverride;
    bool aTransparentValue;
    bool aVisible;
    std::vector<uint32_t> aLevels; // 本帧每网格的级别（绘制用）
    std::vector<uint32_t> aLevelsPrev; // 上一帧的级别（selectLod 的降档防抖输入）
};

// ═══════════════════════════════════════════════════════════
//  cANIM_CONTROLLER — 动画：clip 表 + 播放器 + 状态机（P3）
// ═══════════════════════════════════════════════════════════
// 两个层次，别混：
//   * **手工播放**（Play/CrossFade）：调用方自己决定播哪段，用 clip 名；
//   * **状态驱动**（StateMachine）：给一组参数，由转换表决定播哪段。
// 两者共用**同一个** cANIM_PLAYER 与**同一张** clip 表 —— 各留一份时间轴是这类系统
// 最难查的 bug，这里从结构上堵死。混用时后调用者生效（手工 Play 会打断状态机当前状态，
// 状态机下一次 Update 会按转换表重新接管）。
class cANIM_CONTROLLER {
  public:
    cANIM_CONTROLLER();
    ~cANIM_CONTROLLER(); // 定义在 .cpp：这里只有一个不完整类型的 unique_ptr

    // 注册 clip（非拥有；同名覆盖）。名字为空或 clip 为空返回 false。
    // 同时进状态机的 clip 表（只有一份）。
    bool RegisterClip(const char *name, const sANIM_CLIP *clip);
    const sANIM_CLIP *FindClip(const char *name) const;
    uint32_t ClipCount() const;
    const char *ClipName(uint32_t index) const; // 越界返回 ""

    // ── 手工播放 ──
    bool Play(const char *name, bool loop = true, float speed = 1.0f);
    bool CrossFade(const char *name, float duration, bool loop = true);
    void Stop();
    void Update(float dt); // 时间轴驱动（dt <= 0 / NaN 不推进；状态机不参与）

    // ── 状态驱动 ──
    cANIM_STATE_MACHINE &StateMachine(); // 建表（AddState/AddTransition/AddEvent）
    const cANIM_STATE_MACHINE &StateMachine() const;
    // 进初始状态并接管播放器。没有初始状态 / clip 缺失返回 false。
    bool StartStateMachine();
    // 每帧一次：参数 → 转换 → 驱动播放器 → 事件。必须先 StartStateMachine。
    bool UpdateWithStateMachine(float dt, sANIM_PARAMS &params);
    bool StateMachineActive() const;

    bool Playing() const;
    bool Sample(uint32_t node, sANIM_TRANSFORM &out) const;
    const char *Current() const; // 手工播放的 clip 名，或状态机的 clip 名
    float Time() const;
    float BlendWeight() const;

  private:
    struct sCLIP_ENTRY {
        std::string aName;
        const sANIM_CLIP *aClip;
    };
    const sCLIP_ENTRY *find(const char *name) const;
    std::vector<sCLIP_ENTRY> aClips;
    cANIM_PLAYER aPlayer;
    // 为什么按指针持有：GN_Entity.h 不能包含 GN_AnimStateMachine.h
    // （那个头包含 <GN.h>，而 <GN.h> 又包含本文件 → 循环）。前向声明 + 指针是最小代价。
    std::unique_ptr<cANIM_STATE_MACHINE> aMachine; // clip 表也在这里（RegisterClip 转发）
    bool aMachineActive;
    std::string aCurrent;
};

// ═══════════════════════════════════════════════════════════
//  cSKELETON_CONTROLLER — 骨骼：世界矩阵 / 调色板 / UBO 打包
// ═══════════════════════════════════════════════════════════
// 现状差距（P1 解决）：computePalette → packPalette 这条链此前只在单测里逐段跑过，
// 没有任何「持有骨架 + 缓存调色板」的对象。GPU 上传（P2）不在这里。
class cSKELETON_CONTROLLER {
  public:
    cSKELETON_CONTROLLER();

    void SetSkeleton(const sSKELETON *skeleton); // 非拥有；换骨架会清空缓存
    const sSKELETON *Skeleton() const;
    uint32_t JointCount() const;

    // 绑定姿势的局部变换（来自 sJOINT 的 aBindTranslation/aBindRotation/aBindScale）。
    // 用途：clip 只覆盖部分关节时，未被覆盖的关节应当保持绑定姿势，而不是被写成
    // 零平移/零缩放的四元数（那会把模型压成一个点，比"不动"难查得多）。
    std::vector<sANIM_TRANSFORM> BindPoseLocal() const;

    // 输入本帧局部姿势（长度必须 == JointCount()），算 world 与 palette。
    // 失败（长度不符 / 未设骨架）返回 false 且不清掉上一次成功的缓存。
    bool ComputePalette(const std::vector<sANIM_TRANSFORM> &local);

    bool PaletteValid() const;
    const std::vector<GNg::sMAT44> &Palette() const;
    const std::vector<GNg::sMAT44> &World() const; // 调试/挂点用（P2 起逐步有人消费）
    // 关节世界位置（世界矩阵的平移列）；越界返回原点。
    GNg::sVEC3 JointWorldPosition(uint32_t joint) const;

    // 打包到 GPU 关节 UBO 布局（128 上限截断，未用槽清零）。返回写入矩阵数。
    uint32_t PackPalette(sSKIN_UBO &out) const;

    // 该骨架该走哪条 GPU 调色板路径（UBO ≤128 / SSBO ≤4096 / NONE）。
    // 纯 CPU 判断（selectSkinPalettePath），所以没有 GPU 也能单测；实际绘制入口在
    // _Vulkan/vkSkinnedDispatch.h（GN_VkDrawSkinnedAuto）。
    eSKIN_PALETTE_PATH PalettePath() const;

  private:
    const sSKELETON *aSkeleton;
    std::vector<GNg::sMAT44> aWorld;
    std::vector<GNg::sMAT44> aPalette;
    bool aValid;
};

// ═══════════════════════════════════════════════════════════
//  cPHYSICS_CONTROLLER — 物理：step + 映射 + 快照插值 + 接触事件（P4）
// ═══════════════════════════════════════════════════════════
// 三件事：
//   1. **映射**：entityId ↔ bodyHandle（P1 已有）；
//   2. **快照插值**：物理走固定 60Hz，渲染可能 144Hz。直接读最新刚体位置会看到
//      "每两帧才动一次"的台阶；这里在 Step 前后各存一份快照，按 alpha 插值。
//   3. **接触事件**：把 `cPHYSICS_WORLD` 的接触明细翻译成"实体对"（带 enter/exit），
//      调用方不需要知道 bodyHandle。
//
// 插值的口径（重要，别想当然；这三条都踩过）：
//   * 快照**只在真的跑了子步时才前移**。否则"本帧没跑物理"会把 prev 也覆盖成 curr，
//     两者相等 ⇒ 位置两帧才动一次，插值白做（第一版就是这样，144Hz 下 84 帧不动）。
//   * prev/curr 是"最近一次子步前/后"的状态，alpha = 余量 / 单步。于是渲染时刻随余量
//     从 prev 平滑走到 curr；下一次子步时 prev 正好接上 curr，画面连续。
//   * **不要**把 alpha 除以子步数。一帧跑 N 步时按 N 倍速走完这一段是连续的；
//     除以 N 会停在 t+N·s/N，下一帧又跳到 t+N·s —— 那是真正的跳帧。
class cPHYSICS_CONTROLLER {
  public:
    // 一个刚体的渲染快照。
    struct sSNAPSHOT {
        GNg::sVEC3 aPos;
        GNg::sQUAT aRot;
        bool aValid;
        sSNAPSHOT();
    };

    // 一条"实体对"接触事件（entityId 0 = 该刚体没有登记实体，例如地面/三角网）。
    struct sCONTACT_EVENT {
        uint32_t aEntityA;
        uint32_t aEntityB;
        GNg::sVEC3 aNormal; // A → B
        GNg::sVEC3 aPoint;
        float aPenetration;
        bool aEntered; // 本帧新出现（上一帧这一对不存在）
        sCONTACT_EVENT();
    };

    cPHYSICS_CONTROLLER();

    void SetWorld(cPHYSICS_WORLD *world); // 非拥有
    cPHYSICS_WORLD *World() const;

    bool AttachBody(uint32_t entityId, uint32_t handle); // 同一 entity 重复登记会覆盖
    bool DetachBody(uint32_t entityId);
    void ClearBindings();
    uint32_t BodyOf(uint32_t entityId) const; // 无映射返回 cPHYSICS_WORLD::INVALID_HANDLE
    int32_t EntityOf(uint32_t handle) const; // 无映射返回 -1
    uint32_t BindingCount() const;

    // 推进一帧：记下子步前状态 → cPHYSICS_WORLD::Step(dt) → 若真的跑了子步则把
    // (前, 后) 提交成 (prev, curr)，然后刷新接触事件。
    // renderAlpha 由调用方给（cGAME_CLOCK::Alpha()），只钳到 [0,1]，不做其它折算。
    void Step(float dt, float renderAlpha = 0.0f);

    // 渲染用的插值因子（已含子步折算），恒在 [0,1]。
    float RenderAlpha() const;
    // 上一帧实际子步数（0 = 本帧不足一步，位置未变）。
    uint32_t LastSubSteps() const;

    // 刚体 → 模型（最新状态，不插值）。无时钟/物理调试/探针用。
    bool ApplyTransform(uint32_t entityId, cMODEL &model) const;
    // 刚体 → 模型（按 RenderAlpha() 在 prev/curr 间插值）。渲染用，比最新状态晚不到一步。
    bool ApplyTransformInterpolated(uint32_t entityId, cMODEL &model) const;

    // ── 接触事件（每帧 Step 后刷新）──
    uint32_t ContactCount() const;
    const sCONTACT_EVENT &ContactAt(uint32_t index) const; // 越界返回一条零值（aEntityA/B = 0）
    uint32_t EnteredContactCount() const;
    // 上一帧有、本帧没有的实体对（"离开"事件）。只在 Step 后的一帧内有效。
    uint32_t ExitedContactCount() const;
    bool ExitedContactAt(uint32_t index, uint32_t &entityA, uint32_t &entityB) const;
    // 是否上报"没有实体"的接触（默认 false：地面/静态三角网不刷屏）。
    void SetReportUnmapped(bool report);
    bool ReportUnmapped() const;

  private:
    struct sPAIR {
        uint32_t aEntityA;
        uint32_t aEntityB;
    };
    sSNAPSHOT captureBody(uint32_t handle) const;
    void refreshContacts();
    bool pairIsLatched(uint32_t entityA, uint32_t entityB) const;
    static uint64_t pairKey(uint32_t entityA, uint32_t entityB);

    cPHYSICS_WORLD *aWorld;
    std::vector<uint32_t> aEntityIds;
    std::vector<uint32_t> aHandles;
    std::vector<sSNAPSHOT> aPrev;
    std::vector<sSNAPSHOT> aCurr;
    float aRenderAlpha;
    uint32_t aLastSubSteps;
    bool aReportUnmapped;
    std::vector<sCONTACT_EVENT> aContacts;
    std::vector<uint64_t> aPrevPairKeys; // 上一帧的实体对（算 enter/exit）
    std::vector<sPAIR> aExited;
};

// ═══════════════════════════════════════════════════════════
//  cENTITY — 四套控制器的宿主
// ═══════════════════════════════════════════════════════════
class cENTITY {
  public:
    cENTITY();

    uint32_t aId; // 场景内编号（0 = 未分配；由场景/调用方填）
    cMESH_CONTROLLER aMesh;
    cANIM_CONTROLLER aAnim;
    cSKELETON_CONTROLLER aSkeleton;
    cPHYSICS_CONTROLLER aPhysics;
    cCHARACTER aCharacter; // P5：胶囊角色（不参与刚体求解，只消费静态三角网）

    // 局部姿势缓存（动画 → 骨骼之间的那条边）。长度 == 关节数时才有效。
    // 调用方可以直接往里写（例如程序化姿势），也可以 SetLocalPose()。
    void SetLocalPose(const std::vector<sANIM_TRANSFORM> &local);
    const std::vector<sANIM_TRANSFORM> &LocalPose() const;
    bool bHasLocalPose; // SetLocalPose 成功过 / 动画采样填过

    // 状态机参数包（P3）。每帧填好再 Update；Update 末尾触发器会被清空。
    sANIM_PARAMS aParams;

    // 渲染插值因子（P4）：调用方每帧填 cGAME_CLOCK::Alpha()，物理→模型同步用它。
    float aRenderAlpha;

    // 固定顺序：物理 → 动画 → 骨骼（plan §2）。网格部分见 PrepareRender。
    // **注意**：本函数会调 `aPhysics.Step(dt)`，而它推进的是**整个 cPHYSICS_WORLD**。
    // 所以"N 个实体共享同一个世界"时不能对每个实体都调它（那会让物理跑 N 倍速）；
    // 多实体场景请用 `cSCENE::SetPhysicsController(共享控制器)`，由场景统一推进一次。
    void Update(float dt);

    // 只做"动画 → 骨骼"，不推进物理、不同步刚体变换。
    // 用途：物理由场景/上层统一推进之后，逐实体刷姿势。
    void UpdateAnimation(float dt);

    // ── 相机跟随（C1）──
    // 把本实体打包成相机目标，交给 `cCAMERA::SetTarget()`。
    //
    // 为什么要这个 helper：`SetTarget` 要的是 `sCAMERA_TARGET{位置, AABB}`，
    // 而"实体的位置"有四种可能来源（网格模型、角色胶囊、物理刚体、动画骨骼），
    // 每个调用点都自己挑一个的话，相机跟随就会在"角色走了但相机不动"这类地方出 bug。
    // 这里的优先级是**眼睛高度更合理的那一个**：
    //   1. `aCharacter` 已初始化（胶囊高度 > 0）⇒ 用胶囊中心 + heightOffset；
    //   2. 否则有网格模型 ⇒ 用模型的世界 AABB 中心；
    //   3. 都没有 ⇒ 返回 false（不要返回一个"看起来像原点"的假目标）。
    // heightOffset 是**在选定中心之上**再加的偏移（第三人称相机通常要看向头部而非腰部）。
    // bounds 只在有网格时有效（空盒 = 相机只用位置，不做自动取景）。
    bool BuildCameraTarget(sCAMERA_TARGET *out, float heightOffset = 0.0f) const;

    // 渲染前：刷新世界 AABB + 选级 + 剔除。返回本帧是否可见。
    bool PrepareRender(const sLOD_VIEW &view, const sFRUSTUM *frustum);
    bool Visible() const;
    void Draw(const GNg::sVEC3 &cameraPos) const;
    void DrawShadow() const;

  private:
    std::vector<sANIM_TRANSFORM> aLocalPose; // 动画 → 骨骼之间的那条边
};

} // namespace GN3d
#endif // _GN_ENTITY_H_
