// GN_Entity.cpp — 实体与四套控制器（P1：接线壳）。
//
// 这一层刻意"薄"：每个方法要么转发给已验算法，要么维护一点点状态（上一帧 LOD、
// 调色板缓存、映射表）。P1 的验收标准是「行为不变 + 有真实消费者」，
// 不是「把四个控制器写完整」——完整版分别在 P2（GPU 蒙皮）、P3（状态机）、P4（插值）。

#include "GN_Entity.h"
#include "GN_AnimStateMachine.h"

#include <cmath>
#include <cstring>

namespace GN3d {

// ═══════════════════════════════════════════════════════════
//  cMESH_CONTROLLER
// ═══════════════════════════════════════════════════════════
cMESH_CONTROLLER::cMESH_CONTROLLER()
    : aModel(nullptr), aPolicy(), aMaxScreenErrorPx(1.0f), aCull(true), aSortTransparent(false),
      aTransparentOverride(false), aTransparentValue(false), aVisible(false) {}

void cMESH_CONTROLLER::SetModel(cMODEL *model) {
    if(aModel == model) return;
    aModel = model;
    aLevels.clear();
    aLevelsPrev.clear();
    aVisible = false;
}

cMODEL *cMESH_CONTROLLER::Model() const { return aModel; }

void cMESH_CONTROLLER::SetMaxScreenError(float pixels) {
    aMaxScreenErrorPx = pixels;
    aPolicy.aMaxScreenErrorPx = pixels; // 与策略保持同一来源
}

void cMESH_CONTROLLER::SetLodPolicy(const sLOD_POLICY &policy) {
    aPolicy = policy;
    aMaxScreenErrorPx = policy.aMaxScreenErrorPx; // 旧接口继续可读
}

const sLOD_POLICY &cMESH_CONTROLLER::LodPolicy() const { return aPolicy; }

bool cMESH_CONTROLLER::HasTransparentMaterial() const {
    if(aTransparentOverride) return aTransparentValue;
    if(aModel == nullptr) return false;
    for(uint32_t i = 0; i < aModel->aMeshCount; ++i) {
        const sMESH &mesh = aModel->aMeshes[i];
        if(mesh.aMaterial != nullptr && mesh.aMaterial->aAlphaMode == eALPHA_BLEND) return true;
    }
    return false;
}

void cMESH_CONTROLLER::SetTransparentOverride(bool enabled, bool transparent) {
    aTransparentOverride = enabled;
    aTransparentValue = transparent;
}

float cMESH_CONTROLLER::MaxScreenError() const { return aMaxScreenErrorPx; }

void cMESH_CONTROLLER::SetCullEnabled(bool enabled) { aCull = enabled; }

bool cMESH_CONTROLLER::CullEnabled() const { return aCull; }

void cMESH_CONTROLLER::SetSortTransparent(bool enabled) { aSortTransparent = enabled; }

bool cMESH_CONTROLLER::SortTransparent() const { return aSortTransparent; }

bool cMESH_CONTROLLER::Prepare(const sLOD_VIEW &view, const sFRUSTUM *frustum) {
    aVisible = false;
    if(aModel == nullptr) return false;

    // 世界 AABB 每帧刷新：模型可能被物理/动画改了变换（P4 起是常态）。
    if(!aModel->computeWorldAABB()) {
        aLevels.clear();
        return false;
    }
    if(aCull && frustum != nullptr && !aModel->isVisibleIn(*frustum)) {
        aLevels.clear();
        return false;
    }

    // 逐网格选级：selectLod 的 prevLevel 是"降档需额外余量"的防抖输入，
    // 所以必须把**上一帧**的级别传进去（selectLodForModel 的便捷版没有这个参数）。
    // 网格数变化时（换模型）对齐长度，首帧一律从 LOD0 起。
    if(aLevelsPrev.size() != aModel->aMeshCount) aLevelsPrev.assign(aModel->aMeshCount, 0u);
    aLevels.assign(aModel->aMeshCount, 0u);
    for(uint32_t i = 0; i < aModel->aMeshCount; ++i) {
        // P6：策略化选级（像素误差 + 屏幕占比）；与旧 selectLod 相比只会更精细，
        // 不会更粗 —— 所以既有的 lod.* 验收不受影响。
        aLevels[i] = selectLodWithPolicy(aModel->aMeshes[i], aModel->aWorldAABB, view, aPolicy,
                                        aLevelsPrev[i]);
    }
    aLevelsPrev = aLevels;
    aVisible = true;
    return true;
}

bool cMESH_CONTROLLER::Visible() const { return aVisible; }

uint32_t cMESH_CONTROLLER::LodLevel(uint32_t meshIndex) const {
    if(meshIndex >= aLevels.size()) return 0;
    return aLevels[meshIndex];
}

uint32_t cMESH_CONTROLLER::LodCount() const { return (uint32_t)aLevels.size(); }

void cMESH_CONTROLLER::Draw(const GNg::sVEC3 &cameraPos) const {
    if(aModel == nullptr || !aVisible) return;
    if(aSortTransparent) {
        aModel->DrawPBRSorted(cameraPos);
        return;
    }
    aModel->DrawPBRWithLod(aLevels.empty() ? nullptr : aLevels.data(),
                           (uint32_t)aLevels.size());
}

void cMESH_CONTROLLER::DrawShadow() const {
    if(aModel == nullptr || !aVisible) return;
    aModel->DrawShadow();
}

// ═══════════════════════════════════════════════════════════
//  cANIM_CONTROLLER
// ═══════════════════════════════════════════════════════════
cANIM_CONTROLLER::cANIM_CONTROLLER()
    : aPlayer(), aMachine(new cANIM_STATE_MACHINE()), aMachineActive(false) {}

cANIM_CONTROLLER::~cANIM_CONTROLLER() {}

bool cANIM_CONTROLLER::RegisterClip(const char *name, const sANIM_CLIP *clip) {
    if(name == nullptr || name[0] == '\0' || clip == nullptr) return false;
    for(size_t i = 0; i < aClips.size(); ++i) {
        if(aClips[i].aName == name) {
            aClips[i].aClip = clip; // 同名覆盖（重新导入资产时用）
            aMachine->RegisterClip(name, clip);
            return true;
        }
    }
    sCLIP_ENTRY entry;
    entry.aName = name;
    entry.aClip = clip;
    aClips.push_back(entry);
    aMachine->RegisterClip(name, clip); // 同一份表：状态机按名查 clip 时不会再有一份
    return true;
}

const cANIM_CONTROLLER::sCLIP_ENTRY *cANIM_CONTROLLER::find(const char *name) const {
    if(name == nullptr) return nullptr;
    for(size_t i = 0; i < aClips.size(); ++i) {
        if(aClips[i].aName == name) return &aClips[i];
    }
    return nullptr;
}

const sANIM_CLIP *cANIM_CONTROLLER::FindClip(const char *name) const {
    const sCLIP_ENTRY *entry = find(name);
    return entry ? entry->aClip : nullptr;
}

uint32_t cANIM_CONTROLLER::ClipCount() const { return (uint32_t)aClips.size(); }

const char *cANIM_CONTROLLER::ClipName(uint32_t index) const {
    if(index >= aClips.size()) return "";
    return aClips[index].aName.c_str();
}

bool cANIM_CONTROLLER::Play(const char *name, bool loop, float speed) {
    const sCLIP_ENTRY *entry = find(name);
    if(entry == nullptr) return false;
    aPlayer.play(entry->aClip, loop, speed);
    aCurrent = entry->aName;
    return true;
}

bool cANIM_CONTROLLER::CrossFade(const char *name, float duration, bool loop) {
    const sCLIP_ENTRY *entry = find(name);
    if(entry == nullptr) return false;
    aPlayer.crossFade(entry->aClip, duration, loop);
    aCurrent = entry->aName;
    return true;
}

void cANIM_CONTROLLER::Stop() {
    // 手工 Stop 同时退出状态驱动：否则状态机会在下一帧把播放器重新拉起来，
    // "停下了却又在播"是最难解释的那类行为。
    if(aMachineActive) aMachine->Stop();
    aMachineActive = false;
    aPlayer.stop();
    aCurrent.clear();
}

void cANIM_CONTROLLER::Update(float dt) {
    // cANIM_PLAYER::update 自己会处理非法 dt；这里只为可读性显式挡一次。
    if(!(dt > 0.0f) || !std::isfinite(dt)) return;
    aPlayer.update(dt);
}

cANIM_STATE_MACHINE &cANIM_CONTROLLER::StateMachine() { return *aMachine; }

const cANIM_STATE_MACHINE &cANIM_CONTROLLER::StateMachine() const { return *aMachine; }

bool cANIM_CONTROLLER::StartStateMachine() {
    aMachineActive = aMachine->Start(aPlayer);
    aCurrent.clear(); // 之后 Current() 报状态机那边的 clip 名
    return aMachineActive;
}

bool cANIM_CONTROLLER::UpdateWithStateMachine(float dt, sANIM_PARAMS &params) {
    if(!aMachineActive) return false;
    return aMachine->Update(dt, params);
}

bool cANIM_CONTROLLER::StateMachineActive() const { return aMachineActive; }

bool cANIM_CONTROLLER::Playing() const { return aPlayer.isPlaying(); }

bool cANIM_CONTROLLER::Sample(uint32_t node, sANIM_TRANSFORM &out) const {
    return aPlayer.sample(node, out);
}

const char *cANIM_CONTROLLER::Current() const {
    if(aMachineActive) {
        const char *clipName = aMachine->CurrentClipName();
        if(clipName != nullptr && clipName[0] != '\0') return clipName;
    }
    return aCurrent.c_str();
}

float cANIM_CONTROLLER::Time() const { return aPlayer.time(); }

float cANIM_CONTROLLER::BlendWeight() const { return aPlayer.blendWeight(); }

// ═══════════════════════════════════════════════════════════
//  cSKELETON_CONTROLLER
// ═══════════════════════════════════════════════════════════
cSKELETON_CONTROLLER::cSKELETON_CONTROLLER() : aSkeleton(nullptr), aValid(false) {}

void cSKELETON_CONTROLLER::SetSkeleton(const sSKELETON *skeleton) {
    if(aSkeleton == skeleton) return;
    aSkeleton = skeleton;
    aWorld.clear();
    aPalette.clear();
    aValid = false;
}

const sSKELETON *cSKELETON_CONTROLLER::Skeleton() const { return aSkeleton; }

uint32_t cSKELETON_CONTROLLER::JointCount() const {
    return aSkeleton ? aSkeleton->jointCount() : 0u;
}

std::vector<sANIM_TRANSFORM> cSKELETON_CONTROLLER::BindPoseLocal() const {
    std::vector<sANIM_TRANSFORM> local;
    if(aSkeleton == nullptr) return local;
    local.resize(aSkeleton->aJoints.size());
    for(size_t i = 0; i < aSkeleton->aJoints.size(); ++i) {
        const sJOINT &joint = aSkeleton->aJoints[i];
        local[i].aTranslation = joint.aBindTranslation;
        local[i].aRotation = joint.aBindRotation;
        local[i].aScale = joint.aBindScale;
    }
    return local;
}

bool cSKELETON_CONTROLLER::ComputePalette(const std::vector<sANIM_TRANSFORM> &local) {
    if(aSkeleton == nullptr || local.size() != aSkeleton->aJoints.size()) return false;
    std::vector<GNg::sMAT44> world;
    std::vector<GNg::sMAT44> palette;
    if(!aSkeleton->computeWorld(local, world)) return false;
    if(!aSkeleton->computePalette(local, palette)) return false;
    aWorld.swap(world);
    aPalette.swap(palette);
    aValid = true;
    return true;
}

bool cSKELETON_CONTROLLER::PaletteValid() const { return aValid; }

const std::vector<GNg::sMAT44> &cSKELETON_CONTROLLER::Palette() const { return aPalette; }

const std::vector<GNg::sMAT44> &cSKELETON_CONTROLLER::World() const { return aWorld; }

GNg::sVEC3 cSKELETON_CONTROLLER::JointWorldPosition(uint32_t joint) const {
    GNg::sVEC3 origin = {0.0f, 0.0f, 0.0f};
    if(joint >= aWorld.size()) return origin;
    // 列主序：平移在 m[12..14]
    const GNg::sMAT44 &m = aWorld[joint];
    origin.x = m.m[12];
    origin.y = m.m[13];
    origin.z = m.m[14];
    return origin;
}

eSKIN_PALETTE_PATH cSKELETON_CONTROLLER::PalettePath() const {
    // 骨架关节数决定容量需求（不是"已算出的 palette 大小"）：换骨架后即使还没算过
    // palette，也应该报出"将走哪条路径"，便于调用方提前告警（例如 5000 关节的资产）。
    return selectSkinPalettePath(JointCount());
}

uint32_t cSKELETON_CONTROLLER::PackPalette(sSKIN_UBO &out) const {
    if(!aValid || aPalette.empty()) {
        std::memset(&out, 0, sizeof(out));
        return 0;
    }
    return packPalette(aPalette.data(), (uint32_t)aPalette.size(), out);
}

// ═══════════════════════════════════════════════════════════
//  cPHYSICS_CONTROLLER
// ═══════════════════════════════════════════════════════════
cPHYSICS_CONTROLLER::sSNAPSHOT::sSNAPSHOT() : aPos{0.0f, 0.0f, 0.0f}, aRot(), aValid(false) {}

cPHYSICS_CONTROLLER::sCONTACT_EVENT::sCONTACT_EVENT()
    : aEntityA(0), aEntityB(0), aNormal{0.0f, 1.0f, 0.0f}, aPoint{0.0f, 0.0f, 0.0f},
      aPenetration(0.0f), aEntered(false) {}

cPHYSICS_CONTROLLER::cPHYSICS_CONTROLLER()
    : aWorld(nullptr), aRenderAlpha(0.0f), aLastSubSteps(0), aReportUnmapped(false) {}

void cPHYSICS_CONTROLLER::SetWorld(cPHYSICS_WORLD *world) {
    if(aWorld == world) return;
    aWorld = world;
    ClearBindings(); // 世界换掉了，旧句柄一律失效
}

cPHYSICS_WORLD *cPHYSICS_CONTROLLER::World() const { return aWorld; }

bool cPHYSICS_CONTROLLER::AttachBody(uint32_t entityId, uint32_t handle) {
    if(aWorld == nullptr || handle == cPHYSICS_WORLD::INVALID_HANDLE) return false;
    if(entityId == 0) return false; // 0 保留为"未分配编号"
    for(size_t i = 0; i < aEntityIds.size(); ++i) {
        if(aEntityIds[i] == entityId) {
            aHandles[i] = handle;
            return true;
        }
    }
    aEntityIds.push_back(entityId);
    aHandles.push_back(handle);
    // 新登记的快照立即对齐当前状态：否则第一次 ApplyTransform 会把模型插到原点。
    const sRIGID_BODY *body = aWorld->GetBody(handle);
    sSNAPSHOT snapshot;
    if(body != nullptr) {
        snapshot.aPos = body->aPos;
        snapshot.aRot = body->aRot;
        snapshot.aValid = true;
    }
    aPrev.push_back(snapshot);
    aCurr.push_back(snapshot);
    return true;
}

bool cPHYSICS_CONTROLLER::DetachBody(uint32_t entityId) {
    for(size_t i = 0; i < aEntityIds.size(); ++i) {
        if(aEntityIds[i] != entityId) continue;
        aEntityIds.erase(aEntityIds.begin() + (long)i);
        aHandles.erase(aHandles.begin() + (long)i);
        aPrev.erase(aPrev.begin() + (long)i);
        aCurr.erase(aCurr.begin() + (long)i);
        // 解绑后该实体不再出现在接触事件里：把上一帧 pair 表里含它的项一起清掉，
        // 否则下一帧会报一条"离开"，调用方可能对已经不存在的实体做处理。
        const uint64_t prefix = ((uint64_t)entityId << 32);
        std::vector<uint64_t> kept;
        kept.reserve(aPrevPairKeys.size());
        for(size_t k = 0; k < aPrevPairKeys.size(); ++k) {
            const uint64_t key = aPrevPairKeys[k];
            const uint32_t lo = (uint32_t)(key >> 32);
            const uint32_t hi = (uint32_t)(key & 0xFFFFFFFFu);
            if(lo == entityId || hi == entityId || (key & prefix) == prefix) continue;
            kept.push_back(key);
        }
        aPrevPairKeys.swap(kept);
        return true;
    }
    return false;
}

void cPHYSICS_CONTROLLER::ClearBindings() {
    aEntityIds.clear();
    aHandles.clear();
    aPrev.clear();
    aCurr.clear();
    aContacts.clear();
    aPrevPairKeys.clear();
    aExited.clear();
    aLastSubSteps = 0;
    aRenderAlpha = 0.0f;
}

uint32_t cPHYSICS_CONTROLLER::BodyOf(uint32_t entityId) const {
    for(size_t i = 0; i < aEntityIds.size(); ++i) {
        if(aEntityIds[i] == entityId) return aHandles[i];
    }
    return cPHYSICS_WORLD::INVALID_HANDLE;
}

int32_t cPHYSICS_CONTROLLER::EntityOf(uint32_t handle) const {
    for(size_t i = 0; i < aHandles.size(); ++i) {
        if(aHandles[i] == handle) return (int32_t)aEntityIds[i];
    }
    return -1;
}

uint32_t cPHYSICS_CONTROLLER::BindingCount() const { return (uint32_t)aEntityIds.size(); }

cPHYSICS_CONTROLLER::sSNAPSHOT cPHYSICS_CONTROLLER::captureBody(uint32_t handle) const {
    sSNAPSHOT snapshot;
    if(aWorld == nullptr) return snapshot;
    const sRIGID_BODY *body = aWorld->GetBody(handle);
    if(body == nullptr) return snapshot; // aValid = false
    snapshot.aPos = body->aPos;
    snapshot.aRot = body->aRot;
    snapshot.aValid = true;
    return snapshot;
}

void cPHYSICS_CONTROLLER::Step(float dt, float renderAlpha) {
    aLastSubSteps = 0;
    if(aWorld == nullptr) {
        aRenderAlpha = 0.0f;
        return;
    }
    // 1) 记下"子步之前"的状态（还没提交：如果这一帧没跑子步，快照就不该动）
    std::vector<sSNAPSHOT> before;
    before.reserve(aHandles.size());
    for(size_t i = 0; i < aHandles.size(); ++i) before.push_back(captureBody(aHandles[i]));

    // 2) 推进世界
    aWorld->Step(dt);
    aLastSubSteps = aWorld->GetLastSubStepCount();

    // 3) 只有真的跑了子步，才把 (前, 后) 提交成 (prev, curr)。
    //    没跑子步的帧保持旧快照，靠 alpha 在旧的一段里继续走 —— 这样 144Hz 渲染
    //    在 60Hz 物理下才每帧都有推进（见头文件里的三条口径）。
    if(aLastSubSteps > 0) {
        for(size_t i = 0; i < aHandles.size(); ++i) {
            aPrev[i] = before[i];
            aCurr[i] = captureBody(aHandles[i]);
        }
    }

    // 4) 钳插值因子（不按子步数折算）
    aRenderAlpha = renderAlpha;
    if(!(aRenderAlpha >= 0.0f)) aRenderAlpha = 0.0f;
    if(aRenderAlpha > 1.0f) aRenderAlpha = 1.0f;

    // 5) 接触事件
    refreshContacts();
}

float cPHYSICS_CONTROLLER::RenderAlpha() const { return aRenderAlpha; }

uint32_t cPHYSICS_CONTROLLER::LastSubSteps() const { return aLastSubSteps; }

bool cPHYSICS_CONTROLLER::ApplyTransform(uint32_t entityId, cMODEL &model) const {
    if(aWorld == nullptr) return false;
    const uint32_t handle = BodyOf(entityId);
    if(handle == cPHYSICS_WORLD::INVALID_HANDLE) return false;
    const sRIGID_BODY *body = aWorld->GetBody(handle);
    if(body == nullptr) return false;
    model.SetPosition(body->aPos.x, body->aPos.y, body->aPos.z);
    model.SetRotation(body->aRot);
    return true;
}

bool cPHYSICS_CONTROLLER::ApplyTransformInterpolated(uint32_t entityId, cMODEL &model) const {
    if(aWorld == nullptr) return false;
    const uint32_t handle = BodyOf(entityId);
    if(handle == cPHYSICS_WORLD::INVALID_HANDLE) return false;
    const sRIGID_BODY *body = aWorld->GetBody(handle);
    if(body == nullptr) return false;

    // 找到该实体的快照槽位
    size_t slot = aPrev.size();
    for(size_t i = 0; i < aEntityIds.size(); ++i) {
        if(aEntityIds[i] == entityId) {
            slot = i;
            break;
        }
    }
    // 没有快照（未登记）/ 快照无效（刚体刚被移除）：退回最新状态，绝不用零值插值。
    if(slot >= aPrev.size() || !aPrev[slot].aValid || !aCurr[slot].aValid) {
        model.SetPosition(body->aPos.x, body->aPos.y, body->aPos.z);
        model.SetRotation(body->aRot);
        return true;
    }

    const sSNAPSHOT &from = aPrev[slot];
    const sSNAPSHOT &to = aCurr[slot];
    const float t = aRenderAlpha;
    const GNg::sVEC3 position = {from.aPos.x + (to.aPos.x - from.aPos.x) * t,
                                 from.aPos.y + (to.aPos.y - from.aPos.y) * t,
                                 from.aPos.z + (to.aPos.z - from.aPos.z) * t};
    GNg::sQUAT rotation;
    GNg::QuatSlerp(&rotation, &from.aRot, &to.aRot, t);
    model.SetPosition(position.x, position.y, position.z);
    model.SetRotation(rotation);
    return true;
}

uint64_t cPHYSICS_CONTROLLER::pairKey(uint32_t entityA, uint32_t entityB) {
    return ((uint64_t)entityA << 32) | (uint64_t)entityB;
}

bool cPHYSICS_CONTROLLER::pairIsLatched(uint32_t entityA, uint32_t entityB) const {
    // 「两端都睡着就保持接触」：物理引擎为了性能会跳过睡着的刚体对，于是
    // "球躺在地上"从某一帧起就查不到接触了。但游戏语义里这显然是持续接触，
    // 所以这里对睡着的一对做 latch（不产生 enter/exit 抖动）。
    bool anyMapped = false;
    uint32_t entities[2] = {entityA, entityB};
    for(int side = 0; side < 2; ++side) {
        const uint32_t entity = entities[side];
        if(entity == 0) continue; // 无实体的一端（地面/三角网）不影响判断
        anyMapped = true;
        const uint32_t handle = BodyOf(entity);
        if(handle == cPHYSICS_WORLD::INVALID_HANDLE) continue; // 已解绑：不靠它 latch
        const sRIGID_BODY *body = aWorld->GetBody(handle);
        if(body == nullptr) continue; // 刚体已移除：不 latch（下面会报 exit）
        if(!body->aSleeping) return false; // 有醒着的：以世界真实接触为准
    }
    return anyMapped;
}

void cPHYSICS_CONTROLLER::refreshContacts() {
    aContacts.clear();
    aExited.clear();
    if(aWorld == nullptr) return;

    std::vector<uint64_t> currentKeys;

    // 1) 世界当前报的接触
    const uint32_t count = aWorld->GetContactCount();
    for(uint32_t i = 0; i < count; ++i) {
        cPHYSICS_WORLD::sCONTACT_INFO info;
        if(!aWorld->GetContactInfo(i, info)) continue;
        const int32_t entityLo = (info.aBodyLo == cPHYSICS_WORLD::INVALID_HANDLE)
                                     ? -1
                                     : EntityOf(info.aBodyLo);
        const int32_t entityHi = EntityOf(info.aBodyHi);
        // 没有实体的一端（地面/三角网）记 0；两端都没实体时整条跳过（不是"实体间的接触"）。
        const uint32_t a = (entityLo > 0) ? (uint32_t)entityLo : 0u;
        const uint32_t b = (entityHi > 0) ? (uint32_t)entityHi : 0u;
        if(a == 0 && b == 0) continue;
        if(a == 0 || b == 0) {
            if(!aReportUnmapped) continue;
        }

        // 规范化成 (小, 大)：同一对不论谁先谁后都算同一条，避免 enter/exit 抖动
        const uint64_t key = pairKey(a <= b ? a : b, a <= b ? b : a);
        bool seen = false;
        for(size_t k = 0; k < aPrevPairKeys.size(); ++k) {
            if(aPrevPairKeys[k] == key) {
                seen = true;
                break;
            }
        }
        bool duplicated = false;
        for(size_t k = 0; k < currentKeys.size(); ++k) {
            if(currentKeys[k] == key) {
                duplicated = true;
                break;
            }
        }
        // 同一对可能有多个接触点/流形：只登记一次，但事件带上（第一个）明细
        if(duplicated) continue;

        sCONTACT_EVENT event;
        event.aEntityA = a;
        event.aEntityB = b;
        event.aNormal = info.aNormal;
        event.aPoint = info.aPoint;
        event.aPenetration = info.aPenetration;
        event.aEntered = !seen;
        currentKeys.push_back(key);
        aContacts.push_back(event);
    }

    // 2) 上一帧有、本帧世界没报的：睡着的一对保持（latch），其余报 exit
    for(size_t k = 0; k < aPrevPairKeys.size(); ++k) {
        const uint64_t key = aPrevPairKeys[k];
        bool stillThere = false;
        for(size_t n = 0; n < currentKeys.size(); ++n) {
            if(currentKeys[n] == key) {
                stillThere = true;
                break;
            }
        }
        if(stillThere) continue;

        const uint32_t entityA = (uint32_t)(key >> 32);
        const uint32_t entityB = (uint32_t)(key & 0xFFFFFFFFu);
        if(pairIsLatched(entityA, entityB)) {
            // 睡着：接触保持，不产生 enter/exit
            currentKeys.push_back(key);
            sCONTACT_EVENT event;
            event.aEntityA = entityA;
            event.aEntityB = entityB;
            event.aEntered = false;
            aContacts.push_back(event);
            continue;
        }
        sPAIR pair;
        pair.aEntityA = entityA;
        pair.aEntityB = entityB;
        aExited.push_back(pair);
    }

    aPrevPairKeys.swap(currentKeys);
}

uint32_t cPHYSICS_CONTROLLER::ContactCount() const { return (uint32_t)aContacts.size(); }

const cPHYSICS_CONTROLLER::sCONTACT_EVENT &cPHYSICS_CONTROLLER::ContactAt(uint32_t index) const {
    static const sCONTACT_EVENT kEmpty;
    if(index >= aContacts.size()) return kEmpty;
    return aContacts[index];
}

uint32_t cPHYSICS_CONTROLLER::EnteredContactCount() const {
    uint32_t entered = 0;
    for(size_t i = 0; i < aContacts.size(); ++i) {
        if(aContacts[i].aEntered) ++entered;
    }
    return entered;
}

uint32_t cPHYSICS_CONTROLLER::ExitedContactCount() const { return (uint32_t)aExited.size(); }

bool cPHYSICS_CONTROLLER::ExitedContactAt(uint32_t index, uint32_t &entityA,
                                         uint32_t &entityB) const {
    if(index >= aExited.size()) return false;
    entityA = aExited[index].aEntityA;
    entityB = aExited[index].aEntityB;
    return true;
}

void cPHYSICS_CONTROLLER::SetReportUnmapped(bool report) { aReportUnmapped = report; }

bool cPHYSICS_CONTROLLER::ReportUnmapped() const { return aReportUnmapped; }

// ═══════════════════════════════════════════════════════════
//  cENTITY
// ═══════════════════════════════════════════════════════════
cENTITY::cENTITY() : aId(0), bHasLocalPose(false), aRenderAlpha(0.0f) {}

void cENTITY::SetLocalPose(const std::vector<sANIM_TRANSFORM> &local) {
    aLocalPose = local;
    bHasLocalPose = !local.empty();
}

const std::vector<sANIM_TRANSFORM> &cENTITY::LocalPose() const { return aLocalPose; }

void cENTITY::Update(float dt) {
    // 固定顺序：物理 → 动画 → 骨骼（plan §2）。
    // 1) 物理：根位移/朝向先定，后面的动画采样才拿得到本帧位置。
    aPhysics.Step(dt, aRenderAlpha);
    if(aId != 0) {
        cMODEL *model = aMesh.Model();
        // 物理 → 模型用快照插值：物理可能只在这一帧跑了 0 步或 3 步，
        // 直接取最新位置会看到"每两帧跳一次"的台阶（见 cPHYSICS_CONTROLLER 的注释）。
        if(model != nullptr) aPhysics.ApplyTransformInterpolated(aId, *model);
    }

    // 2) 动画：状态机优先（有则按参数转换并驱动播放器），否则走手工播放的时间轴。
    if(aAnim.StateMachineActive()) {
        aAnim.UpdateWithStateMachine(dt, aParams);
    } else {
        aAnim.Update(dt);
    }
    const uint32_t joints = aSkeleton.JointCount();
    if(joints > 0 && aAnim.Playing()) {
        // 以绑定姿势为底，再覆盖有通道的关节：clip 通常只驱动一部分关节，
        // 其余关节必须留在绑定姿势上（写零会得到退化的全零矩阵）。
        std::vector<sANIM_TRANSFORM> local = aSkeleton.BindPoseLocal();
        if(local.size() != joints) local.assign(joints, sANIM_TRANSFORM());
        bool any = false;
        for(uint32_t node = 0; node < joints; ++node) {
            sANIM_TRANSFORM transform;
            if(aAnim.Sample(node, transform)) {
                local[node] = transform;
                any = true;
            }
        }
        // 全部关节都没有通道 = 这段 clip 与骨架不匹配：保持旧姿势，不要把零姿势写进去
        // （零姿势会把模型压成一个点，比"不动"难查得多）。
        if(any) SetLocalPose(local);
    }

    // 3) 骨骼：局部姿势 → world → palette。
    if(bHasLocalPose) aSkeleton.ComputePalette(aLocalPose);
}

bool cENTITY::PrepareRender(const sLOD_VIEW &view, const sFRUSTUM *frustum) {
    return aMesh.Prepare(view, frustum);
}

bool cENTITY::Visible() const { return aMesh.Visible(); }

void cENTITY::Draw(const GNg::sVEC3 &cameraPos) const { aMesh.Draw(cameraPos); }

void cENTITY::DrawShadow() const { aMesh.DrawShadow(); }

} // namespace GN3d
