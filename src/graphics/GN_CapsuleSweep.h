#pragma once
#ifndef _GN_CAPSULE_SWEEP_H_
#define _GN_CAPSULE_SWEEP_H_
// ╔══════════════════════════════════════════════════════════╗
// ║  GN_mini — 胶囊扫掠（C4，纯 CPU）                        ║
// ╚══════════════════════════════════════════════════════════╝
//
// 解决的问题：`cCHARACTER` 的台阶抬升是"抬-移-落"三步 —— 先假设能抬上去，
// 试移动，再往下探。对**薄台阶**（高度小于胶囊半径）它会有穿插感：
// 抬起来的那一帧脚已经越过了台阶顶，往下探却还"没进"台阶内部，于是落点悬空，
// 下一帧再掉一次 —— 表现为"上台阶时抖一下"。
//
// 根本原因：**离散点查询**（只在起点/终点做"有没有穿透"）会漏掉"中途扫过"的情况。
// 正确做法是**扫掠**：把胶囊从起点沿位移扫到终点，求出最早接触时刻 t（TOI），
// 在 t 处停下并沿法线推出。这就是 sweep / continuous collision。
//
// 本文件的算法：保守推进（conservative advancement）+ 二分细化。
//   1. 从 t=0 开始，在当前位置查一次穿透；
//   2. 没有穿透 → 用**到最近几何的距离**决定安全推进量（不是固定步长，
//      这样"贴着墙走"不会退化成无限小步长）；
//   3. 有穿透 → 在 [t_prev, t] 之间二分，逼近最早接触点；
//   4. 收敛后返回 t 与接触法线。
//
// 为什么不用解析 TOI：胶囊对三角网的解析扫掠要处理"球扫掠面/圆柱扫掠面/端盖"三类
// 二次曲面与三角形的组合，共十几种 case，写对很难、单测也很难覆盖。
// 保守推进的代价是精度略低（本实现 1e-4 的相对精度），但**每个分支都能测**。

#include "GN_3Dmath.h"
#include "GN_Physics.h" // sCONTACT_MANIFOLD / queryCapsuleTriangle

#include <cstdint>
#include <vector>

namespace GN3d {

struct sCAPSULE_SWEEP_RESULT {
    float aTOI; // 最早接触时刻，[0,1]；1 表示全程无碰撞
    GNg::sVEC3 aNormal; // 接触点法线（由几何指向胶囊，"推出去"的方向）；无接触时为零
    GNg::sVEC3 aPosition; // 在 TOI 处并被推出后的位置（安全位置）
    bool aHit; // 是否发生了接触
    uint32_t aIterations; // 迭代次数（诊断/性能）

    sCAPSULE_SWEEP_RESULT();
};

// 一段三角形（世界坐标）
struct sSWEEP_TRIANGLE {
    GNg::sVEC3 aA;
    GNg::sVEC3 aB;
    GNg::sVEC3 aC;
};

struct sCAPSULE_SWEEP_PARAMS {
    float aRadius;
    float aHalfHeight; // 圆柱段半长（总半高 = aHalfHeight + aRadius），与 sRIGID_BODY 一致
    float aSkinWidth; // 附加的安全余量（默认 1e-3）
    uint32_t aMaxIterations; // 默认 24
    float aTolerance; // TOI 精度（默认 1e-4，相对整段位移）

    sCAPSULE_SWEEP_PARAMS();
};

// 扫掠一棵胶囊。from/to 是**胶囊中心**（质心）的起止位置。
// 返回最早接触时刻与安全位置。
//
// `outNormal` 的方向约定与 `queryCapsuleTriangle` 一致：几何 → 胶囊。
sCAPSULE_SWEEP_RESULT sweepCapsuleAgainstTriangles(const GNg::sVEC3 &from, const GNg::sVEC3 &to,
                                                   const sCAPSULE_SWEEP_PARAMS &params,
                                                   const sSWEEP_TRIANGLE *triangles,
                                                   uint32_t triangleCount);

// 辅助：胶囊中心在 center 时，到所有三角形的**最近距离**（负值表示已穿透）。
// 保守推进用它决定"能安全走多远"。
float capsuleMinDistance(const GNg::sVEC3 &center, const sCAPSULE_SWEEP_PARAMS &params,
                         const sSWEEP_TRIANGLE *triangles, uint32_t triangleCount);

} // namespace GN3d
#endif // _GN_CAPSULE_SWEEP_H_
