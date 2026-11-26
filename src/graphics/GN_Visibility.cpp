// GN_Visibility.cpp — 网格显隐规则集（纯 CPU）实现。
//
// 只有名字匹配与状态合成，不 include Vulkan、不读文件、不碰全局变量：
// 同一串操作永远得到同一份结果，两个实例互不影响。

#include "GN_Visibility.h"

#include <cstring>

namespace GN3d {

namespace {

char lowerAscii(char c) {
    return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
}

// 大小写不敏感的前缀比较。
bool startsWithIgnoreCase(const std::string &text, const char *prefix) {
    const size_t length = std::strlen(prefix);
    if (length == 0 || length > text.size()) {
        return false;
    }
    for (size_t i = 0; i < length; ++i) {
        if (lowerAscii(text[i]) != lowerAscii(prefix[i])) {
            return false;
        }
    }
    return true;
}

// 大小写不敏感的子串查找。
bool containsIgnoreCase(const std::string &text, const char *needle) {
    const size_t length = std::strlen(needle);
    if (length == 0 || length > text.size()) {
        return false;
    }
    for (size_t start = 0; start + length <= text.size(); ++start) {
        size_t offset = 0;
        while (offset < length && lowerAscii(text[start + offset]) == lowerAscii(needle[offset])) {
            ++offset;
        }
        if (offset == length) {
            return true;
        }
    }
    return false;
}

// 大小写不敏感的相等。
bool equalsIgnoreCase(const std::string &text, const char *other) {
    const size_t length = std::strlen(other);
    if (length != text.size()) {
        return false;
    }
    for (size_t i = 0; i < length; ++i) {
        if (lowerAscii(text[i]) != lowerAscii(other[i])) {
            return false;
        }
    }
    return true;
}

// 去掉 `#N` 后缀（glTF 一个节点拆多个 primitive 时加的后缀）。
std::string baseNameOf(const char *name) {
    std::string base(name != 0 ? name : "");
    const size_t hash = base.find('#');
    if (hash != std::string::npos) {
        base.erase(hash);
    }
    return base;
}

} // namespace

// ═══════════════════════════════════════════════════════════
//  描述结构
// ═══════════════════════════════════════════════════════════

sNAME_RULE::sNAME_RULE() : aKind(eNAME_SUBSTRING), aPattern(0) {}

sNAME_RULE::sNAME_RULE(eNAME_MATCH kind, const char *pattern) : aKind(kind), aPattern(pattern) {}

sHIDE_GROUP_DESC::sHIDE_GROUP_DESC()
    : aKey(0), aRules(0), aRuleCount(0), aInitiallyVisible(true) {}

sHIDE_GROUP_DESC::sHIDE_GROUP_DESC(const char *key, const sNAME_RULE *rules, uint32_t ruleCount,
                                   bool initiallyVisible)
    : aKey(key), aRules(rules), aRuleCount(ruleCount), aInitiallyVisible(initiallyVisible) {}

sVARIANT_FAMILY_DESC::sVARIANT_FAMILY_DESC() : aKey(0), aRule() {}

sVARIANT_FAMILY_DESC::sVARIANT_FAMILY_DESC(const char *key, const sNAME_RULE &rule)
    : aKey(key), aRule(rule) {}

sVISIBILITY_DESC::sVISIBILITY_DESC()
    : aGroups(0), aGroupCount(0), aFamilies(0), aFamilyCount(0) {}

bool sVISIBILITY_DESC::IsValid() const {
    if (aGroupCount > 0 && aGroups == 0) {
        return false;
    }
    if (aFamilyCount > 0 && aFamilies == 0) {
        return false;
    }
    return true;
}

// ═══════════════════════════════════════════════════════════
//  名字匹配
// ═══════════════════════════════════════════════════════════

bool matchNameRule(const char *name, const sNAME_RULE &rule) {
    if (name == 0 || rule.aPattern == 0 || rule.aPattern[0] == '\0') {
        return false; // 空规则不命中任何东西（区别见头注释：调用方要"全部"应传空 desc）
    }
    const std::string base = baseNameOf(name);
    switch (rule.aKind) {
    case eNAME_EXACT_BASE:
        return equalsIgnoreCase(base, rule.aPattern);
    case eNAME_PREFIX:
        return startsWithIgnoreCase(base, rule.aPattern);
    case eNAME_SUBSTRING:
        return containsIgnoreCase(base, rule.aPattern);
    }
    return false; // 越界枚举值：保守不命中
}

// ═══════════════════════════════════════════════════════════
//  sREASON
// ═══════════════════════════════════════════════════════════

cVISIBILITY_SET::sREASON::sREASON()
    : aHiddenByGroup(false), aHiddenByMask(false), aHiddenByFamily(false), aGroup(-1) {}

// ═══════════════════════════════════════════════════════════
//  sMESH_INFO
// ═══════════════════════════════════════════════════════════

cVISIBILITY_SET::sMESH_INFO::sMESH_INFO()
    : aFamily(-1), aFamilySlot(-1), aMasked(false) {}

// ═══════════════════════════════════════════════════════════
//  构建
// ═══════════════════════════════════════════════════════════

cVISIBILITY_SET::cVISIBILITY_SET() {}

bool cVISIBILITY_SET::Build(const char *const *names, uint32_t count,
                           const sVISIBILITY_DESC &desc) {
    // 先清空：失败时对象停在"空规则"状态，而不是残留上一次的半个配置。
    aMeshes.clear();
    aNames.clear();
    aGroups.clear();
    aFamilies.clear();

    if (count > 0 && names == 0) {
        return false;
    }
    if (!desc.IsValid()) {
        return false;
    }
    for (uint32_t group = 0; group < desc.aGroupCount; ++group) {
        if (desc.aGroups[group].aRuleCount > 0 && desc.aGroups[group].aRules == 0) {
            return false;
        }
    }

    aMeshes.resize(count);
    aNames.resize(count);
    for (uint32_t mesh = 0; mesh < count; ++mesh) {
        const char *name = (names[mesh] != 0) ? names[mesh] : "";
        aMeshes[mesh].aName.assign(name);
        aMeshes[mesh].aBase = baseNameOf(name);
        aNames[mesh].assign(name);
    }

    aGroups.resize(desc.aGroupCount);
    for (uint32_t group = 0; group < desc.aGroupCount; ++group) {
        const sHIDE_GROUP_DESC &source = desc.aGroups[group];
        aGroups[group].aKey.assign(source.aKey != 0 ? source.aKey : "");
        aGroups[group].aVisible = source.aInitiallyVisible;
    }

    aFamilies.resize(desc.aFamilyCount);
    for (uint32_t family = 0; family < desc.aFamilyCount; ++family) {
        const sVARIANT_FAMILY_DESC &source = desc.aFamilies[family];
        aFamilies[family].aKey.assign(source.aKey != 0 ? source.aKey : "");
        aFamilies[family].aSelected = -1; // 默认"全部显示"：只留一套会露头皮
    }

    RebuildMembership(desc);
    return true;
}

void cVISIBILITY_SET::RebuildMembership(const sVISIBILITY_DESC &desc) {
    // 幂等：族成员表先清空再登记（当前只由 Build 调用，但重复调用不应累积重复成员）。
    for (uint32_t family = 0; family < aFamilies.size(); ++family) {
        aFamilies[family].aMeshes.clear();
    }
    // ── 组：一个网格可以属于多个组 ──
    for (uint32_t mesh = 0; mesh < aMeshes.size(); ++mesh) {
        aMeshes[mesh].aGroups.clear();
        const char *name = aMeshes[mesh].aName.c_str();
        for (uint32_t group = 0; group < aGroups.size(); ++group) {
            const sHIDE_GROUP_DESC &source = desc.aGroups[group];
            for (uint32_t rule = 0; rule < source.aRuleCount; ++rule) {
                if (matchNameRule(name, source.aRules[rule])) {
                    aMeshes[mesh].aGroups.push_back(group);
                    break;
                }
            }
        }
    }

    // ── 变体族：一个网格最多属于一个族（按注册顺序取第一个命中）──
    for (uint32_t mesh = 0; mesh < aMeshes.size(); ++mesh) {
        aMeshes[mesh].aFamily = -1;
        aMeshes[mesh].aFamilySlot = -1;
        const char *name = aMeshes[mesh].aName.c_str();
        for (uint32_t family = 0; family < aFamilies.size(); ++family) {
            if (matchNameRule(name, desc.aFamilies[family].aRule)) {
                aMeshes[mesh].aFamily = static_cast<int32_t>(family);
                aMeshes[mesh].aFamilySlot = static_cast<int32_t>(aFamilies[family].aMeshes.size());
                aFamilies[family].aMeshes.push_back(mesh);
                break; // 互斥：不再尝试后面的族
            }
        }
    }
}

// ═══════════════════════════════════════════════════════════
//  查询
// ═══════════════════════════════════════════════════════════

const char *cVISIBILITY_SET::NameOf(uint32_t mesh) const {
    if (mesh >= aNames.size()) {
        return "";
    }
    return aNames[mesh].c_str();
}

bool cVISIBILITY_SET::IsVisible(uint32_t mesh) const {
    if (mesh >= aMeshes.size()) {
        return false;
    }
    const sMESH_INFO &info = aMeshes[mesh];
    // 组：任何一个所属组被关掉就隐藏（组可重叠 ⇒ 全部都要可见）
    for (size_t i = 0; i < info.aGroups.size(); ++i) {
        if (!aGroups[info.aGroups[i]].aVisible) {
            return false;
        }
    }
    if (info.aMasked) {
        return false;
    }
    if (info.aFamily >= 0) {
        const sFAMILY_STATE &family = aFamilies[static_cast<size_t>(info.aFamily)];
        if (family.aSelected >= 0 && family.aSelected != info.aFamilySlot) {
            return false;
        }
    }
    return true;
}

uint32_t cVISIBILITY_SET::VisibleCount() const {
    uint32_t count = 0;
    for (uint32_t mesh = 0; mesh < aMeshes.size(); ++mesh) {
        if (IsVisible(mesh)) {
            ++count;
        }
    }
    return count;
}

cVISIBILITY_SET::sREASON cVISIBILITY_SET::Explain(uint32_t mesh) const {
    sREASON reason;
    if (mesh >= aMeshes.size()) {
        return reason;
    }
    const sMESH_INFO &info = aMeshes[mesh];
    for (size_t i = 0; i < info.aGroups.size(); ++i) {
        if (!aGroups[info.aGroups[i]].aVisible) {
            reason.aHiddenByGroup = true;
            reason.aGroup = static_cast<int32_t>(info.aGroups[i]);
            break;
        }
    }
    reason.aHiddenByMask = info.aMasked;
    if (info.aFamily >= 0) {
        const sFAMILY_STATE &family = aFamilies[static_cast<size_t>(info.aFamily)];
        if (family.aSelected >= 0 && family.aSelected != info.aFamilySlot) {
            reason.aHiddenByFamily = true;
        }
    }
    return reason;
}

// ═══════════════════════════════════════════════════════════
//  组
// ═══════════════════════════════════════════════════════════

uint32_t cVISIBILITY_SET::GroupCount() const {
    return static_cast<uint32_t>(aGroups.size());
}

const char *cVISIBILITY_SET::GroupKey(uint32_t group) const {
    if (group >= aGroups.size()) {
        return "";
    }
    return aGroups[group].aKey.c_str();
}

bool cVISIBILITY_SET::GroupVisible(uint32_t group) const {
    if (group >= aGroups.size()) {
        return true; // 越界当作"没有这个组"，不影响可见性
    }
    return aGroups[group].aVisible;
}

bool cVISIBILITY_SET::ToggleGroup(uint32_t group) {
    if (group >= aGroups.size()) {
        return false;
    }
    aGroups[group].aVisible = !aGroups[group].aVisible;
    // 用户接了手：把这组成员的 mask 交回手动控制。
    for (uint32_t mesh = 0; mesh < aMeshes.size(); ++mesh) {
        const std::vector<uint32_t> &groups = aMeshes[mesh].aGroups;
        for (size_t i = 0; i < groups.size(); ++i) {
            if (groups[i] == group) {
                aMeshes[mesh].aMasked = false;
                break;
            }
        }
    }
    return true;
}

// ═══════════════════════════════════════════════════════════
//  变体族
// ═══════════════════════════════════════════════════════════

uint32_t cVISIBILITY_SET::FamilyCount() const {
    return static_cast<uint32_t>(aFamilies.size());
}

const char *cVISIBILITY_SET::FamilyKey(uint32_t family) const {
    if (family >= aFamilies.size()) {
        return "";
    }
    return aFamilies[family].aKey.c_str();
}

uint32_t cVISIBILITY_SET::FamilySize(uint32_t family) const {
    if (family >= aFamilies.size()) {
        return 0;
    }
    return static_cast<uint32_t>(aFamilies[family].aMeshes.size());
}

int32_t cVISIBILITY_SET::FamilySelection(uint32_t family) const {
    if (family >= aFamilies.size()) {
        return -1;
    }
    return aFamilies[family].aSelected;
}

bool cVISIBILITY_SET::CycleFamily(uint32_t family) {
    if (family >= aFamilies.size()) {
        return false;
    }
    sFAMILY_STATE &state = aFamilies[family];
    const int32_t size = static_cast<int32_t>(state.aMeshes.size());
    if (size == 0) {
        return false;
    }
    if (state.aSelected + 1 >= size) {
        state.aSelected = -1; // 回到"全部显示"
    } else {
        ++state.aSelected;
    }
    // 与 ToggleGroup 对称：用户切换了这套变体，就把它成员的 mask 交回手动控制。
    // 早期实现漏了这一步，表现是「默认组合 mask 掉的服装，按 6 切换身体没反应」。
    for (size_t i = 0; i < state.aMeshes.size(); ++i) {
        aMeshes[state.aMeshes[i]].aMasked = false;
    }
    return true;
}

bool cVISIBILITY_SET::SetFamilySelection(uint32_t family, int32_t slot) {
    if (family >= aFamilies.size()) {
        return false;
    }
    sFAMILY_STATE &state = aFamilies[family];
    const int32_t size = static_cast<int32_t>(state.aMeshes.size());
    if (slot >= size) {
        return false;
    }
    state.aSelected = (slot < 0) ? -1 : slot;
    for (size_t i = 0; i < state.aMeshes.size(); ++i) {
        aMeshes[state.aMeshes[i]].aMasked = false;
    }
    return true;
}

// ═══════════════════════════════════════════════════════════
//  mask
// ═══════════════════════════════════════════════════════════

uint32_t cVISIBILITY_SET::SetMaskByName(const char *name, bool hidden) {
    if (name == 0 || name[0] == '\0') {
        return 0;
    }
    uint32_t changed = 0;
    for (uint32_t mesh = 0; mesh < aMeshes.size(); ++mesh) {
        // 按**基名**精确比较：配置里写的是槽位名，而网格名可能带 #prim 后缀。
        if (equalsIgnoreCase(aMeshes[mesh].aBase, name)) {
            if (aMeshes[mesh].aMasked != hidden) {
                ++changed;
            }
            aMeshes[mesh].aMasked = hidden;
        }
    }
    return changed;
}

uint32_t cVISIBILITY_SET::SetMaskByRule(const sNAME_RULE &rule, bool hidden) {
    uint32_t changed = 0;
    for (uint32_t mesh = 0; mesh < aMeshes.size(); ++mesh) {
        if (matchNameRule(aMeshes[mesh].aName.c_str(), rule)) {
            if (aMeshes[mesh].aMasked != hidden) {
                ++changed;
            }
            aMeshes[mesh].aMasked = hidden;
        }
    }
    return changed;
}

void cVISIBILITY_SET::ClearMask() {
    for (uint32_t mesh = 0; mesh < aMeshes.size(); ++mesh) {
        aMeshes[mesh].aMasked = false;
    }
}

uint32_t cVISIBILITY_SET::MaskedCount() const {
    uint32_t count = 0;
    for (uint32_t mesh = 0; mesh < aMeshes.size(); ++mesh) {
        if (aMeshes[mesh].aMasked) {
            ++count;
        }
    }
    return count;
}

// ═══════════════════════════════════════════════════════════
//  全局
// ═══════════════════════════════════════════════════════════

void cVISIBILITY_SET::ResetAll() {
    for (uint32_t group = 0; group < aGroups.size(); ++group) {
        aGroups[group].aVisible = true;
    }
    for (uint32_t family = 0; family < aFamilies.size(); ++family) {
        aFamilies[family].aSelected = -1;
    }
    ClearMask();
}

} // namespace GN3d
