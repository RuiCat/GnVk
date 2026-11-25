// GN_RootMotion.cpp — Root Motion 提取（C3）。
//
// 实现口径见头文件。三处容易写错的地方：
//   1. 局部位移必须按**当前朝向**旋转，不是按"上一帧的朝向"也不是按动画里编码的朝向；
//   2. 首帧只记录 —— 否则第一帧会把 clip 起点的根位置当成位移；
//   3. 超限的帧要**重新记录**（而不是保持旧历史），否则下一帧会把两帧的差当成位移，
//      等于把瞬移拆成两次"合法"位移，钳制就白做了。

#include <GN.h>
#include "GN_RootMotion.h"

#include <cmath>

namespace GN3d {

namespace {
const float kPi = 3.14159265358979323846f;

bool vecIsFinite(const GNg::sVEC3 &v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

bool quatIsFinite(const GNg::sQUAT &q) {
    return std::isfinite(q.x) && std::isfinite(q.y) && std::isfinite(q.z) && std::isfinite(q.w);
}

// 把角度差归一化到 [-pi, pi]（处理 359° → 1° 这种跨越）
float wrapAngle(float angle) {
    while(angle > kPi) angle -= 2.0f * kPi;
    while(angle < -kPi) angle += 2.0f * kPi;
    return angle;
}

} // namespace

// ═══════════════════════════════════════════════════════════
//  sROOT_MOTION
// ═══════════════════════════════════════════════════════════
sROOT_MOTION::sROOT_MOTION() : aDeltaPosition{0.0f, 0.0f, 0.0f}, aDeltaYaw(0.0f), aValid(false) {}

// ═══════════════════════════════════════════════════════════
//  工具
// ═══════════════════════════════════════════════════════════
float quatYaw(const GNg::sQUAT &q) {
    if(!quatIsFinite(q)) return 0.0f;
    // 归一化，避免未归一化的四元数让 atan2 的比值失真
    const float len = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
    if(!(len > 1e-8f)) return 0.0f;
    const float x = q.x / len;
    const float y = q.y / len;
    const float z = q.z / len;
    const float w = q.w / len;
    // 绕 Y 的偏航角：atan2(2(wy + xz), 1 - 2(y² + x²))
    const float sinY = 2.0f * (w * y + x * z);
    const float cosY = 1.0f - 2.0f * (y * y + x * x);
    return std::atan2(sinY, cosY);
}

GNg::sVEC3 rotateYaw(const GNg::sVEC3 &v, float yawRad) {
    const float c = std::cos(yawRad);
    const float s = std::sin(yawRad);
    GNg::sVEC3 out;
    out.x = v.x * c + v.z * s;
    out.y = v.y;
    out.z = -v.x * s + v.z * c;
    return out;
}

// ═══════════════════════════════════════════════════════════
//  cROOT_MOTION_EXTRACTOR
// ═══════════════════════════════════════════════════════════
cROOT_MOTION_EXTRACTOR::cROOT_MOTION_EXTRACTOR()
    : aLastPosition{0.0f, 0.0f, 0.0f}, aLastRotation(), aFacingYaw(0.0f), aMaxStep(2.0f),
      aMaxYawStep(4.0f), aLoopStride(0.0f), aWrapCount(0), aHasHistory(false) {
    aLastRotation.w = 1.0f;
}

void cROOT_MOTION_EXTRACTOR::Reset() {
    aLastPosition = GNg::sVEC3{0.0f, 0.0f, 0.0f};
    aLastRotation = GNg::sQUAT();
    aLastRotation.w = 1.0f;
    aHasHistory = false;
    aWrapCount = 0;
}

void cROOT_MOTION_EXTRACTOR::SetFacingYaw(float yawRad) {
    aFacingYaw = std::isfinite(yawRad) ? wrapAngle(yawRad) : 0.0f;
}

float cROOT_MOTION_EXTRACTOR::FacingYaw() const { return aFacingYaw; }

void cROOT_MOTION_EXTRACTOR::SetLimits(float maxStep, float maxYawStep) {
    aMaxStep = (std::isfinite(maxStep) && maxStep > 0.0f) ? maxStep : 2.0f;
    aMaxYawStep = (std::isfinite(maxYawStep) && maxYawStep > 0.0f) ? maxYawStep : 4.0f;
}

float cROOT_MOTION_EXTRACTOR::MaxStep() const { return aMaxStep; }

float cROOT_MOTION_EXTRACTOR::MaxYawStep() const { return aMaxYawStep; }

void cROOT_MOTION_EXTRACTOR::SetLoopStride(float strideLength) {
    aLoopStride = (std::isfinite(strideLength) && strideLength > 0.0f) ? strideLength : 0.0f;
}

float cROOT_MOTION_EXTRACTOR::LoopStride() const { return aLoopStride; }

uint32_t cROOT_MOTION_EXTRACTOR::WrapCount() const { return aWrapCount; }

void cROOT_MOTION_EXTRACTOR::Prime(const sANIM_TRANSFORM &root) {
    if(!vecIsFinite(root.aTranslation) || !quatIsFinite(root.aRotation)) return;
    aLastPosition = root.aTranslation;
    aLastRotation = root.aRotation;
    aHasHistory = true;
}

bool cROOT_MOTION_EXTRACTOR::HasHistory() const { return aHasHistory; }

bool cROOT_MOTION_EXTRACTOR::Extract(const sANIM_TRANSFORM &root, sROOT_MOTION *out) {
    if(out != nullptr) *out = sROOT_MOTION();
    if(!vecIsFinite(root.aTranslation) || !quatIsFinite(root.aRotation)) {
        // 脏数据：不推进历史，也不要产生位移
        return false;
    }

    // 首帧：只记录。很多 clip 的第 0 帧根关节并不在原点，
    // 直接当位移用会让角色"跳"到那个位置。
    if(!aHasHistory) {
        aLastPosition = root.aTranslation;
        aLastRotation = root.aRotation;
        aHasHistory = true;
        return false;
    }

    // 局部位移（动画自身的坐标）→ 按当前朝向转到世界
    GNg::sVEC3 localDelta;
    localDelta.x = root.aTranslation.x - aLastPosition.x;
    localDelta.y = root.aTranslation.y - aLastPosition.y;
    localDelta.z = root.aTranslation.z - aLastPosition.z;

    const float lastYaw = quatYaw(aLastRotation);
    const float nowYaw = quatYaw(root.aRotation);
    const float deltaYaw = wrapAngle(nowYaw - lastYaw);

    // 前进历史（无论是否采纳都要前移，否则下一帧的差会累积）
    aLastPosition = root.aTranslation;
    aLastRotation = root.aRotation;

    // ── 循环回绕补偿 ──
    // 循环型 clip 的根关节在末尾回绕到起点，位移会是约 -strideLength 的一次大跳。
    // 这不是瞬移，而是"这一圈走完了"。补上 strideLength 让它等价于"继续前进"。
    // 不处理的话：丢掉这一帧 ⇒ 每循环少走一步（实测 4 秒只走 0.03 m）；
    // 当正常位移 ⇒ 角色被倒退一整步。两种都是错的。
    if(aLoopStride > 0.0f && localDelta.z < 0.0f) {
        const float horizLen = std::sqrt(localDelta.x * localDelta.x + localDelta.z * localDelta.z);
        const float tol = aLoopStride * 0.25f;
        if(std::fabs(horizLen - aLoopStride) <= tol) {
            localDelta.z += aLoopStride; // "倒退一圈" → "向前一小步"
            ++aWrapCount;
        }
    }

    // 瞬移检测：超限整帧丢弃（历史已经前移，所以下一帧不会把这两帧的差再算一遍）
    const float stepLen = std::sqrt(localDelta.x * localDelta.x + localDelta.z * localDelta.z +
                                    localDelta.y * localDelta.y);
    if(stepLen > aMaxStep) return false;
    if(std::fabs(deltaYaw) > aMaxYawStep) return false;

    if(out != nullptr) {
        out->aDeltaPosition = rotateYaw(localDelta, aFacingYaw);
        out->aDeltaYaw = deltaYaw;
        out->aValid = true;
    }
    return true;
}

} // namespace GN3d
