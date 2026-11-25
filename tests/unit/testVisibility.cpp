// testVisibility.cpp — 网格显隐规则集（换装 / 部件开关 / 变体切换）。
//
// 验收重点，按重要性排序：
//
//   1. **跨实例隔离**。抽这个模块的直接动机就是"两个角色不能同时换装"
//      （旧实现是 4 个文件级全局变量）。所以第一条用例就是两个实例各改各的、
//      互不影响 —— 这是"可复用"这件事唯一有说服力的证明。
//
//   2. **三类匹配语义必须不同且各自正确**。旧实现里三种规则并存且都不一致
//      （大小写敏感 strstr / 前缀 / 精确），猫耳组不得不写 "Ribbon" 和 "ribbon"
//      两条才算对。现在统一成大小写不敏感，但 EXACT / PREFIX / SUBSTRING 的
//      区别必须钉死：写错一条就会误伤别的网格。
//
//   3. **mask 与变体族必须对称地"交回手动控制"**。旧实现只有 ToggleGroup 清 mask、
//      CycleFamily 不清，于是"默认组合 mask 掉的服装，按 6 切换身体没反应"。
//      这条是真实缺陷，用用例钉住修复。
#include "testFramework.h"

#include <GN.h>
#include "GN_Visibility.h"

#include <cstring>
#include <string>
#include <vector>

using namespace GN3d;

namespace {

// ── 一套贴近真实 imoScene 的规则与名字 ─────────────────────
// 名字取自实际 glTF 节点命名习惯：Mesh_Hair / Mesh_Hair.001 / Mesh_Body#1 …
const char *gNames[] = {
    "Mesh_Hair",       // 0  发型 0
    "Mesh_Hair.001",   // 1  发型 1
    "Mesh_Hair.002",   // 2  发型 2
    "Mesh_Body",       // 3  身体 0
    "Mesh_Body.001",   // 4  身体 1
    "CatEar",          // 5  装饰：猫耳
    "CatBell",         // 6  装饰：铃铛
    "Ribbon",          // 7  装饰：丝带
    "PleatedMask",     // 8  装饰：口罩
    "Hat",             // 9  装饰：帽子
    "RoomWall",        // 10 房间（不属于任何组/族）
};
const uint32_t gNameCount = sizeof(gNames) / sizeof(gNames[0]);

const sNAME_RULE kCatRules[] = {{eNAME_SUBSTRING, "CatEar"}, {eNAME_SUBSTRING, "CatBell"}};
const sNAME_RULE kHatRules[] = {{eNAME_PREFIX, "Hat"}};
const sNAME_RULE kMaskRules[] = {{eNAME_SUBSTRING, "Mask"}};
// 只写一条 "ribbon"：统一成大小写不敏感后，它就能命中 "Ribbon"。
// 旧实现需要 "Ribbon" + "ribbon" 两条，这条用例是那个问题的回归守卫。
const sNAME_RULE kRibbonRules[] = {{eNAME_SUBSTRING, "ribbon"}};

const sHIDE_GROUP_DESC kGroups[] = {
    {"cat", kCatRules, 2, true},
    {"hat", kHatRules, 1, true},
    {"mask", kMaskRules, 1, true},
    {"ribbon", kRibbonRules, 1, true},
};
const uint32_t kGroupCount = sizeof(kGroups) / sizeof(kGroups[0]);

const sVARIANT_FAMILY_DESC kFamilies[] = {
    {"hair", sNAME_RULE(eNAME_PREFIX, "Mesh_Hair")},
    {"body", sNAME_RULE(eNAME_PREFIX, "Mesh_Body")},
};
const uint32_t kFamilyCount = sizeof(kFamilies) / sizeof(kFamilies[0]);

sVISIBILITY_DESC makeDesc() {
    sVISIBILITY_DESC desc;
    desc.aGroups = kGroups;
    desc.aGroupCount = kGroupCount;
    desc.aFamilies = kFamilies;
    desc.aFamilyCount = kFamilyCount;
    return desc;
}

void buildDefault(cVISIBILITY_SET &set) {
    CHECK(set.Build(gNames, gNameCount, makeDesc()));
}

// 便捷查询：按名字问可见性（名字重复时取第一个）。
bool visibleByName(const cVISIBILITY_SET &set, const char *name) {
    for (uint32_t i = 0; i < set.MeshCount(); ++i) {
        if (std::strcmp(set.NameOf(i), name) == 0) {
            return set.IsVisible(i);
        }
    }
    return false;
}

} // namespace

