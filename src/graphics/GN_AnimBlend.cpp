// GN_AnimBlend.cpp — 1D 混合空间 + 动画层遮罩（D3）。
//
// 实现口径见头文件。这里强调三处容易写错的地方：
//   1. 权重必须**归一化到 1**（含"只有部分 clip 有该节点通道"的重归一化）；
//   2. 相位同步要让每个 clip 用自己的 duration 换算时间；
//   3. 层的采样失败**不能**当成"单位变换"参与插值。

#include <GN.h>
#include "GN_AnimBlend.h"

#include <algorithm>
#include <cmath>

namespace GN3d {

namespace {

// 与 GN_3Danim.cpp 里的同名函数保持一致的插值口径。
// 不能直接复用它（那个是文件内 static），但语义必须一样：旋转走最短弧 slerp。
void blendTransform(const sANIM_TRANSFORM &from, const sANIM_TRANSFORM &to, float t,
                    sANIM_TRANSFORM &out) {
    out.aTranslation.x = from.aTranslation.x + (to.aTranslation.x - from.aTranslation.x) * t;
    out.aTranslation.y = from.aTranslation.y + (to.aTranslation.y - from.aTranslation.y) * t;
    out.aTranslation.z = from.aTranslation.z + (to.aTranslation.z - from.aTranslation.z) * t;
    out.aScale.x = from.aScale.x + (to.aScale.x - from.aScale.x) * t;
    out.aScale.y = from.aScale.y + (to.aScale.y - from.aScale.y) * t;
    out.aScale.z = from.aScale.z + (to.aScale.z - from.aScale.z) * t;
    GNg::QuatSlerp(&out.aRotation, &from.aRotation, &to.aRotation, t);
}

bool transformIsFinite(const sANIM_TRANSFORM &t) {
    return std::isfinite(t.aTranslation.x) && std::isfinite(t.aTranslation.y) &&
           std::isfinite(t.aTranslation.z) && std::isfinite(t.aRotation.x) &&
           std::isfinite(t.aRotation.y) && std::isfinite(t.aRotation.z) &&
           std::isfinite(t.aRotation.w) && std::isfinite(t.aScale.x) &&
           std::isfinite(t.aScale.y) && std::isfinite(t.aScale.z);
}

} // namespace

// ═══════════════════════════════════════════════════════════
//  sANIM_BLEND_POINT
// ═══════════════════════════════════════════════════════════
sANIM_BLEND_POINT::sANIM_BLEND_POINT() : aThreshold(0.0f), aClip(0) {}

// ═══════════════════════════════════════════════════════════
//  cANIM_BLEND_1D
// ═══════════════════════════════════════════════════════════
cANIM_BLEND_1D::cANIM_BLEND_1D() : aParameter(0.0f), aPhase(0.0f), aLoop(true) {}

void cANIM_BLEND_1D::Clear() {
    aPoints.clear();
    aParameter = 0.0f;
    aPhase = 0.0f;
    aLoop = true;
}

bool cANIM_BLEND_1D::AddPoint(float threshold, const sANIM_CLIP *clip) {
    if(clip == 0) return false;
    if(!std::isfinite(threshold)) return false;
    // 阈值必须唯一：两点同阈值会让权重插值出现 0/0。
    for(size_t i = 0; i < aPoints.size(); ++i) {
        if(aPoints[i].aThreshold == threshold) return false;
    }
    sANIM_BLEND_POINT point;
    point.aThreshold = threshold;
    point.aClip = clip;
    aPoints.push_back(point);
    // 保持按阈值升序 —— EvaluateWeights 与 Sample 都依赖有序（避免每次调用时排序）
    std::sort(aPoints.begin(), aPoints.end(),
              [](const sANIM_BLEND_POINT &lhs, const sANIM_BLEND_POINT &rhs) {
                  return lhs.aThreshold < rhs.aThreshold;
              });
    return true;
}

uint32_t cANIM_BLEND_1D::PointCount() const { return (uint32_t)aPoints.size(); }

const sANIM_BLEND_POINT *cANIM_BLEND_1D::PointAt(uint32_t index) const {
    if(index >= aPoints.size()) return 0;
    return &aPoints[index];
}

void cANIM_BLEND_1D::SetParameter(float parameter) {
    aParameter = std::isfinite(parameter) ? parameter : 0.0f;
}

float cANIM_BLEND_1D::Parameter() const { return aParameter; }

void cANIM_BLEND_1D::EvaluateWeights(float parameter, std::vector<float> &out) const {
    out.clear();
    const size_t count = aPoints.size();
    if(count == 0) return;
    out.assign(count, 0.0f);
    if(!std::isfinite(parameter)) parameter = aPoints[0].aThreshold;

    // 两端外：钳到端点（不外推）
    if(parameter <= aPoints[0].aThreshold) {
        out[0] = 1.0f;
        return;
    }
    if(parameter >= aPoints[count - 1].aThreshold) {
        out[count - 1] = 1.0f;
        return;
    }
    // 落在相邻两点之间：线性权重
    for(size_t i = 0; i + 1 < count; ++i) {
        const float lo = aPoints[i].aThreshold;
        const float hi = aPoints[i + 1].aThreshold;
        if(!(parameter >= lo && parameter <= hi)) continue;
        const float span = hi - lo;
        if(!(span > 0.0f)) { // AddPoint 保证唯一，理论上不会到这
            out[i] = 1.0f;
            return;
        }
        const float u = (parameter - lo) / span;
        out[i] = 1.0f - u;
        out[i + 1] = u;
        return;
    }
    // 理论上到不了（上面的两段已覆盖整个实数轴），保险起见给端点
    out[0] = 1.0f;
}

void cANIM_BLEND_1D::Play(bool loop) {
    aPhase = 0.0f;
    aLoop = loop;
}

void cANIM_BLEND_1D::Update(float dt) {
    if(!(dt > 0.0f)) return; // 负数/NaN 不动
    const float duration = Duration();
    if(!(duration > 0.0f)) return; // 没有可用的时长：相位无意义
    aPhase += dt / duration;
    if(aLoop) {
        // 用 fmod 而不是 while 循环：dt 可能远大于 duration（掉帧后一次追很多圈）
        aPhase = std::fmod(aPhase, 1.0f);
        if(aPhase < 0.0f) aPhase += 1.0f; // fmod 对负数返回负值
    } else if(aPhase > 1.0f) {
        aPhase = 1.0f;
    }
}

float cANIM_BLEND_1D::Phase() const { return aPhase; }

void cANIM_BLEND_1D::SetPhase(float phase) {
    if(!std::isfinite(phase)) phase = 0.0f;
    if(phase < 0.0f) phase = 0.0f;
    if(phase >= 1.0f) phase = aLoop ? std::fmod(phase, 1.0f) : 1.0f;
    aPhase = phase;
}

bool cANIM_BLEND_1D::IsLooping() const { return aLoop; }

float cANIM_BLEND_1D::Duration() const {
    float best = 0.0f;
    for(size_t i = 0; i < aPoints.size(); ++i) {
        if(aPoints[i].aClip == 0) continue;
        if(aPoints[i].aClip->aDuration > best) best = aPoints[i].aClip->aDuration;
    }
    return best;
}

float cANIM_BLEND_1D::timeOf(const sANIM_CLIP *clip) const {
    if(clip == 0) return 0.0f;
    // 相位 → 该 clip 自己的时间。用各自 duration 换算 = "一个完整循环内相位对齐"，
    // 这正是 walk/run 混合时不出现"两条腿各走各的"的原因。
    float time = aPhase * clip->aDuration;
    if(time < 0.0f) time = 0.0f;
    if(time > clip->aDuration) time = clip->aDuration;
    return time;
}

bool cANIM_BLEND_1D::Sample(uint32_t node, sANIM_TRANSFORM &out) const {
    if(aPoints.empty()) return false;

    std::vector<float> weights;
    EvaluateWeights(aParameter, weights);

    // 先各自采样，只保留"该 clip 有该节点通道"的那些 —— 权重重新归一化。
    sANIM_TRANSFORM acc;
    float accWeight = 0.0f;
    bool any = false;
    for(size_t i = 0; i < aPoints.size() && i < weights.size(); ++i) {
        const float w = weights[i];
        if(!(w > 0.0f)) continue;
        const sANIM_CLIP *clip = aPoints[i].aClip;
        if(clip == 0) continue;
        sANIM_TRANSFORM local;
        if(!clip->sample(node, timeOf(clip), local)) continue; // 该 clip 没这个节点
        if(!transformIsFinite(local)) continue; // 数据损坏：跳过而不是污染累积值

        if(!any) {
            acc = local;
            accWeight = w;
            any = true;
            continue;
        }
        // 增量混合：把 local 按 w/(accWeight+w) 的比例混进 acc
        const float total = accWeight + w;
        const float t = total > 0.0f ? (w / total) : 0.0f;
        blendTransform(acc, local, t, acc);
        accWeight = total;
    }
    if(!any) return false;
    // accWeight 已经是"有通道的那些 clip 的权重和"，acc 内部按比例混合完成，
    // 所以不需要再除以 accWeight（除的话会把部分权重的情况放大成 1）。
    out = acc;
    return true;
}

// ═══════════════════════════════════════════════════════════
//  sANIM_LAYER
// ═══════════════════════════════════════════════════════════
sANIM_LAYER::sANIM_LAYER() : aSource(0), aWeight(1.0f) {}

float sANIM_LAYER::WeightAt(uint32_t joint) const {
    float w = aWeight;
    if(!std::isfinite(w)) w = 0.0f;
    if(w < 0.0f) w = 0.0f;
    if(w > 1.0f) w = 1.0f;
    if(aMask.empty()) return w; // 空遮罩 = 全关节
    if(joint >= aMask.size()) return 0.0f; // 超出遮罩长度的关节不受影响（保守）
    float m = aMask[joint];
    if(!std::isfinite(m)) m = 0.0f;
    if(m < 0.0f) m = 0.0f;
    if(m > 1.0f) m = 1.0f;
    return w * m;
}

// ═══════════════════════════════════════════════════════════
//  cANIM_LAYER_STACK
// ═══════════════════════════════════════════════════════════
cANIM_LAYER_STACK::cANIM_LAYER_STACK() {}

void cANIM_LAYER_STACK::Clear() { aLayers.clear(); }

bool cANIM_LAYER_STACK::SetBase(cANIM_BLEND_1D *source) {
    if(source == 0) return false;
    aLayers.clear();
    sANIM_LAYER base;
    base.aSource = source;
    base.aWeight = 1.0f; // 基础层永远是满权重
    aLayers.push_back(base);
    return true;
}

uint32_t cANIM_LAYER_STACK::AddLayer(cANIM_BLEND_1D *source, float weight, const float *mask,
                                     uint32_t maskCount) {
    if(source == 0) return 0;
    if(aLayers.empty()) return 0; // 必须先有基础层
    sANIM_LAYER layer;
    layer.aSource = source;
    layer.aWeight = std::isfinite(weight) ? std::max(0.0f, std::min(1.0f, weight)) : 0.0f;
    if(mask != 0 && maskCount > 0) layer.aMask.assign(mask, mask + maskCount);
    aLayers.push_back(layer);
    return (uint32_t)(aLayers.size() - 1);
}

uint32_t cANIM_LAYER_STACK::LayerCount() const { return (uint32_t)aLayers.size(); }

const sANIM_LAYER *cANIM_LAYER_STACK::LayerAt(uint32_t index) const {
    if(index >= aLayers.size()) return 0;
    return &aLayers[index];
}

sANIM_LAYER *cANIM_LAYER_STACK::LayerAt(uint32_t index) {
    if(index >= aLayers.size()) return 0;
    return &aLayers[index];
}

void cANIM_LAYER_STACK::Update(float dt) {
    for(size_t i = 0; i < aLayers.size(); ++i) {
        if(aLayers[i].aSource != 0) aLayers[i].aSource->Update(dt);
    }
}

bool cANIM_LAYER_STACK::Sample(uint32_t node, sANIM_TRANSFORM &out) const {
    if(aLayers.empty()) return false;

    // 基础层：直接取（权重恒为 1）
    bool any = false;
    if(aLayers[0].aSource != 0) {
        if(aLayers[0].aSource->Sample(node, out)) any = true;
    }

    // 覆盖层：逐层 lerp。**该层采不到该节点就跳过**（不是回 T-pose）。
    for(size_t i = 1; i < aLayers.size(); ++i) {
        const sANIM_LAYER &layer = aLayers[i];
        const float w = layer.WeightAt(node);
        if(!(w > 0.0f)) continue;
        if(layer.aSource == 0) continue;
        sANIM_TRANSFORM local;
        if(!layer.aSource->Sample(node, local)) continue; // 这层没这个节点
        if(!any) {
            // 基础层没有该节点：上层直接以自己的权重"接管"
            if(w >= 1.0f) {
                out = local;
            } else {
                // 部分权重时与单位变换混合？不 —— 与"没有基础值"混合没有意义，
                // 直接采用上层（否则会把关节拉到原点）。这条与 Sample 的
                // "缺失不当单位变换"口径一致。
                out = local;
            }
            any = true;
            continue;
        }
        sANIM_TRANSFORM blended;
        blendTransform(out, local, w, blended);
        out = blended;
    }
    return any;
}

} // namespace GN3d
