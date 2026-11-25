// tests/imoScene/imoSceneOutfit.cpp — 换装接线：规则表 + 按键映射 + 写回 cMODEL。
//
// 为什么按名字而不是材质：一套装饰（如猫耳+铃铛+尾巴）跨多个网格、共用一个材质，
// 按材质切换会把同材质的其它部件一起藏掉。名字匹配对 glTF 导出的命名稳定。
//
// 判定逻辑不在这里：`GN3d::cVISIBILITY_SET`（src/graphics/GN_Visibility.h）负责
// 三类名字匹配、三来源 AND 合成、mask/变体族的对称清空；本文件只提供规则描述、
// 把按键翻译成操作、以及把结果写进 sMESH::aVisible。

#include "imoSceneOutfit.h"

#include "imoSceneActions.h"

#include "GN_Visibility.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace imoScene {

namespace {

// ═══════════════════════════════════════════════════════════
//  一、规则表（纯数据）
// ═══════════════════════════════════════════════════════════
// 组 = 可整体开关的一批网格。SUBSTRING 比 PREFIX 更合适：装饰名散落在各个前缀下
// （CatEar / CatBell / CatTail），没有共同的词头。
//
// 注意 `"ribbon"` 只写了一条小写：匹配已统一为**大小写不敏感**，
// 不再需要旧实现那样同时写 "Ribbon" 和 "ribbon" 两条。
const GN3d::sNAME_RULE kCatRules[] = {
    {GN3d::eNAME_SUBSTRING, "CatEar"},
    {GN3d::eNAME_SUBSTRING, "CatBell"},
    {GN3d::eNAME_SUBSTRING, "CatTail"},
};
const GN3d::sNAME_RULE kHatRules[] = {
    {GN3d::eNAME_SUBSTRING, "Hat"},
};
const GN3d::sNAME_RULE kMaskRules[] = {
    {GN3d::eNAME_SUBSTRING, "PleatedMask"},
};
// 丝带/领结/项圈：同一类装饰的三种叫法。
const GN3d::sNAME_RULE kRibbonRules[] = {
    {GN3d::eNAME_SUBSTRING, "Ribbon"},
    {GN3d::eNAME_SUBSTRING, "Collar"},
};

struct sGROUP_BINDING {
    GN::eACTION aAction; // 哪个键切它
    GN3d::sHIDE_GROUP_DESC aDesc;
};

const sGROUP_BINDING kGroupBindings[] = {
    {eIMO_ACT_OUTFIT_CAT, GN3d::sHIDE_GROUP_DESC("cat（猫耳/铃铛/尾巴）", kCatRules, 3, true)},
    {eIMO_ACT_OUTFIT_HAT, GN3d::sHIDE_GROUP_DESC("hat（帽子）", kHatRules, 1, true)},
    {eIMO_ACT_OUTFIT_MASK, GN3d::sHIDE_GROUP_DESC("mask（口罩）", kMaskRules, 1, true)},
    {eIMO_ACT_OUTFIT_RIBBON, GN3d::sHIDE_GROUP_DESC("ribbon（丝带/领结/项圈）", kRibbonRules, 2, true)},
};
const uint32_t kGroupCount = sizeof(kGroupBindings) / sizeof(kGroupBindings[0]);

// 变体族 = 同一槽位的互斥变体。用 PREFIX：`Mesh_Hair.001` 属于发型族。
const GN3d::sVARIANT_FAMILY_DESC kFamilyDescs[] = {
    GN3d::sVARIANT_FAMILY_DESC("hair（发型）", GN3d::sNAME_RULE(GN3d::eNAME_PREFIX, "Mesh_Hair")),
    GN3d::sVARIANT_FAMILY_DESC("body（身体/服装）",
                               GN3d::sNAME_RULE(GN3d::eNAME_PREFIX, "Mesh_Body")),
};
const uint32_t kFamilyCount = sizeof(kFamilyDescs) / sizeof(kFamilyDescs[0]);

// 族下标常量：0 = 发型、1 = 身体（与 kFamilyDescs 顺序一致）
const uint32_t kFamilyHair = 0;
const uint32_t kFamilyBody = 1;

// ═══════════════════════════════════════════════════════════
//  二、状态
// ═══════════════════════════════════════════════════════════
// 只剩模型指针（写回目标）；显隐状态全在 aVisibility 里，可被替换/复制。
GN3d::cMODEL *gModel = nullptr;
GN3d::cVISIBILITY_SET gVisibility;
uint32_t gVisibleMeshCount = 0; // 缓存的可见数（HUD 每帧读）

// 把规则集的结果刷进 cMODEL。**这是本文件唯一写 aVisible 的地方。**
void applyToModel() {
    if (gModel == nullptr) {
        gVisibleMeshCount = 0;
        return;
    }
    uint32_t visible = 0;
    // 网格数取两边较小值：规则集是按同一份名字建的，但不做假设。
    const uint32_t count = (gModel->aMeshCount < gVisibility.MeshCount())
                               ? gModel->aMeshCount
                               : gVisibility.MeshCount();
    for (uint32_t meshIndex = 0; meshIndex < count; ++meshIndex) {
        const bool shown = gVisibility.IsVisible(meshIndex);
        gModel->aMeshes[meshIndex].aVisible = shown;
        if (shown) {
            ++visible;
        }
    }
    // 规则集之外的网格（理论上不该有）保持导入时的可见性，并计入统计。
    for (uint32_t meshIndex = count; meshIndex < gModel->aMeshCount; ++meshIndex) {
        if (gModel->aMeshes[meshIndex].aVisible) {
            ++visible;
        }
    }
    gVisibleMeshCount = visible;
}

// 把默认名单套到规则集上：名单里给的是**槽位名**（不带 #prim），按基名精确匹配。
// 只作用于身体/衣服 —— 装饰仍归 1–4 键管（方案 A 的约定）。
uint32_t applyDefaultHidden(const std::vector<std::string> &hidden) {
    uint32_t changed = 0;
    for (size_t item = 0; item < hidden.size(); ++item) {
        changed += gVisibility.SetMaskByName(hidden[item].c_str(), true);
    }
    return changed;
}

} // namespace

