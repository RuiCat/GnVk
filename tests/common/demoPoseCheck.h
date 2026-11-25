#pragma once
#ifndef _GN_DEMO_POSE_CHECK_H_
#define _GN_DEMO_POSE_CHECK_H_
// demoPoseCheck.h — 演示程序的自检（防止"腿反了"那类方向错误）
//
// ═══ 为什么需要它 ═══
// 2026-11-25 的 rootMotion / animBlend 两个演示里，火柴人的**腿是反的（脚朝上）**，
// 而我还截了图、看了图，没看出来。事后定位根因是：
//
//   腿的位置偏移按方向 (0, -cos, +sin) 算，
//   而四元数却写成 +sin ⇒ 实际旋转方向是 (0, -cos, -sin)。
//   两者**符号相反**，于是腿的下端跑到髋的高度上去了。
//
// 关键点：这个错误**AABB 查不出来**。AABB 只描述"形状占多大地方"，
// 不描述"哪一端是脚"。所以必须显式比较**两个方向**：
//   ① 从关节指向肢体中心的位移方向（位置偏移算出来的）
//   ② 从四元数反推的肢体朝向
// 两者必须同向。这一条能抓住"位置与旋转不一致"整类错误。
//
// 用法（每帧摆完姿势后调一次；失败会打印并返回 false）：
//   if (!demoCheck::checkLimb(leg, hipPos, "左腿")) gPoseFailed = true;
// 演示程序在 main 里据此返回非零退出码，smoke 就能拦住。

#include <GN.h>
#include "GN_3Dmath.h"
#include "GN_3Dmodel.h"

#include <cmath>
#include <cstdio>