// ═══════════════════════════════════════════════════════════
//  一、名字匹配
// ═══════════════════════════════════════════════════════════

TEST_CASE(visNameRuleThreeKindsDiffer) {
    // EXACT：只命中自己
    CHECK(matchNameRule("Mesh_Body", sNAME_RULE(eNAME_EXACT_BASE, "Mesh_Body")));
    CHECK_EQ(matchNameRule("Mesh_Body.001", sNAME_RULE(eNAME_EXACT_BASE, "Mesh_Body")), false);
    // PREFIX：命中自己和所有派生
    CHECK(matchNameRule("Mesh_Body", sNAME_RULE(eNAME_PREFIX, "Mesh_Body")));
    CHECK(matchNameRule("Mesh_Body.001", sNAME_RULE(eNAME_PREFIX, "Mesh_Body")));
    CHECK_EQ(matchNameRule("Mesh_BodyArmor", sNAME_RULE(eNAME_EXACT_BASE, "Mesh_Body")), false);
    // SUBSTRING：命中任何包含它的名字
    CHECK(matchNameRule("Mesh_BodyArmor", sNAME_RULE(eNAME_SUBSTRING, "Body")));
    CHECK_EQ(matchNameRule("Mesh_Hair", sNAME_RULE(eNAME_SUBSTRING, "Body")), false);
}

TEST_CASE(visNameRuleIsCaseInsensitive) {
    // 旧实现的装饰组匹配大小写敏感，猫耳组不得不写两条 needle；
    // 统一成不敏感后一条就够 —— 这里两种写法都必须命中。
    CHECK(matchNameRule("Ribbon", sNAME_RULE(eNAME_SUBSTRING, "ribbon")));
    CHECK(matchNameRule("ribbon", sNAME_RULE(eNAME_SUBSTRING, "RIBBON")));
    CHECK(matchNameRule("Mesh_Hair", sNAME_RULE(eNAME_PREFIX, "mesh_hair")));
    CHECK(matchNameRule("Mesh_Hair", sNAME_RULE(eNAME_EXACT_BASE, "MESH_HAIR")));
}

TEST_CASE(visNameRuleIgnoresPrimSuffix) {
    // 一个 glTF 节点拆多个 primitive 时会加 "#1" 后缀；三类规则都按基名判。
    CHECK(matchNameRule("Mesh_Body#1", sNAME_RULE(eNAME_EXACT_BASE, "Mesh_Body")));
    CHECK(matchNameRule("Mesh_Body#1", sNAME_RULE(eNAME_PREFIX, "Mesh_Body")));
    CHECK(matchNameRule("Clothes_imo_chan#0", sNAME_RULE(eNAME_EXACT_BASE, "Clothes_imo_chan")));
    // 不带后缀的名字不能被误判成带后缀的那个
    CHECK_EQ(matchNameRule("Mesh_Body", sNAME_RULE(eNAME_EXACT_BASE, "Mesh_Body#1")), false);
}

TEST_CASE(visNameRuleRejectsEmptyAndNull) {
    // 空规则不命中任何东西（避免"某条规则忘了填"变成"全部命中"这种灾害）。
    CHECK_EQ(matchNameRule("Mesh_Body", sNAME_RULE(eNAME_SUBSTRING, "")), false);
    CHECK_EQ(matchNameRule("Mesh_Body", sNAME_RULE(eNAME_PREFIX, 0)), false);
    CHECK_EQ(matchNameRule(0, sNAME_RULE(eNAME_SUBSTRING, "Body")), false);
    // 非法枚举值保守不命中
    CHECK_EQ(matchNameRule("Mesh_Body", sNAME_RULE(static_cast<eNAME_MATCH>(99), "Body")), false);
}

// ═══════════════════════════════════════════════════════════
//  二、初始状态与成员归属
// ═══════════════════════════════════════════════════════════

