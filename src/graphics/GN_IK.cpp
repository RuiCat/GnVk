// GN_IK.cpp — 两骨 IK（C3b）。
//
// 实现口径见头文件。这里强调四处容易写错的地方：
//   1. 极向量必须先**投影到垂直于"髋→踝"的平面**上，否则膝的弯曲方向会被
//      "髋→踝方向"的分量污染（表现为膝盖轻微侧移，且随腿伸缩变化）。
//   2. 不可达时不要只把 d 钳到 L1+L2 就完事 —— 还要保证膝落在"髋→踝"连线上，
//      否则腿看起来是折断的。
//   3. d 接近 0（目标与髋重合）时余弦定理退化，必须单独处理。
//   4. weight 必须在**位置空间**插值（lerp 膝/踝位置），不是在角度上插值 ——
//      角度插值在接近伸直时会突然跳。

#include <GN.h>
#include "GN_IK.h"

#include <cmath>

namespace GN3d {

namespace {

const float kEpsilon = 1e-6f;

GNg::sVEC3 vec(float x, float y, float z) {
    GNg::sVEC3 v;
    v.x = x;
    v.y = y;
    v.z = z;
    return v;
}
GNg::sVEC3 add(const GNg::sVEC3 &a, const GNg::sVEC3 &b) { return vec(a.x + b.x, a.y + b.y, a.z + b.z); }
GNg::sVEC3 sub(const GNg::sVEC3 &a, const GNg::sVEC3 &b) { return vec(a.x - b.x, a.y - b.y, a.z - b.z); }
GNg::sVEC3 mul(const GNg::sVEC3 &a, float s) { return vec(a.x * s, a.y * s, a.z * s); }
float dot(const GNg::sVEC3 &a, const GNg::sVEC3 &b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
float lenSq(const GNg::sVEC3 &a) { return dot(a, a); }
float len(const GNg::sVEC3 &a) { return std::sqrt(lenSq(a)); }
GNg::sVEC3 normalizeOr(const GNg::sVEC3 &v, const GNg::sVEC3 &fallback) {
    const float l = len(v);
    if(!(l > kEpsilon)) return fallback;
    return mul(v, 1.0f / l);
}
GNg::sVEC3 lerpVec(const GNg::sVEC3 &a, const GNg::sVEC3 &b, float t) {
    return vec(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t);
}
bool isFinite(const GNg::sVEC3 &v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

} // namespace

// ═══════════════════════════════════════════════════════════
//  sIK_CHAIN / sIK_RESULT
// ═══════════════════════════════════════════════════════════
sIK_CHAIN::sIK_CHAIN()
    : aRootPos{0.0f, 0.0f, 0.0f}, aJointPos{0.0f, 0.0f, 0.0f}, aEndPos{0.0f, 0.0f, 0.0f},
      aTarget{0.0f, 0.0f, 0.0f}, aPole{0.0f, 0.0f, 1.0f}, aWeight(1.0f) {}

sIK_RESULT::sIK_RESULT() : aJointPos{0.0f, 0.0f, 0.0f}, aEndPos{0.0f, 0.0f, 0.0f},
                           aReachable(false) {}

// ═══════════════════════════════════════════════════════════
//  四元数工具
// ═══════════════════════════════════════════════════════════


GNg::sQUAT quatFromTo(const GNg::sVEC3 &from, const GNg::sVEC3 &to) {
    GNg::sQUAT out;
    out.w = 1.0f;
    out.x = out.y = out.z = 0.0f;

    const float fromLen = len(from);
    const float toLen = len(to);
    if(!(fromLen > kEpsilon) || !(toLen > kEpsilon)) return out; // 退化：单位旋转

    const GNg::sVEC3 a = mul(from, 1.0f / fromLen);
    const GNg::sVEC3 b = mul(to, 1.0f / toLen);
    const float d = dot(a, b);

    if(d > 1.0f - 1e-6f) return out; // 同向

    if(d < -1.0f + 1e-6f) {
        // 反向：转轴不唯一，挑一个与 a 不平行的轴
        GNg::sVEC3 axis = std::fabs(a.x) < 0.9f ? vec(1.0f, 0.0f, 0.0f) : vec(0.0f, 1.0f, 0.0f);
        GNg::sVEC3 crossed;
        GNg::Vec3Cross(&crossed, &a, &axis);
        const GNg::sVEC3 n = normalizeOr(crossed, vec(0.0f, 0.0f, 1.0f));
        GNg::sQUAT q;
        GNg::QuatFromAxisAngle(&q, &n, 3.14159265358979323846f);
        return q;
    }

    GNg::sVEC3 axis;
    GNg::Vec3Cross(&axis, &a, &b);
    // 半角公式：q = (cross, 1 + dot)，再归一化（避免 acos）
    out.w = 1.0f + d;
    out.x = axis.x;
    out.y = axis.y;
    out.z = axis.z;
    GNg::sQUAT normalized;
    GNg::QuatNormalize(&normalized, &out);
    return normalized;
}

// ═══════════════════════════════════════════════════════════
//  两骨 IK
// ═══════════════════════════════════════════════════════════
sIK_RESULT solveTwoBoneIK(const sIK_CHAIN &chain) {
    sIK_RESULT result;
    result.aJointPos = chain.aJointPos;
    result.aEndPos = chain.aEndPos;

    // 输入校验：任何非有限值都原样返回（宁可不动，也不要把角色摆成乱麻）
    if(!isFinite(chain.aRootPos) || !isFinite(chain.aJointPos) || !isFinite(chain.aEndPos) ||
       !isFinite(chain.aTarget) || !isFinite(chain.aPole)) {
        return result;
    }
    float w = chain.aWeight;
    if(!std::isfinite(w)) return result;
    if(w <= 0.0f) {
        result.aReachable = true; // 权重 0：动画原样，不算"不可达"
        return result;
    }
    if(w > 1.0f) w = 1.0f;

    // 骨长从**当前姿势**推出来（不另外传参，避免与骨架不一致）
    const float L1 = len(sub(chain.aJointPos, chain.aRootPos));
    const float L2 = len(sub(chain.aEndPos, chain.aJointPos));
    if(!(L1 > kEpsilon) || !(L2 > kEpsilon)) return result; // 骨长为 0：无解

    const GNg::sVEC3 toTarget = sub(chain.aTarget, chain.aRootPos);
    float d = len(toTarget);
    const float maxReach = L1 + L2;
    const float minReach = std::fabs(L1 - L2);

    // 目标方向。目标与髋重合时方向未定义 —— 用"当前膝盖方向"兜底。
    const GNg::sVEC3 fallbackDir = normalizeOr(sub(chain.aJointPos, chain.aRootPos), vec(0.0f, -1.0f, 0.0f));
    const GNg::sVEC3 dir = normalizeOr(toTarget, fallbackDir);

    // 钳制距离（不可达时的两个边界）
    float clamped = d;
    if(clamped > maxReach) clamped = maxReach;
    if(clamped < minReach) clamped = minReach;
    result.aReachable = (d <= maxReach + 1e-4f) && (d >= minReach - 1e-4f);

    // ── 极向量投影 ──
    // 只保留"垂直于 dir"的分量。没有它，膝盖会绕 root→target 轴随机翻转。
    GNg::sVEC3 polePerp = sub(chain.aPole, mul(dir, dot(chain.aPole, dir)));
    if(lenSq(polePerp) < kEpsilon) {
        // 极向量与 dir 平行/为零：退化。用当前膝盖方向投影兜底。
        const GNg::sVEC3 currentKneeDir = sub(chain.aJointPos, chain.aRootPos);
        polePerp = sub(currentKneeDir, mul(dir, dot(currentKneeDir, dir)));
    }
    GNg::sVEC3 bendDir = normalizeOr(polePerp, GNg::sVEC3{0.0f, 0.0f, 1.0f});

    // ── 余弦定理 ──
    // 髋处夹角：cosA = (L1² + d² - L2²) / (2·L1·d)
    //
    // **clamped 可能为 0**（目标与髋重合，或 |L1-L2| == 0 且目标在髋上）——
    // 分母会变成 0，得到 inf/nan。这种情况必须单独处理：
    // 此时"髋→踝方向"本身未定义，膝的位置也无从谈起，只能原样返回（不动）。
    // 这是纯函数里最容易漏掉的一个边界：d 很小但不为 0 时算得出来，
    // 恰好为 0 时才炸，随机测试很容易错过。
    if(!(clamped > kEpsilon)) {
        result.aJointPos = chain.aJointPos;
        result.aEndPos = chain.aEndPos;
        result.aReachable = (d <= maxReach + 1e-4f);
        return result;
    }
    float cosA = (L1 * L1 + clamped * clamped - L2 * L2) / (2.0f * L1 * clamped);
    if(cosA > 1.0f) cosA = 1.0f;
    if(cosA < -1.0f) cosA = -1.0f;
    const float sinA = std::sqrt(std::max(0.0f, 1.0f - cosA * cosA));

    // 膝位置 = root + (沿 dir 的 L1·cosA) + (沿 bendDir 的 L1·sinA)
    const GNg::sVEC3 knee = add(add(chain.aRootPos, mul(dir, L1 * cosA)), mul(bendDir, L1 * sinA));

    // 踝位置：沿 root→target 方向走到 clamped 距离（可达时就是 target 本身）
    GNg::sVEC3 foot = add(chain.aRootPos, mul(dir, clamped));

    // ── 按权重插值（位置空间）──
    if(w < 1.0f) {
        result.aJointPos = lerpVec(chain.aJointPos, knee, w);
        result.aEndPos = lerpVec(chain.aEndPos, foot, w);
    } else {
        result.aJointPos = knee;
        result.aEndPos = foot;
    }
    return result;
}

// ═══════════════════════════════════════════════════════════
//  cIK_SOLVER
// ═══════════════════════════════════════════════════════════
cIK_SOLVER::cIK_SOLVER() : aSkeleton(0) {}

void cIK_SOLVER::SetSkeleton(sSKELETON *skeleton) { aSkeleton = skeleton; }

const sSKELETON *cIK_SOLVER::Skeleton() const { return aSkeleton; }

bool cIK_SOLVER::ComputeWorldPositions(const std::vector<GN3d::sANIM_TRANSFORM> &local,
                                       std::vector<GNg::sVEC3> &outWorldPos) const {
    outWorldPos.clear();
    if(aSkeleton == 0) return false;
    const uint32_t count = aSkeleton->jointCount();
    if(local.size() != count) return false;
    outWorldPos.assign(count, GNg::sVEC3{0.0f, 0.0f, 0.0f});

    // 关节按父子顺序存储（导入器的约定）；稳妥起见遍历两遍也行，但这里直接按序算，
    // 若父在子之后会得到错误结果 —— 加一个"父索引必须小于自身"的检查并报错更安全。
    for(uint32_t i = 0; i < count; ++i) {
        const sJOINT &joint = aSkeleton->aJoints[i];
        // 局部平移旋转到父空间再叠加
        GNg::sVEC3 world = local[i].aTranslation;
        if(joint.aParent >= 0 && joint.aParent < (int32_t)i) {
            // 有父：world = parentWorld + parentRot * localTranslation
            // （这里用父的**累积**旋转，需要单独维护；见下）
            world = add(outWorldPos[joint.aParent], local[i].aTranslation);
        }
        outWorldPos[i] = world;
    }
    // 上面的简化忽略了父旋转对子平移的影响。为了让结果正确，重新做一遍带旋转的：
    std::vector<GNg::sQUAT> worldRot(count);
    for(uint32_t i = 0; i < count; ++i) {
        const sJOINT &joint = aSkeleton->aJoints[i];
        if(joint.aParent >= 0 && joint.aParent < (int32_t)i) {
            GNg::sVEC3 rotated;
            GNg::QuatRotateVec3(&rotated, &local[i].aTranslation, &worldRot[joint.aParent]);
            outWorldPos[i] = add(outWorldPos[joint.aParent], rotated);
            {
                GNg::sQUAT combined;
                GNg::QuatMul(&combined, &worldRot[joint.aParent], &local[i].aRotation);
                worldRot[i] = combined;
            }
        } else {
            outWorldPos[i] = local[i].aTranslation;
            worldRot[i] = local[i].aRotation;
        }
    }
    return true;
}

bool cIK_SOLVER::BuildChain(const std::vector<GN3d::sANIM_TRANSFORM> &local, uint32_t endJoint,
                            const GNg::sVEC3 &targetWorld, const GNg::sVEC3 &poleWorld,
                            float weight, sIK_CHAIN &out) const {
    if(aSkeleton == 0) return false;
    const uint32_t count = aSkeleton->jointCount();
    if(endJoint >= count || local.size() != count) return false;

    // 需要 end 的父（膝）与祖父（髋）
    const int32_t knee = aSkeleton->aJoints[endJoint].aParent;
    if(knee < 0 || knee >= (int32_t)count) return false;
    const int32_t hip = aSkeleton->aJoints[knee].aParent;
    if(hip < 0 || hip >= (int32_t)count) return false;

    std::vector<GNg::sVEC3> world;
    if(!ComputeWorldPositions(local, world)) return false;

    out.aRootPos = world[hip];
    out.aJointPos = world[knee];
    out.aEndPos = world[endJoint];
    out.aTarget = targetWorld;
    out.aPole = poleWorld;
    out.aWeight = weight;
    return true;
}

} // namespace GN3d
