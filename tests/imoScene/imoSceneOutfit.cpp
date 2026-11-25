// tests/imoScene/imoSceneOutfit.cpp — 换装：按网格名分组做显隐切换。
//
// 为什么按名字而不是材质：一套装饰（如猫耳+铃铛+尾巴）跨多个网格、共用一个材质，
// 按材质切换会把同材质的其它部件一起藏掉。名字匹配对 glTF 导出的命名稳定。
#include "imoSceneOutfit.h"

#include "imoSceneActions.h"

#include <cstdio>
#include <cstring>
#include <vector>

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

// 变体族：导入资产里同名网格（Mesh_Hair / Mesh_Hair.001 …、Mesh_Body / Mesh_Body.001 …）
// 是同一槽位的不同套发型/服装，游戏里同一时刻只激活一套。默认 -1 = 全部显示（保持导入画面），
// 键 5 / 6 循环切换，切到末尾回到"全部"。
struct sVARIANT_FAMILY {
    const char *aPrefix;
    std::vector<uint32_t> aMeshes;
    int32_t aSelected; // -1 = 全部
};

sVARIANT_FAMILY kFamilies[] = {
    {"Mesh_Hair", std::vector<uint32_t>(), -1},
    {"Mesh_Body", std::vector<uint32_t>(), -1},
};
const uint32_t kFamilyCount = sizeof(kFamilies) / sizeof(kFamilies[0]);

bool variantFamilyVisible(uint32_t meshIndex) {
    for (uint32_t family = 0; family < kFamilyCount; ++family) {
        const sVARIANT_FAMILY &slot = kFamilies[family];
        if (slot.aSelected < 0) {
            continue;
        }
        for (size_t item = 0; item < slot.aMeshes.size(); ++item) {
            if (slot.aMeshes[item] != meshIndex) {
                continue;
            }
            if (static_cast<int32_t>(item) != slot.aSelected) {
                return false;
            }
            break;
        }
    }
    return true;
}

// 名称前缀命中即属于该族（名字形如 Mesh_Hair、Mesh_Hair.001）。
bool nameHasPrefix(const char *name, const char *prefix) {
    return std::strncmp(name, prefix, std::strlen(prefix)) == 0;
}

void applyGroups();

void buildFamilies() {
    for (uint32_t family = 0; family < kFamilyCount; ++family) {
        kFamilies[family].aMeshes.clear();
        // 发型默认只显示第一套（9 套发型叠在一起会明显"头发过厚"）；身体/服装默认全显示。
        kFamilies[family].aSelected = (family == 0) ? 1 : -1;
    }
    if (gModel == nullptr) {
        return;
    }
    for (uint32_t meshIndex = 0; meshIndex < gModel->aMeshCount; ++meshIndex) {
        const char *name = gModel->aMeshes[meshIndex].aName;
        for (uint32_t family = 0; family < kFamilyCount; ++family) {
            if (nameHasPrefix(name, kFamilies[family].aPrefix)) {
                kFamilies[family].aMeshes.push_back(meshIndex);
                break;
            }
        }
    }
}

void cycleFamily(uint32_t family) {
    if (family >= kFamilyCount) {
        return;
    }
    sVARIANT_FAMILY &slot = kFamilies[family];
    if (slot.aMeshes.empty()) {
        return;
    }
    if (slot.aSelected + 1 >= static_cast<int32_t>(slot.aMeshes.size())) {
        slot.aSelected = -1; // 回到"全部显示"
    } else {
        ++slot.aSelected;
    }
    applyGroups();
}

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
        mesh.aVisible = visible && variantFamilyVisible(meshIndex);
    }
}

} // namespace

void outfitInit(GN3d::cMODEL &model) {
    gModel = &model;
    for (uint32_t groupIndex = 0; groupIndex < kGroupCount; ++groupIndex) {
        kGroups[groupIndex].aVisible = true;
    }
    buildFamilies();
    // 家族 -1 = 全部；把默认选中的那套换算成下标（0 = 显示第一套）。
    for (uint32_t family = 0; family < kFamilyCount; ++family) {
        if (!kFamilies[family].aMeshes.empty()) {
            kFamilies[family].aSelected = (family == 0) ? 0 : -1;
        }
    }
    std::printf("[imoScene] 换装: 发型 %zu 套 / 身体 %zu 套（5/6 循环切换，0 恢复全部）\n",
                kFamilies[0].aMeshes.size(), kFamilies[1].aMeshes.size());
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
    if (gInput.Pressed(eIMO_ACT_OUTFIT_HAIR)) {
        cycleFamily(0);
        changed = false; // cycleFamily 内部已刷新可见性
    }
    if (gInput.Pressed(eIMO_ACT_OUTFIT_BODY)) {
        cycleFamily(1);
        changed = false;
    }
    if (gInput.Pressed(eIMO_ACT_OUTFIT_RESET)) {
        for (uint32_t groupIndex = 0; groupIndex < kGroupCount; ++groupIndex) {
            kGroups[groupIndex].aVisible = true;
        }
        for (uint32_t family = 0; family < kFamilyCount; ++family) {
            kFamilies[family].aSelected = -1;
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