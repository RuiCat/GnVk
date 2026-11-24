#pragma once
#ifndef _GN_CHARACTER_H_
#define _GN_CHARACTER_H_
// ╔══════════════════════════════════════════════════════════╗
// ║  GN_mini — 角色控制器（P5：胶囊移动，纯 CPU，无刚体）     ║
// ╚══════════════════════════════════════════════════════════╝
//
// 为什么需要它：`cPHYSICS_WORLD` 只有刚体。做任何第三人称/第一人称游戏，第一件撞上的事
// 就是"人要能走、能上台阶、不能卡在斜坡上、跳起来要落在该落的地方"。这些都不能靠
// 给角色加一个刚体解决（刚体会打滑、会被别的刚体推、斜坡上会自己滑下去）。
//
// 设计（与刚体完全分开）：
//   * 角色**不向物理世界加入刚体**，只消费**静态三角网**。它自己积分、自己解穿透。
//     这样角色与动态刚体不会互相踩（角色不推箱子、箱子也不推角色 —— v1 边界）。
//   * 几何来源：`AddStaticMesh(const sMESH&)`（与 cPHYSICS_WORLD::AddStaticMesh 同一个
//     顶点布局，11 floats/顶点）或直接 `AddTriangle`。窄相复用引擎已有的
//     `queryCapsuleTriangle`（同一份三角形数学，不存在第二套）。
//   * 每步顺序（顺序错了行为就不对，别调换）：
//       重力积分 → 水平移动 + 解穿透 → 斜坡投影 → 台阶抬升 → 地面检测 → 卡住检测
//   * 胶囊以**中心**为位置（总高 = 圆柱段半长×2 + 半径×2），`FootPosition()` 给了
//     "脚底在哪"，写模型时用那个。
//   * 本层不读全局输入、不碰渲染、不含 Vulkan 依赖（`GN_Entity.h` 里 cENTITY 按值持有它）。
// 依赖：**必须先包含 GN.h**（GN_API / 基础类型）。本头不能自己去包含 <GN.h>：
// GN_Entity.h 包含它，而 <GN.h> 又包含 GN_Entity.h —— 谁先被 .cpp 直接包含谁就会
// 撞上"GN_Entity.h 里 cCHARACTER 还没声明"的循环包含。
#include "GN_3Dmath.h"
#include "GN_3Dmodel.h" // sMESH（AddStaticMesh）

#include <cstdint>
#include <vector>

namespace GN3d {

// ═══════════════════════════════════════════════════════════
//  描述
// ═══════════════════════════════════════════════════════════
struct sCHARACTER_DESC {
    float aRadius; // 胶囊半径（m），默认 0.3
    float aHeight; // 胶囊**总**高（含两端半球），默认 1.7
    float aMaxSlopeDeg; // 超过这个角就算墙（不能站也不能走上去），默认 45
    float aStepHeight; // 自动抬升的台阶高度（m），默认 0.35
    float aWalkSpeed; // 普通移动速度（m/s），默认 3.5
    float aSprintSpeed; // 冲刺速度（m/s），默认 6.0
    float aJumpSpeed; // 起跳瞬时竖直速度（m/s），默认 5.0
    float aGravityScale; // 重力倍数，默认 1.0（用世界重力或 SetGravity 的值）
    float aSkinWidth; // 解穿透时多推出去的余量（m），默认 0.01
    // 解穿透迭代上限：卡在凹角里需要多次推才能出来；0 或负按 1 处理。
    uint32_t aResolveIterations; // 默认 6

    sCHARACTER_DESC();
};

// 一步之后的"发生了什么"，供上层做音效/动画状态（不影响移动本身）。
struct sCHARACTER_STEP {
    bool aGrounded; // 本步结束时是否踩在地上
    bool aJumped; // 本步是否执行了起跳
    bool aHitWall; // 水平移动被墙挡住
    bool aSteppedUp; // 触发了台阶抬升
    bool aHitCeiling; // 上升时撞到头
    float aGroundNormalY; // 地面法线 y 分量（1 = 平地；无地面时为 0）
    float aSlopeDeg; // 地面坡度（度；无地面为 0）
    float aHorizontalDistance; // 本步实际水平位移（m）

    sCHARACTER_STEP();
};

// ═══════════════════════════════════════════════════════════
//  cCHARACTER
// ═══════════════════════════════════════════════════════════
class cCHARACTER {
  public:
    cCHARACTER();

    void Init(const sCHARACTER_DESC &desc);
    const sCHARACTER_DESC &Desc() const;

