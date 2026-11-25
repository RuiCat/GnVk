#pragma once
#ifndef _GN_VISIBILITY_H_
#define _GN_VISIBILITY_H_
// ╔══════════════════════════════════════════════════════════╗
// ║  GN_mini — 网格显隐规则集（换装 / 部件开关 / 变体切换）  ║
// ╚══════════════════════════════════════════════════════════╝
//
// 解决的问题：`sMESH::aVisible` 是引擎字段，但**决定它取什么值的那套规则**
// 之前只存在于演示程序里的几个文件级全局变量中。具体的病：
//
//   * 两个角色不可能同时换装（全局变量只有一份）；
//   * 规则与"读按键 / 读环境变量 / 写 JSON"搅在一起，任何逻辑都没法单独测；
//   * 同一个问题"这个网格是不是 X"有三种匹配规则并存：
//       装饰组用**大小写敏感**的 strstr（所以猫耳组不得不写 "Ribbon" 和 "ribbon" 两条，
//       换一套资产命名习惯就静默失效）；
//       变体族用前缀（"Mesh_Body" 会命中将来的 "Mesh_BodyArmor"）；
//       默认组合用"去掉 #prim 后精确相等"。
//
// 这个模块只做一件事：**给一组名字和一组规则，算出每个名字是否可见**。
// 它不认 cMODEL、不读输入、不读文件、不碰全局状态 —— 因此可以被两个场景同时使用，
// 也可以在没有模型资产的机器上完整验证。
//
// ═══ 三个显隐来源，AND 合成 ═══
//
//   可见 = 没有任何"隐藏的组"命中它
//        ∧ 它不在 mask 名单里
//        ∧ 它不是"被排除的变体"
//
// 优先级（冲突时谁说了算）：**mask 最高 → 变体族 → 组**。
// 之所以要写死这个顺序：mask 表达的是"这套资产的默认配置"（导入时定下的），
// 变体族表达"这个槽位当前选中哪一套"，组表达"用户随时开关的部件"。
// 从稳定到易变排列，用户每次操作都能覆盖上一层。
//
// ═══ 变体族是互斥的，组是可以重叠的 ═══
//
//   * 一个网格**最多属于一个**变体族（按注册顺序取第一个命中的族）——
//     同一个网格不可能同时是"第 3 套发型"和"第 5 套身体"；
//   * 一个网格**可以属于多个**组 —— 项圈既算"丝带/领结"组也算"装饰"组是合理的，
//     只要有一组被关掉它就隐藏。
//
// ═══ 用户操作会清掉自己那块 mask ═══
//
// mask 是"默认配置"，用户一旦手动操作某一块（关掉一个组 / 切换一个变体），
// 就表示"这块我自己管"，于是**该块成员的 mask 被清掉**。
// 这条对组和变体族**对称**处理：早期实现只有组会清、变体不清，表现是
// 「默认组合把不穿的衣服 mask 掉了，按 6 切换身体没反应」—— 见 CHANGELOG。

#include <GN.h> // 必须先于本组头：GN_API 等基础宏

#include <cstdint>
#include <string>
#include <vector>

namespace GN3d {

// ═══════════════════════════════════════════════════════════
//  名字匹配（三类，全部大小写不敏感）
// ═══════════════════════════════════════════════════════════
// 匹配对象是「基名」：网格名里的 `#N` 后缀（一个节点拆成多个 primitive 时加的）
// 会被去掉。所以 `Mesh_Body#1` 的基名是 `Mesh_Body`，三类规则都按基名判。
enum eNAME_MATCH {
    eNAME_EXACT_BASE = 0, // 基名完全相等：只命中 Mesh_Body 本身，不命中 Mesh_Body.001
    eNAME_PREFIX, // 基名以它开头：命中 Mesh_Body / Mesh_Body.001
    eNAME_SUBSTRING, // 基名包含它：命中任何名字里带这段的网格
};

struct sNAME_RULE {
    eNAME_MATCH aKind;
    const char *aPattern; // 不能为空；空 pattern 表示"这条规则不用"（Build 时跳过）

    sNAME_RULE();
    sNAME_RULE(eNAME_MATCH kind, const char *pattern);
};

// 单独暴露的匹配函数：便于对拍与调试（模块内部也用它）。
// name 或 pattern 为 null 返回 false；pattern 为空串返回 false（空规则不命中任何东西）。
bool matchNameRule(const char *name, const sNAME_RULE &rule);

// ═══════════════════════════════════════════════════════════
//  规则描述
// ═══════════════════════════════════════════════════════════
// 一组可整体开关的网格（猫耳/帽子/口罩/丝带…）。
struct sHIDE_GROUP_DESC {
    const char *aKey; // 标签，只用于查询与日志，不参与匹配
    const sNAME_RULE *aRules;
    uint32_t aRuleCount;
    bool aInitiallyVisible; // 初始是否可见（默认 true）