namespace demoCheck {

// 判据的区分度随摆动角度增大。低于这个角度两种实现算出的量几乎一样，
// 检查会漏掉问题 —— 调用方应只在 |angle| >= 该值时检查。
const float kMinAngleForCheck = 0.15f;

// 从"绕单轴旋转"的四元数反推肢体朝向（局部 -Y 旋转后的世界方向）。
// axis: 0=X, 1=Y, 2=Z。只处理单轴旋转 —— 演示里的肢体都是这样摆的；
// 复合旋转请改用矩阵（此处刻意保持简单，避免自己实现出第四个 bug）。
inline GNg::sVEC3 limbDirFromQuat(const GNg::sQUAT &q, int axis) {
    float s = (axis == 0) ? q.x : (axis == 1) ? q.y : q.z;
    if(s < -1.0f) s = -1.0f;
    if(s > 1.0f) s = 1.0f;
    const float theta = 2.0f * std::asin(s);
    const float c = std::cos(theta);
    const float sn = std::sin(theta);
    // R_x(θ)·(0,-1,0) = (0,-c,-sn)；R_y / R_z 对应地把 -Y 转出去
    if(axis == 0) return GNg::sVEC3{0.0f, -c, -sn};
    if(axis == 1) return GNg::sVEC3{sn, -c, 0.0f};
    return GNg::sVEC3{-sn, -c, 0.0f};
}

// 检查一个肢体：位置偏移用的方向 与 四元数给出的朝向 是否一致。
//
// ═══ 判据的推导（第一版写错过，记下来）═══
// 设关节 J、肢体中心 C、肢体半长 h = L/2、横向偏移 lat（左右腿分开的那点距离）。
// 正确实现里 C = J + dir·h + lat，其中 dir 就是四元数给出的朝向。
// 若位置偏移用 dir_pos 而四元数给 dir_quat，且两者不一致（本次 bug：只有 z 分量符号相反）：
//     C = J + dir_pos·h + lat
//     (C - J) 沿 dir_quat 的**垂直分量** = |lat + (dir_pos - dir_quat)·h|
//     正确时 = |lat|（只有横向偏移），bug 版会显著变大。
//
// 实测（L=0.72, lat=0.13）：
//     angle=0.30：正确 垂直分量 0.130、bug 0.241（涨 85%，阈值 0.198 ⇒ 抓到）
//     angle=0.09：正确 0.130、      bug 0.146（只涨 12%，抓不到）
// **所以本检查必须在肢体有明显摆动时才做** —— 静止姿势下两种实现几乎无差别。
// 调用方请用 `|angle| > 0.15` 之类的前置条件（见 kMinAngleForCheck）。
//
// ═══ 另一个前提：横向偏移必须**垂直于**肢体方向 ═══
// 推导里假设 `lat ⊥ dir`。腿满足（腿朝下、左右分开在 X）。
// 但**张开的手臂不满足**：肩宽偏移在 X，而手臂转开后又有了 X 分量，两者不垂直，
// 于是 lat 会投影到平行分量上，把 parallel 顶到上限之外 —— **误报**。
// （第一版对 animBlend 的手臂也做了检查，结果正确实现被误判成 bug。）
// 结论：本检查适用于"肢体方向近似垂直于横向偏移"的场合，主要就是腿。
// 手臂这类横向肢体请用别的判据（或干脆不做）。
//
// 反过来"只看 AABB 或只看脚底高度"都抓不到这个 bug —— 两个版本的脚底 y 是一样的
// （因为 dir_pos 与 dir_quat 的 y 分量相同，只有 z 相反）。
inline bool checkLimb(const GN3d::cMODEL *limb, const GNg::sVEC3 &jointPos, float limbLength,
                      const char *name, int axis = 0, float maxDirY = -0.2f) {
    if(limb == 0) return true;
    const float dx = limb->aPosition.x - jointPos.x;
    const float dy = limb->aPosition.y - jointPos.y;
    const float dz = limb->aPosition.z - jointPos.z;
    const GNg::sVEC3 quatDir = limbDirFromQuat(limb->aRotation, axis);
    // 平行分量（沿肢体朝向）
    const float parallel = dx * quatDir.x + dy * quatDir.y + dz * quatDir.z;
    // 垂直分量（扣掉平行部分剩下的）
    const float px = dx - quatDir.x * parallel;
    const float py = dy - quatDir.y * parallel;
    const float pz = dz - quatDir.z * parallel;
    const float perp = std::sqrt(px * px + py * py + pz * pz);

    const float half = (limbLength > 0.0f) ? limbLength * 0.5f : 0.0f;

    // ① 平行分量应 ≈ 半长（容差 25%）；太小/为负说明方向整体不对
    if(half > 0.0f && (parallel < half * 0.75f || parallel > half * 1.25f)) {
        std::fprintf(stderr,
                     "[poseCheck] FAIL %s: 沿肢体方向的偏移 %.3f，期望 ≈ %.3f"
                     " —— 位置偏移与旋转方向不一致\n",
                     name, parallel, half);
        return false;
    }
    // ② 垂直分量只能有"横向偏移"那点量级（< 半长 * 0.55 ≈ lat 的两倍）
    if(half > 0.0f && perp > half * 0.55f) {
        std::fprintf(stderr,
                     "[poseCheck] FAIL %s: 垂直分量 %.3f 过大（上限 %.3f）"
                     " —— 肢体被扭成斜的，通常是位置偏移与四元数符号相反\n",
                     name, perp, half * 0.55f);
        return false;
    }
    // ③ 朝向约束（腿要朝下；手臂传 maxDirY=1.0 表示不限制）
    if(quatDir.y > maxDirY) {
        std::fprintf(stderr, "[poseCheck] FAIL %s: 肢体朝向异常 (dir.y=%.3f, 期望 <= %.3f)\n",
                     name, quatDir.y, maxDirY);
        return false;
    }
    return true;
}

// ═══════════════════════════════════════════════════════════
//  通用版：不假设"绕单轴旋转"
// ═══════════════════════════════════════════════════════════
// 上面那个 checkLimb 用 limbDirFromQuat 反推朝向，只对"绕 X/Y/Z 单轴"成立。
// 但用 `quatFromTo(down, dir)` 摆出来的骨头是任意轴旋转 —— 那个假设不成立。
//
// 这里直接**用四元数把局部 -Y 转过去**（与真实渲染完全同一套数学），所以对任意
// 旋转都成立，且不依赖"哪根轴"。
//
// 判据同前：肢体中心相对关节的位移，必须**平行于**肢体朝向
//   * 平行分量 ≈ 半长（容差 25%）
//   * 垂直分量 ≤ 半长 × 0.55（只允许横向偏移那点量级）
inline bool checkLimbAny(const GN3d::cMODEL *limb, const GNg::sVEC3 &jointPos, float limbLength,
                         const char *name) {
    if(limb == 0) return true;
    const float dx = limb->aPosition.x - jointPos.x;
    const float dy = limb->aPosition.y - jointPos.y;
    const float dz = limb->aPosition.z - jointPos.z;

    // 局部 -Y 经该旋转后的世界朝向（与渲染同源，不做单轴假设）
    const GNg::sVEC3 localDown{0.0f, -1.0f, 0.0f};
    GNg::sVEC3 dir;
    GNg::QuatRotateVec3(&dir, &localDown, &limb->aRotation);

    const float parallel = dx * dir.x + dy * dir.y + dz * dir.z;
    const float px = dx - dir.x * parallel;
    const float py = dy - dir.y * parallel;
    const float pz = dz - dir.z * parallel;
    const float perp = std::sqrt(px * px + py * py + pz * pz);
    const float half = (limbLength > 0.0f) ? limbLength * 0.5f : 0.0f;

    if(half > 0.0f && (parallel < half * 0.75f || parallel > half * 1.25f)) {
        std::fprintf(stderr,
                     "[poseCheck] FAIL %s: 沿肢体方向的偏移 %.3f，期望 ≈ %.3f"
                     " —— 位置偏移与旋转方向不一致\n",
                     name, parallel, half);
        return false;
    }
    if(half > 0.0f && perp > half * 0.55f) {
        std::fprintf(stderr,
                     "[poseCheck] FAIL %s: 垂直分量 %.3f 过大（上限 %.3f）"
                     " —— 肢体被扭成斜的\n",
                     name, perp, half * 0.55f);
        return false;
    }
    return true;
}

} // namespace demoCheck
#endif // _GN_DEMO_POSE_CHECK_H_