    // ── 几何（世界坐标静态三角网）──
    void ClearGeometry();
    void AddTriangle(const GNg::sVEC3 &a, const GNg::sVEC3 &b, const GNg::sVEC3 &c);
    // 顶点布局 = 引擎 3D 网格（11 floats/顶点，只取前 3 个）；顶点须已是世界坐标。
    void AddStaticMesh(const sMESH &mesh);
    // 一个 ±halfSize 的轴对齐盒子（6 面 × 2 三角形），底面落在 centerY - halfSize.y。
    void AddBox(const GNg::sVEC3 &center, const GNg::sVEC3 &halfSize);
    // 一块朝上的斜面：以 x 方向爬升，坡度 = atan(height / run)（度）。返回实际几何高度。
    void AddRamp(const GNg::sVEC3 &origin, float run, float rise, float width);
    // 一块朝上的水平地板（两个三角形），y = height。
    void AddFloor(const GNg::sVEC3 &center, float halfX, float halfZ, float height);
    uint32_t TriangleCount() const;

    // ── 状态 ──
    void SetPosition(const GNg::sVEC3 &centerPos); // 胶囊中心
    const GNg::sVEC3 &Position() const;
    const GNg::sVEC3 &FootPosition() const; // 胶囊最低点（写模型时用它）
    void SetVelocity(const GNg::sVEC3 &velocity);
    const GNg::sVEC3 &Velocity() const;
    void SetGravity(const GNg::sVEC3 &gravity);
    const GNg::sVEC3 &Gravity() const;

    // 瞬移：位置直接设定，**清空速度**（含竖直），并丢掉上一次的地面/卡住状态。
    // 不这样做的话，瞬移后上一帧的落地速度会让角色立刻弹飞或穿地。
    void Teleport(const GNg::sVEC3 &centerPos);

    bool IsGrounded() const;
    float GroundNormalY() const; // 无地面 = 0
    float SlopeDeg() const; // 无地面 = 0
    bool IsStuck() const; // 上一帧解穿透没解干净（被夹住）

    // ── 每步 ──
    // wishDir：**水平**期望方向（内部会归一化；零向量 = 不主动移动，只受重力）。
    // sprint：用 aSprintSpeed 而不是 aWalkSpeed。
    // jump：本步是否请求起跳（只在着地时生效）。
    // 返回本步的详细结果。
    sCHARACTER_STEP Move(const GNg::sVEC3 &wishDir, bool sprint, bool jump, float dt);

    // 便捷：直接按"意图"走一步（上层把手柄/键盘映射成这个结构）。
    struct sINTENT {
        float aMoveForward; // -1..1
        float aMoveRight; // -1..1
        bool aSprint;
        bool aJump;
        sINTENT();
    };
    sCHARACTER_STEP MoveIntent(const GNg::sVEC3 &forwardAxis, const GNg::sVEC3 &rightAxis,
                               const sINTENT &intent, float dt);

  private:
    struct sTRIANGLE {
        GNg::sVEC3 aA;
        GNg::sVEC3 aB;
        GNg::sVEC3 aC;
        GNg::sVEC3 aNormal; // 面法线（退化三角形不入表）
    };

    // 胶囊线段（下半球心 → 上半球心）
    void segment(const GNg::sVEC3 &center, GNg::sVEC3 &e0, GNg::sVEC3 &e1) const;
    float cylinderHalf() const; // 圆柱段半长 = (aHeight - 2r) / 2，钳到 >= 0
    // 最深穿透（返回是否命中）；normal 由三角形指向胶囊（"推出去"的方向）。
    bool deepestPenetration(const GNg::sVEC3 &center, GNg::sVEC3 &outNormal, float &outDepth) const;
    // 解穿透：返回是否已完全解干净（false = 迭代用尽还有穿透 ⇒ IsStuck）。
    bool resolvePenetration(GNg::sVEC3 &center) const;
    // 地面探测：返回是否着地，并输出法线 y 与坡度。
    bool probeGround(const GNg::sVEC3 &center, float &outNormalY, float &outSlopeDeg) const;
    void resetTransient();

    sCHARACTER_DESC aDesc;
    std::vector<sTRIANGLE> aTriangles;
    GNg::sVEC3 aPosition;
    GNg::sVEC3 aFootPosition;
    GNg::sVEC3 aVelocity;
    GNg::sVEC3 aGravity;
    bool aGrounded;
    bool aStuck;
    float aGroundNormalY;
    float aSlopeDeg;
};

} // namespace GN3d
#endif // _GN_CHARACTER_H_
