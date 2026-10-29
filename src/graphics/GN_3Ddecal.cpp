// GN_3Ddecal.cpp — 3D 地面贴花：固定容量环形池、寿命推进、末段渐隐、渲染条目转换。
//
// 确定性：写入顺序由环形游标决定（池满覆盖最老），同参数 + 同 spawn/update 序列结果完全一致。
// 本文件不依赖 Vulkan；渲染侧（_Vulkan/vkDecal.cpp）只消费 sDECAL_RENDER_ITEM。
#include <GN.h>
#include "GN_3Ddecal.h"

#include <cmath>

namespace GN3d {

namespace {

const float kDecalMaxDt = 0.1f;
const float kDecalFadeFraction = 0.3f; // 最后 30% 寿命用于渐隐

bool finiteVec(const GNg::sVEC3 &v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

} // namespace

cDECAL_SYSTEM::cDECAL_SYSTEM() : aCapacity(0), aActiveCount(0), aCursor(0) {}

cDECAL_SYSTEM::~cDECAL_SYSTEM() {}

bool cDECAL_SYSTEM::create(uint32_t capacity) {
    if(capacity == 0 || isCreated()) return false;
    aDecals.assign(capacity, sDECAL());
    aAlive.assign(capacity, 0);
    aCapacity = capacity;
    aActiveCount = 0;
    aCursor = 0;
    return true;
}

void cDECAL_SYSTEM::clear() {
    if(aDecals.empty()) return;
    aAlive.assign(aDecals.size(), 0);
    aActiveCount = 0;
    aCursor = 0;
}

bool cDECAL_SYSTEM::spawn(const sDECAL &decal) {
    if(!isCreated()) return false;
    if(!(decal.aSize > 0.0f) || !std::isfinite(decal.aSize)) return false;
    if(!finiteVec(decal.aPosition)) return false;
    if(!std::isfinite(decal.aColor.w) || decal.aColor.w < 0.0f || decal.aColor.w > 1.0f) {
        return false;
    }
    if(!std::isfinite(decal.aLife) || decal.aLife < 0.0f) return false;

    // 环形写：池满时覆盖最老槽位（该槽已有的贴花直接被替换）。
    const uint32_t slot = aCursor % aCapacity;
    if(!aAlive[slot]) ++aActiveCount;
    aDecals[slot] = decal;
    aDecals[slot].aAge = 0.0f;
    aAlive[slot] = 1;
    aCursor = (aCursor + 1u) % aCapacity;
    return true;
}

void cDECAL_SYSTEM::update(float dt) {
    if(aDecals.empty() || !(dt > 0.0f) || !std::isfinite(dt)) return;
    if(dt > kDecalMaxDt) dt = kDecalMaxDt;
    for(uint32_t i = 0; i < aCapacity; ++i) {
        if(!aAlive[i]) continue;
        sDECAL &decal = aDecals[i];
        decal.aAge += dt;
        if(decal.aLife > 0.0f && decal.aAge >= decal.aLife) recycleSlot(i);
    }
}

void cDECAL_SYSTEM::recycleSlot(uint32_t index) {
    if(index >= aCapacity || !aAlive[index]) return;
    aAlive[index] = 0;
    if(aActiveCount > 0) --aActiveCount;
}

float cDECAL_SYSTEM::decalFadeAlpha(uint32_t index) const {
    if(!alive(index)) return 0.0f;
    const sDECAL &decal = aDecals[index];
    if(!(decal.aLife > 0.0f)) return decal.aColor.w; // 永久贴花
    const float fadeStart = decal.aLife * (1.0f - kDecalFadeFraction);
    if(decal.aAge <= fadeStart) return decal.aColor.w;
    const float remaining = decal.aLife - decal.aAge;
    if(remaining <= 0.0f) return 0.0f;
    const float fadeSpan = decal.aLife * kDecalFadeFraction;
    if(!(fadeSpan > 0.0f)) return 0.0f;
    return decal.aColor.w * (remaining / fadeSpan);
}

uint32_t gatherDecalRenderItems(const cDECAL_SYSTEM &system, sDECAL_RENDER_ITEM *out,
                                uint32_t maxOut) {
    if(out == 0 || maxOut == 0 || !system.isCreated()) return 0;
    const sDECAL *decals = system.decals();
    if(decals == 0) return 0;
    uint32_t written = 0;
    for(uint32_t i = 0; i < system.capacity() && written < maxOut; ++i) {
        if(!system.alive(i)) continue;
        const sDECAL &decal = decals[i];
        const float alpha = system.decalFadeAlpha(i);
        if(!(alpha > 0.0f)) continue; // 已完全淡出的贴花不再产生绘制条目
        sDECAL_RENDER_ITEM &item = out[written];
        item.aPosition[0] = decal.aPosition.x;
        item.aPosition[1] = decal.aPosition.y;
        item.aPosition[2] = decal.aPosition.z;
        item.aSize = decal.aSize;
        item.aColor[0] = decal.aColor.x;
        item.aColor[1] = decal.aColor.y;
        item.aColor[2] = decal.aColor.z;
        item.aColor[3] = alpha;
        item.aKind = decal.aKind;
        ++written;
    }
    return written;
}

} // namespace GN3d