TEST_CASE(visBuildStartsAllVisible) {
    cVISIBILITY_SET set;
    buildDefault(set);
    CHECK_EQ(set.MeshCount(), gNameCount);
    CHECK_EQ(set.VisibleCount(), gNameCount);
    CHECK_EQ(set.GroupCount(), kGroupCount);
    CHECK_EQ(set.FamilyCount(), kFamilyCount);
    for (uint32_t i = 0; i < gNameCount; ++i) {
        CHECK(set.IsVisible(i));
    }
}

TEST_CASE(visFamilyMembershipIsExclusiveAndOrdered) {
    cVISIBILITY_SET set;
    buildDefault(set);
    // 发型族 3 套、身体族 2 套
    CHECK_EQ(set.FamilySize(0), 3u);
    CHECK_EQ(set.FamilySize(1), 2u);
    CHECK_EQ(set.FamilySelection(0), -1); // 默认全部
    // "Mesh_Hair" 同时是 hair 族的前缀… 但不会进 body 族
    // （用一个更直接的判据：切到 body 第 0 套后，发型仍全部可见）
    CHECK(set.SetFamilySelection(1, 0));
    CHECK(visibleByName(set, "Mesh_Body"));
    CHECK_EQ(visibleByName(set, "Mesh_Body.001"), false);
    CHECK(visibleByName(set, "Mesh_Hair"));
    CHECK(visibleByName(set, "Mesh_Hair.001"));
}

TEST_CASE(visGroupMembershipCanOverlap) {
    // 一个网格可以属于多个组：只要有一组被关掉就隐藏。
    // 造一个"猫耳"同时被 "cat" 和 "hat" 两条规则命中（模拟资产命名混乱）。
    const char *names[] = {"CatEarHat"};
    const sNAME_RULE catRule[] = {{eNAME_SUBSTRING, "CatEar"}};
    const sNAME_RULE hatRule[] = {{eNAME_SUBSTRING, "Hat"}};
    const sHIDE_GROUP_DESC groups[] = {
        {"cat", catRule, 1, true},
        {"hat", hatRule, 1, true},
    };
    sVISIBILITY_DESC desc;
    desc.aGroups = groups;
    desc.aGroupCount = 2;
    cVISIBILITY_SET set;
    CHECK(set.Build(names, 1, desc));
    CHECK(set.IsVisible(0));
    CHECK(set.ToggleGroup(0)); // 关掉 cat
    CHECK_EQ(set.IsVisible(0), false);
    // 关掉 hat 也不行：两组都被关，仍然隐藏
    CHECK(set.ToggleGroup(1));
    CHECK_EQ(set.IsVisible(0), false);
    // 只开回 cat 仍然隐藏（hat 还关着）—— 这就是"重叠组必须全部可见"的证明
    CHECK(set.ToggleGroup(0));
    CHECK_EQ(set.IsVisible(0), false);
    CHECK(set.ToggleGroup(1));
    CHECK(set.IsVisible(0));
}

// ═══════════════════════════════════════════════════════════
//  三、组开关
// ═══════════════════════════════════════════════════════════

TEST_CASE(visToggleGroupHidesOnlyItsMembers) {
    cVISIBILITY_SET set;
    buildDefault(set);
    CHECK(set.ToggleGroup(0)); // cat
    CHECK_EQ(set.GroupVisible(0), false);
    CHECK_EQ(visibleByName(set, "CatEar"), false);
    CHECK_EQ(visibleByName(set, "CatBell"), false);
    // 不沾边的网格一个都不能少
    CHECK(visibleByName(set, "Mesh_Hair"));
    CHECK(visibleByName(set, "Ribbon"));
    CHECK(visibleByName(set, "RoomWall"));
    CHECK_EQ(set.VisibleCount(), gNameCount - 2);
    // 再切回来
    CHECK(set.ToggleGroup(0));
    CHECK_EQ(set.VisibleCount(), gNameCount);
}

TEST_CASE(visToggleGroupRejectsOutOfRange) {
    cVISIBILITY_SET set;
    buildDefault(set);
    CHECK_EQ(set.ToggleGroup(kGroupCount), false);
    CHECK_EQ(set.GroupVisible(kGroupCount), true); // 越界当作"没有这个组"
    CHECK_EQ(set.GroupKey(kGroupCount)[0], '\0');
}