// ═══════════════════════════════════════════════════════════
//  对外接口
// ═══════════════════════════════════════════════════════════

void outfitInit(GN3d::cMODEL &model, const char *defaultPath) {
    gModel = &model;

    // ── 1. 收集网格名（规则集要一份 const char* 数组；Build 内部会复制）──
    std::vector<const char *> names(model.aMeshCount);
    for (uint32_t meshIndex = 0; meshIndex < model.aMeshCount; ++meshIndex) {
        names[meshIndex] = model.aMeshes[meshIndex].aName;
    }

    // ── 2. 建规则集 ──
    GN3d::sHIDE_GROUP_DESC groupDescs[kGroupCount];
    for (uint32_t group = 0; group < kGroupCount; ++group) {
        groupDescs[group] = kGroupBindings[group].aDesc;
    }
    GN3d::sVISIBILITY_DESC desc;
    desc.aGroups = groupDescs;
    desc.aGroupCount = kGroupCount;
    desc.aFamilies = kFamilyDescs;
    desc.aFamilyCount = kFamilyCount;
    if (!gVisibility.Build(names.empty() ? 0 : names.data(), model.aMeshCount, desc)) {
        std::printf("[imoScene] 换装规则非法，全部网格保持可见\n");
        for (uint32_t meshIndex = 0; meshIndex < model.aMeshCount; ++meshIndex) {
            model.aMeshes[meshIndex].aVisible = true;
        }
        gVisibleMeshCount = model.aMeshCount;
        return;
    }

    // ── 3. 默认服装组合（可选）──
    const char *path = defaultPath;
    if (path == 0) {
        path = std::getenv("IMO_SCENE_OUTFIT_DEFAULT");
    }
    const char *fallback = ".local/imoScene/outfitDefault.json";
    sOUTFIT_DEFAULT defaults;
    if (parseOutfitDefaultFile(path != 0 ? path : fallback, defaults)) {
        const uint32_t hidden = applyDefaultHidden(defaults.aHidden);
        std::printf("[imoScene] 默认服装组合: 隐藏 %u 个网格（%zu 条名单）\n", hidden,
                    defaults.aHidden.size());
    }

    // ── 4. 默认选中哪套变体：环境变量优先，其次 JSON 的 hair 字段 ──
    int32_t hair = -1;
    int32_t body = -1;
    if (parseIntegerEnv("IMO_SCENE_HAIR", hair)) {
        gVisibility.SetFamilySelection(kFamilyHair, hair);
    } else if (defaults.aHair >= 0) {
        gVisibility.SetFamilySelection(kFamilyHair, defaults.aHair);
    }
    if (parseIntegerEnv("IMO_SCENE_BODY", body)) {
        gVisibility.SetFamilySelection(kFamilyBody, body);
    }

    // ── 5. 写回并报告 ──
    applyToModel();
    std::printf("[imoScene] 换装: 网格 %u / 可见 %u；发型 %u 套、身体 %u 套"
                "（1–4 切装饰，5/6 循环变体，0 恢复全部）\n",
                gVisibility.MeshCount(), gVisibleMeshCount, gVisibility.FamilySize(kFamilyHair),
                gVisibility.FamilySize(kFamilyBody));
}

void outfitUpdate() {
    if (gModel == nullptr) {
        return;
    }
    bool changed = false;
    for (uint32_t group = 0; group < kGroupCount; ++group) {
        if (gInput.Pressed(kGroupBindings[group].aAction)) {
            changed = gVisibility.ToggleGroup(group) || changed;
        }
    }
    if (gInput.Pressed(eIMO_ACT_OUTFIT_HAIR)) {
        changed = gVisibility.CycleFamily(kFamilyHair) || changed;
    }
    if (gInput.Pressed(eIMO_ACT_OUTFIT_BODY)) {
        changed = gVisibility.CycleFamily(kFamilyBody) || changed;
    }
    if (gInput.Pressed(eIMO_ACT_OUTFIT_RESET)) {
        gVisibility.ResetAll();
        changed = true;
    }
    if (changed) {
        applyToModel();
    }
}

uint32_t outfitVisibleMeshCount() { return gVisibleMeshCount; }

const GN3d::cVISIBILITY_SET &outfitVisibility() { return gVisibility; }

} // namespace imoScene
