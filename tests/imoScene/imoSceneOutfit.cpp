// tests/imoScene/imoSceneOutfit.cpp — 换装：按网格名分组做显隐切换。
//
// 为什么按名字而不是材质：一套装饰（如猫耳+铃铛+尾巴）跨多个网格、共用一个材质，
// 按材质切换会把同材质的其它部件一起藏掉。名字匹配对 glTF 导出的命名稳定。
#include "imoSceneOutfit.h"

#include "imoSceneActions.h"

#include <cstring>

namespace imoScene {

namespace {

// 每个装饰组最多匹配 3 个名字子串；aVisible 是组状态（true = 显示）。
struct sOUTFIT_GROUP {
    GN::eACTION aAction;
    const char *aNeedle[3];
    bool aVisible;
};

sOUTFIT_GROUP kGroups[] = {
    {eIMO_ACT_OUTFIT_CAT, {"CatEar", "CatBell", "CatTail"}, true},
    {eIMO_ACT_OUTFIT_HAT, {"Hat", 0, 0}, true},
    {eIMO_ACT_OUTFIT_MASK, {"PleatedMask", 0, 0}, true},
    {eIMO_ACT_OUTFIT_RIBBON, {"Ribbon", "ribbon", "Collar"}, true},
};

const uint32_t kGroupCount = sizeof(kGroups) / sizeof(kGroups[0]);

GN3d::cMODEL *gModel = nullptr;

bool matchesGroup(const sOUTFIT_GROUP &group, const char *meshName) {
    for (int index = 0; index < 3; ++index) {
        const char *needle = group.aNeedle[index];
        if (needle != 0 && std::strstr(meshName, needle) != 0) {
            return true;
        }
    }
    return false;
}

// 按各组当前状态重算每个网格的可见性（未被任何隐藏组命中的网格始终可见）。
void applyGroups() {
    if (gModel == nullptr) {
        return;
    }
    for (uint32_t meshIndex = 0; meshIndex < gModel->aMeshCount; ++meshIndex) {
        GN3d::sMESH &mesh = gModel->aMeshes[meshIndex];
        bool visible = true;
        for (uint32_t groupIndex = 0; groupIndex < kGroupCount; ++groupIndex) {
            if (!kGroups[groupIndex].aVisible &&
                matchesGroup(kGroups[groupIndex], mesh.aName)) {
                visible = false;
                break;
            }
        }
        mesh.aVisible = visible;
    }
}

} // namespace

void outfitInit(GN3d::cMODEL &model) {
    gModel = &model;
    for (uint32_t groupIndex = 0; groupIndex < kGroupCount; ++groupIndex) {
        kGroups[groupIndex].aVisible = true;
    }
    applyGroups();
}

void outfitUpdate() {
    if (gModel == nullptr) {
        return;
    }
    bool changed = false;
    for (uint32_t groupIndex = 0; groupIndex < kGroupCount; ++groupIndex) {
        if (gInput.Pressed(kGroups[groupIndex].aAction)) {
            kGroups[groupIndex].aVisible = !kGroups[groupIndex].aVisible;
            changed = true;
        }
    }
    if (gInput.Pressed(eIMO_ACT_OUTFIT_RESET)) {
        for (uint32_t groupIndex = 0; groupIndex < kGroupCount; ++groupIndex) {
            kGroups[groupIndex].aVisible = true;
        }
        changed = true;
    }
    if (changed) {
        applyGroups();
    }
}

uint32_t outfitVisibleMeshCount() {
    if (gModel == nullptr) {
        return 0;
    }
    uint32_t count = 0;
    for (uint32_t meshIndex = 0; meshIndex < gModel->aMeshCount; ++meshIndex) {
        if (gModel->aMeshes[meshIndex].aVisible) {
            ++count;
        }
    }
    return count;
}

} // namespace imoScene