TEST_CASE(visRibbonGroupMatchesWithSingleLowercaseRule) {
    // 回归守卫：旧实现要写 "Ribbon" + "ribbon" 两条，单条小写会漏掉大写资产。
    cVISIBILITY_SET set;
    buildDefault(set);
    CHECK(set.ToggleGroup(3)); // ribbon 组
    CHECK_EQ(visibleByName(set, "Ribbon"), false); // 资产名是大写开头
}

// ═══════════════════════════════════════════════════════════
//  四、变体族循环
// ═══════════════════════════════════════════════════════════

TEST_CASE(visCycleFamilyWrapsThroughAllThenBack) {
    cVISIBILITY_SET set;
    buildDefault(set);
    // 全部 → 0 → 1 → 2 → 全部
    CHECK_EQ(set.FamilySelection(0), -1);
    CHECK(set.CycleFamily(0));
    CHECK_EQ(set.FamilySelection(0), 0);
    CHECK(visibleByName(set, "Mesh_Hair"));
    CHECK_EQ(visibleByName(set, "Mesh_Hair.001"), false);
    CHECK_EQ(visibleByName(set, "Mesh_Hair.002"), false);

    CHECK(set.CycleFamily(0));
    CHECK_EQ(set.FamilySelection(0), 1);
    CHECK_EQ(visibleByName(set, "Mesh_Hair"), false);
    CHECK(visibleByName(set, "Mesh_Hair.001"));

    CHECK(set.CycleFamily(0));
    CHECK_EQ(set.FamilySelection(0), 2);
    CHECK(visibleByName(set, "Mesh_Hair.002"));

    CHECK(set.CycleFamily(0)); // 回绕到"全部"
    CHECK_EQ(set.FamilySelection(0), -1);
    CHECK_EQ(set.VisibleCount(), gNameCount);
}

TEST_CASE(visCycleFamilyRejectsEmptyAndOutOfRange) {
    const char *names[] = {"RoomWall"};
    const sVARIANT_FAMILY_DESC families[] = {{"hair", sNAME_RULE(eNAME_PREFIX, "Mesh_Hair")}};
    sVISIBILITY_DESC desc;
    desc.aFamilies = families;
    desc.aFamilyCount = 1;
    cVISIBILITY_SET set;
    CHECK(set.Build(names, 1, desc));
    CHECK_EQ(set.FamilySize(0), 0u); // 一个都没命中
    CHECK_EQ(set.CycleFamily(0), false); // 空族不能切
    CHECK_EQ(set.CycleFamily(1), false); // 越界
    CHECK_EQ(set.FamilySelection(1), -1);
    CHECK_EQ(set.FamilyKey(1)[0], '\0');
}

TEST_CASE(visSetFamilySelectionValidatesSlot) {
    cVISIBILITY_SET set;
    buildDefault(set);
    CHECK(set.SetFamilySelection(0, 1));
    CHECK_EQ(set.FamilySelection(0), 1);
    CHECK_EQ(set.SetFamilySelection(0, 3), false); // 只有 3 套（0..2）
    CHECK_EQ(set.FamilySelection(0), 1); // 失败不改状态
    CHECK(set.SetFamilySelection(0, -1)); // 负数 = 全部
    CHECK_EQ(set.FamilySelection(0), -1);
}

// ═══════════════════════════════════════════════════════════
//  五、mask
// ═══════════════════════════════════════════════════════════

TEST_CASE(visMaskByNameHidesExactBase) {
    cVISIBILITY_SET set;
    buildDefault(set);
    // 配置里写的是槽位名（不带 #prim），按基名精确匹配
    CHECK_EQ(set.SetMaskByName("Mesh_Body.001", true), 1u);
    CHECK_EQ(set.MaskedCount(), 1u);
    CHECK(visibleByName(set, "Mesh_Body"));
    CHECK_EQ(visibleByName(set, "Mesh_Body.001"), false);
    // 幂等：再设一次不重复计数
    CHECK_EQ(set.SetMaskByName("Mesh_Body.001", true), 0u);
    CHECK_EQ(set.MaskedCount(), 1u);
    // 清除
    CHECK_EQ(set.SetMaskByName("Mesh_Body.001", false), 1u);
    CHECK_EQ(set.MaskedCount(), 0u);
    CHECK_EQ(set.VisibleCount(), gNameCount);
}

