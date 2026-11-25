// tests/imoScene/imoSceneOutfit.cpp — 换装：按网格名分组做显隐切换。
//
// 为什么按名字而不是材质：一套装饰（如猫耳+铃铛+尾巴）跨多个网格、共用一个材质，
// 按材质切换会把同材质的其它部件一起藏掉。名字匹配对 glTF 导出的命名稳定。
#include "imoSceneOutfit.h"

#include "imoSceneActions.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
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
// 默认服装组合（方案 A）：从游戏 prefab 的激活槽位导出的"只保留这一套"名单，
// 只作用于身体/衣服（Mesh_Body*），装饰仍归 1–4 键管。空 = 不启用。
std::vector<unsigned char> gForceHidden;
std::vector<std::string> gHiddenNames;

// 从 JSON 里取出 "hidden": [ "..." ] 名单（文件很简单，按引号切分足够）。
bool loadOutfitDefault(const char *path) {
    std::FILE *file = std::fopen(path, "rb");
    if (file == 0) {
        return false;
    }
    std::string text;
    char buffer[4096];
    while (true) {
        const size_t read = std::fread(buffer, 1, sizeof(buffer), file);
        if (read == 0) {
            break;
        }
        text.append(buffer, read);
    }
    std::fclose(file);
    const size_t key = text.find("\"hidden\"");
    if (key == std::string::npos) {
        return false;
    }
    const size_t begin = text.find('[', key);
    const size_t end = text.find(']', key);
    if (begin == std::string::npos || end == std::string::npos || end < begin) {
        return false;
    }
    gHiddenNames.clear();
    size_t cursor = begin;
    while (true) {
        const size_t quote = text.find('"', cursor);
        if (quote == std::string::npos || quote > end) {
            break;
        }
        const size_t close = text.find('"', quote + 1);
        if (close == std::string::npos || close > end) {
            break;
        }
        gHiddenNames.push_back(text.substr(quote + 1, close - quote - 1));
        cursor = close + 1;
    }
    return !gHiddenNames.empty();
}

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
        kFamilies[family].aSelected = -1; // 默认全部显示（9 套头发里有头皮/刘海等零件，只留一套会露出光头）
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
        bool forced = false;
        if (meshIndex < gForceHidden.size() && gForceHidden[meshIndex] != 0) {
            forced = true;
        }
        mesh.aVisible = visible && !forced && variantFamilyVisible(meshIndex);
    }
}

} // namespace

void outfitInit(GN3d::cMODEL &model) {
    gModel = &model;
    for (uint32_t groupIndex = 0; groupIndex < kGroupCount; ++groupIndex) {
        kGroups[groupIndex].aVisible = true;
    }
    buildFamilies();
    // 方案 A：默认服装组合。只隐藏 Mesh_Body*（身体/衣服），装饰留给 1–4 键。
    gForceHidden.assign(gModel->aMeshCount, 0);
    const char *defaultPath = std::getenv("IMO_SCENE_OUTFIT_DEFAULT");
    const std::string fallback = ".local/imoScene/outfitDefault.json";
    if (loadOutfitDefault(defaultPath != 0 ? defaultPath : fallback.c_str())) {
        size_t hiddenBody = 0;
        for (uint32_t meshIndex = 0; meshIndex < gModel->aMeshCount; ++meshIndex) {
            const std::string name = gModel->aMeshes[meshIndex].aName;
            if (name.compare(0, 9, "Mesh_Body") != 0) {
                continue;
            }
            // 网格名可能带 "#prim" 后缀；按去掉后缀的节点名精确比较（否则 Mesh_Body 会命中所有 Mesh_Body.*）。
            const size_t hash = name.find('#');
            const std::string base = (hash == std::string::npos) ? name : name.substr(0, hash);
            for (size_t item = 0; item < gHiddenNames.size(); ++item) {
                if (base == gHiddenNames[item]) {
                    gForceHidden[meshIndex] = 1;
                    ++hiddenBody;
                    break;
                }
            }
        }
        std::printf("[imoScene] 默认服装组合: 隐藏身体网格 %zu 个（.local/imoScene/outfitDefault.json）\n",
                    hiddenBody);
    }
    // 默认显示集合可用环境变量固定（便于挑好之后固化）；-1 = 全部。
    const char *hairEnv = std::getenv("IMO_SCENE_HAIR");
    const char *bodyEnv = std::getenv("IMO_SCENE_BODY");
    if (hairEnv != 0) {
        kFamilies[0].aSelected = static_cast<int32_t>(std::atoi(hairEnv));
        if (kFamilies[0].aSelected >= static_cast<int32_t>(kFamilies[0].aMeshes.size())) {
            kFamilies[0].aSelected = -1;
        }
    }
    if (bodyEnv != 0) {
        kFamilies[1].aSelected = static_cast<int32_t>(std::atoi(bodyEnv));
        if (kFamilies[1].aSelected >= static_cast<int32_t>(kFamilies[1].aMeshes.size())) {
            kFamilies[1].aSelected = -1;
        }
    }
    std::printf("[imoScene] 换装: 发型 %zu 套 / 身体 %zu 套（5/6 循环切换，0 恢复全部；"
                "默认 hair=%d body=%d）\n",
                kFamilies[0].aMeshes.size(), kFamilies[1].aMeshes.size(),
                kFamilies[0].aSelected, kFamilies[1].aSelected);
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