    sHIDE_GROUP_DESC();
    // 显式构造：带默认构造函数的类型在 C++11 里不再是聚合体，不能花括号初始化，
    // 而规则表写成 `{"cat", kRules, 2, true}` 的可读性远好于逐字段赋值。
    sHIDE_GROUP_DESC(const char *key, const sNAME_RULE *rules, uint32_t ruleCount,
                     bool initiallyVisible = true);
};

// 同一槽位的一组互斥变体（发型套系 / 服装套系）。
struct sVARIANT_FAMILY_DESC {
    const char *aKey; // 标签（如 "hair"），只用于查询与日志
    sNAME_RULE aRule;

    sVARIANT_FAMILY_DESC();
    sVARIANT_FAMILY_DESC(const char *key, const sNAME_RULE &rule);
};

struct sVISIBILITY_DESC {
    const sHIDE_GROUP_DESC *aGroups;
    uint32_t aGroupCount;
    const sVARIANT_FAMILY_DESC *aFamilies;
    uint32_t aFamilyCount;

    sVISIBILITY_DESC();
    bool IsValid() const; // 数组非空时指针也必须非空
};

// ═══════════════════════════════════════════════════════════
//  显隐规则集
// ═══════════════════════════════════════════════════════════
class cVISIBILITY_SET {
  public:
    cVISIBILITY_SET();

    // 按名字列表与规则建立状态。名字会被**复制**（调用方不必保持数组存活）。
    //
    // 返回 false 表示描述非法，此时对象保持"全部可见、无规则"的状态
    // （不会留下半个配置）。重复调用等于重建：所有组/族状态回到初始值，mask 清空。
    bool Build(const char *const *names, uint32_t count, const sVISIBILITY_DESC &desc);

    bool IsBuilt() const { return !aMeshes.empty() || !aNames.empty(); }
    uint32_t MeshCount() const { return static_cast<uint32_t>(aNames.size()); }
    const char *NameOf(uint32_t mesh) const; // 越界返回 ""

    // ── 查询 ──
    bool IsVisible(uint32_t mesh) const; // 越界返回 false
    uint32_t VisibleCount() const;
    // 可见性来源分解（诊断用：排查"为什么它不见了"）。
    struct sREASON {
        bool aHiddenByGroup;
        bool aHiddenByMask;
        bool aHiddenByFamily;
        int32_t aGroup; // 第一个隐藏它的组下标，-1 = 无

        sREASON();
    };
    sREASON Explain(uint32_t mesh) const;

    // ── 组 ──
    uint32_t GroupCount() const;
    const char *GroupKey(uint32_t group) const;
    bool GroupVisible(uint32_t group) const; // 越界返回 true
    // 切换某组显隐；同时清掉该组成员的 mask（用户接手这块）。
    // 越界或无成员返回 false。
    bool ToggleGroup(uint32_t group);

    // ── 变体族 ──
    uint32_t FamilyCount() const;
    const char *FamilyKey(uint32_t family) const;
    uint32_t FamilySize(uint32_t family) const; // 该族有几个变体；越界返回 0
    int32_t FamilySelection(uint32_t family) const; // -1 = 全部显示；越界返回 -1
    // 循环切换：全部(-1) → 0 → 1 → … → size-1 → 全部。
    // 同时清掉该族成员的 mask（与 ToggleGroup 对称）。
    // 越界或该族为空返回 false。
    bool CycleFamily(uint32_t family);
    // 直接选中某一套；slot < 0 或 >= size 表示"全部"。返回是否生效。
    bool SetFamilySelection(uint32_t family, int32_t slot);

    // ── mask（按名字设置，因为配置来自外部名单而不是下标）──
    // 命中基名的网格全部标记为隐藏（hidden=true）或清除标记。返回改动了几个。
    uint32_t SetMaskByName(const char *name, bool hidden);
    // 按规则组设置 mask（比名字更通用：一份名单可以配一条规则）。
    uint32_t SetMaskByRule(const sNAME_RULE &rule, bool hidden);
    void ClearMask();
    uint32_t MaskedCount() const;

    // ── 全局 ──
    // 所有组可见、所有族"全部"、mask 清空。等价于"刚导入的样子"。
    void ResetAll();

  private:
    void RebuildMembership(const sVISIBILITY_DESC &desc);

    struct sMESH_INFO {
        std::string aName; // 原始名（带 #prim）
        std::string aBase; // 去掉 #prim 的基名（匹配用）
        std::vector<uint32_t> aGroups; // 命中的组（可多个）
        int32_t aFamily; // 所属族（至多一个），-1 = 无
        int32_t aFamilySlot; // 族内序号，-1 = 无
        bool aMasked;

        sMESH_INFO();
    };
    struct sGROUP_STATE {
        std::string aKey;
        bool aVisible;
    };
    struct sFAMILY_STATE {
        std::string aKey;
        std::vector<uint32_t> aMeshes; // 族内网格（按名字顺序）
        int32_t aSelected; // -1 = 全部
    };

    std::vector<sMESH_INFO> aMeshes;
    std::vector<std::string> aNames; // 与 aMeshes 等长，NameOf 用（避免暴露内部结构）
    std::vector<sGROUP_STATE> aGroups;
    std::vector<sFAMILY_STATE> aFamilies;
};

} // namespace GN3d
#endif // _GN_VISIBILITY_H_