TEST_CASE(visMaskByNameIgnoresHashSuffixInConfig) {
    // 配置给 "Mesh_Body"，网格叫 "Mesh_Body#1" —— 必须命中（按基名）。
    const char *names[] = {"Mesh_Body#0", "Mesh_Body#1", "Mesh_Hair"};
    cVISIBILITY_SET set;
    CHECK(set.Build(names, 3, makeDesc()));
    CHECK_EQ(set.SetMaskByName("Mesh_Body", true), 2u); // 两个 primitive 都中
    CHECK_EQ(set.IsVisible(0), false);
    CHECK_EQ(set.IsVisible(1), false);
    CHECK(set.IsVisible(2));
}

TEST_CASE(visMaskByRuleIsMoreGeneral) {
    cVISIBILITY_SET set;
    buildDefault(set);
    // 用前缀规则 mask 掉整个发型族（外部名单可能是"按槽位"给的一批名字）
    CHECK_EQ(set.SetMaskByRule(sNAME_RULE(eNAME_PREFIX, "Mesh_Hair"), true), 3u);
    CHECK_EQ(visibleByName(set, "Mesh_Hair"), false);
    CHECK_EQ(visibleByName(set, "Mesh_Hair.001"), false);
    CHECK_EQ(visibleByName(set, "Mesh_Hair.002"), false);
    CHECK(visibleByName(set, "Mesh_Body"));
    set.ClearMask();
    CHECK_EQ(set.MaskedCount(), 0u);
}

TEST_CASE(visMaskByNameRejectsEmpty) {
    cVISIBILITY_SET set;
    buildDefault(set);
    CHECK_EQ(set.SetMaskByName(0, true), 0u);
    CHECK_EQ(set.SetMaskByName("", true), 0u);
    CHECK_EQ(set.SetMaskByName("NotExist", true), 0u);
    CHECK_EQ(set.MaskedCount(), 0u);
}

// ═══════════════════════════════════════════════════════════
//  六、用户操作与 mask 的对称性（真实缺陷的回归守卫）
// ═══════════════════════════════════════════════════════════

TEST_CASE(visCycleFamilyClearsMaskOfItsMembers) {
    // 旧实现：CycleFamily 不清 mask。于是"默认组合把不穿的衣服 mask 掉"之后，
    // 按 6 切换身体完全没反应（切了但那些网格仍被 mask 挡住）。
    cVISIBILITY_SET set;
    buildDefault(set);
    set.SetMaskByName("Mesh_Body", true);
    set.SetMaskByName("Mesh_Body.001", true);
    CHECK_EQ(set.MaskedCount(), 2u);
    CHECK_EQ(set.VisibleCount(), gNameCount - 2);

    CHECK(set.CycleFamily(1)); // 切到身体第 0 套
    CHECK_EQ(set.MaskedCount(), 0u); // 该族成员的 mask 被清掉
    CHECK_EQ(set.FamilySelection(1), 0);
    CHECK(visibleByName(set, "Mesh_Body")); // 现在真的能看见了
    CHECK_EQ(visibleByName(set, "Mesh_Body.001"), false); // 被族排除
}

TEST_CASE(visToggleGroupClearsMaskOfItsMembers) {
    cVISIBILITY_SET set;
    buildDefault(set);
    set.SetMaskByName("CatEar", true);
    CHECK_EQ(set.VisibleCount(), gNameCount - 1);
    CHECK(set.ToggleGroup(0)); // 关掉 cat 组
    // 组隐藏 + mask 清掉：仍然不可见（组说了算），但 mask 已交回手动
    CHECK_EQ(visibleByName(set, "CatEar"), false);
    CHECK_EQ(set.Explain(set.MeshCount() > 5 ? 5u : 0u).aHiddenByMask, false);
    CHECK(set.ToggleGroup(0)); // 再开回来
    CHECK(visibleByName(set, "CatEar")); // mask 已清 ⇒ 能看见
}

TEST_CASE(visToggleGroupDoesNotClearOtherGroupsMask) {
    // 清 mask 只能清"自己这块"，不能顺手把别人的也清了。
    cVISIBILITY_SET set;
    buildDefault(set);
    set.SetMaskByName("Hat", true);
    CHECK(set.ToggleGroup(0)); // 切 cat 组，与 hat 无关
    CHECK_EQ(visibleByName(set, "Hat"), false); // hat 仍被 mask 挡住
    CHECK_EQ(set.MaskedCount(), 1u);
}

// ═══════════════════════════════════════════════════════════
//  七、优先级与诊断
// ═══════════════════════════════════════════════════════════

TEST_CASE(visThreeSourcesCombineWithAnd) {
    // 三个来源是 AND：只要有一个说隐藏，就隐藏。
    cVISIBILITY_SET set;
    buildDefault(set);
    // 顺序有讲究：SetFamilySelection / ToggleGroup 都会清掉自己成员的 mask
    // （用户接了手），所以要**先选族、再设 mask**，否则 mask 当场被清掉。
    CHECK(set.SetFamilySelection(1, 1)); // 族选第 1 套（Mesh_Body 属于第 0 套）
    CHECK_EQ(set.MaskedCount(), 0u); // 印证：选族会清 mask
    CHECK_EQ(set.SetMaskByName("Mesh_Body", true), 1u); // mask 隐藏
    set.ToggleGroup(0); // 无关组，不影响 Mask
    const cVISIBILITY_SET::sREASON reason = set.Explain(3); // Mesh_Body
    CHECK(reason.aHiddenByMask);
    CHECK(reason.aHiddenByFamily);
    CHECK_EQ(reason.aHiddenByGroup, false);
}

TEST_CASE(visExplainPointsAtHidingGroup) {
    cVISIBILITY_SET set;
    buildDefault(set);
    CHECK(set.ToggleGroup(1)); // hat
    const uint32_t hatIndex = 9; // "Hat"
    const cVISIBILITY_SET::sREASON reason = set.Explain(hatIndex);
    CHECK(reason.aHiddenByGroup);
    CHECK_EQ(reason.aGroup, 1);
    CHECK_EQ(reason.aHiddenByMask, false);
    CHECK_EQ(reason.aHiddenByFamily, false);
    // 可见的网格没有任何隐藏原因
    const cVISIBILITY_SET::sREASON room = set.Explain(10);
    CHECK_EQ(room.aHiddenByGroup, false);
    CHECK_EQ(room.aHiddenByMask, false);
    CHECK_EQ(room.aHiddenByFamily, false);
    // 越界
    const cVISIBILITY_SET::sREASON oob = set.Explain(999);
    CHECK_EQ(oob.aHiddenByGroup, false);
}

// ═══════════════════════════════════════════════════════════
//  八、Reset 与跨实例隔离
// ═══════════════════════════════════════════════════════════

TEST_CASE(visResetAllReturnsToImportedState) {
    cVISIBILITY_SET set;
    buildDefault(set);
    set.ToggleGroup(0);
    set.CycleFamily(0);
    set.SetMaskByName("Mesh_Body", true);
    CHECK(set.VisibleCount() < gNameCount);
    set.ResetAll();
    CHECK_EQ(set.VisibleCount(), gNameCount);
    CHECK_EQ(set.MaskedCount(), 0u);
    CHECK_EQ(set.FamilySelection(0), -1);
    CHECK_EQ(set.FamilySelection(1), -1);
    for (uint32_t group = 0; group < set.GroupCount(); ++group) {
        CHECK(set.GroupVisible(group));
    }
}

TEST_CASE(visTwoInstancesAreIndependent) {
    // **这条用例是抽离本模块的全部理由**：旧实现用文件级全局变量，
    // 两个角色不可能各自换装。现在两个实例必须完全独立。
    cVISIBILITY_SET player;
    cVISIBILITY_SET npc;
    buildDefault(player);
    buildDefault(npc);

    player.ToggleGroup(0); // player 关掉猫耳
    player.CycleFamily(0); // player 换发型
    player.SetMaskByName("Mesh_Body", true);

    // npc 完全不受影响
    CHECK_EQ(npc.VisibleCount(), gNameCount);
    CHECK(npc.GroupVisible(0));
    CHECK_EQ(npc.FamilySelection(0), -1);
    CHECK_EQ(npc.MaskedCount(), 0u);

    // 反向再确认一次
    npc.ToggleGroup(1); // npc 关掉帽子
    CHECK_EQ(player.GroupVisible(1), true);
    CHECK_EQ(player.GroupVisible(0), false);
    CHECK_EQ(npc.GroupVisible(0), true);
    CHECK_EQ(npc.GroupVisible(1), false);

    // 引用同一份规则描述（static 的 kGroups/kFamilies）也不互相污染
    CHECK_STREQ(player.GroupKey(0), npc.GroupKey(0)); // 比字符串，不是比指针
}

TEST_CASE(visBuildCopiesNamesSoCallersArrayCanDie) {
    // Build 必须复制名字：调用方（例如把 std::vector<std::string> 转成 const char*）
    // 不该被迫让数组活到规则集销毁。
    std::vector<std::string> temporary;
    temporary.push_back("Mesh_Hair");
    temporary.push_back("CatEar");
    const char *names[] = {temporary[0].c_str(), temporary[1].c_str()};
    cVISIBILITY_SET set;
    CHECK(set.Build(names, 2, makeDesc()));
    temporary.clear();
    temporary.shrink_to_fit();
    // 名字仍在
    CHECK_STREQ(set.NameOf(0), "Mesh_Hair");
    CHECK_STREQ(set.NameOf(1), "CatEar");
    CHECK_EQ(set.FamilySize(0), 1u);
}

TEST_CASE(visRebuildDiscardsPreviousState) {
    cVISIBILITY_SET set;
    buildDefault(set);
    set.ToggleGroup(0);
    set.CycleFamily(0);
    set.SetMaskByName("Mesh_Body", true);
    // 重建 = 重来一遍：不能残留上次的组状态/mask
    const char *names[] = {"Mesh_Hair", "Hat"};
    CHECK(set.Build(names, 2, makeDesc()));
    CHECK_EQ(set.MeshCount(), 2u);
    CHECK_EQ(set.MaskedCount(), 0u);
    CHECK_EQ(set.FamilySelection(0), -1);
    CHECK(set.GroupVisible(0));
    CHECK_EQ(set.VisibleCount(), 2u);
}

// ═══════════════════════════════════════════════════════════
//  九、非法输入不留下半个配置
// ═══════════════════════════════════════════════════════════

TEST_CASE(visBuildRejectsInvalidDescriptions) {
    cVISIBILITY_SET set;
    // 1) 有组数但没有组数组
    sVISIBILITY_DESC broken;
    broken.aGroupCount = 2;
    CHECK_EQ(set.Build(gNames, gNameCount, broken), false);
    CHECK_EQ(set.MeshCount(), 0u); // 保持空状态

    // 2) 组有规则数但规则指针为空
    const sHIDE_GROUP_DESC badGroup[] = {{"x", 0, 3, true}};
    sVISIBILITY_DESC desc;
    desc.aGroups = badGroup;
    desc.aGroupCount = 1;
    CHECK_EQ(set.Build(gNames, gNameCount, desc), false);
    CHECK_EQ(set.MeshCount(), 0u);

    // 3) 名字数非零但数组为空
    CHECK_EQ(set.Build(0, 5, makeDesc()), false);

    // 4) 之前的合法配置不能被非法调用破坏掉一半
    buildDefault(set);
    CHECK_EQ(set.Build(0, 5, makeDesc()), false);
    CHECK_EQ(set.MeshCount(), 0u); // 文档承诺：失败 ⇒ 空状态
    CHECK_EQ(set.VisibleCount(), 0u);
}

TEST_CASE(visEmptyBuildIsUsable) {
    // 0 个网格也要能安全使用（房间场景没有换装需求）。
    cVISIBILITY_SET set;
    CHECK(set.Build(0, 0, makeDesc()));
    CHECK_EQ(set.MeshCount(), 0u);
    CHECK_EQ(set.VisibleCount(), 0u);
    CHECK_EQ(set.IsVisible(0), false);
    CHECK_EQ(set.VisibleCount(), 0u);
    CHECK_EQ(set.FamilySize(0), 0u);
    CHECK_EQ(set.CycleFamily(0), false); // 族为空：不能切
    CHECK_EQ(set.SetMaskByName("anything", true), 0u);
    set.ResetAll(); // 不崩
    CHECK_STREQ(set.NameOf(0), "");
}

TEST_CASE(visNullNameIsTreatedAsEmpty) {
    const char *names[] = {0, "Hat"};
    cVISIBILITY_SET set;
    CHECK(set.Build(names, 2, makeDesc()));
    CHECK_STREQ(set.NameOf(0), "");
    CHECK(set.IsVisible(0)); // 空名字不属于任何组，可见
    CHECK(set.IsVisible(1));
}
