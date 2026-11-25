#pragma once
#ifndef GN_IMOSCENE_OUTFIT_H
#define GN_IMOSCENE_OUTFIT_H
// tests/imoScene/imoSceneOutfit.h — 换装：把按键动作接到显隐规则集，再把结果写回 cMODEL。
//
// 分工（这一层只剩三件事）：
//   1. **规则表**：哪几个名字算"猫耳组"、哪些前缀是发型/身体变体（见 .cpp 的 kGroups/kFamilies）；
//   2. **按键映射**：数字键 1–4 切组、5/6 循环变体、0 恢复（读 GN::cINPUT_MAP）；
//   3. **写回**：把 GN3d::cVISIBILITY_SET 的结果刷进 sMESH::aVisible。
//
// 显隐的**判定逻辑**全部在 GN3d::cVISIBILITY_SET（src/graphics/GN_Visibility.h）：
// 三类名字匹配、三个来源的 AND 合成、mask 与变体族的对称清空。那一层有 29 例单测，
// 且可以被多个场景同时使用（本文件只有一份全局状态）。
#include "GN.h"
#include "GN_3Dmodel.h"
#include "GN_Visibility.h" // 返回值类型 cVISIBILITY_SET 的完整定义

#include <string>
#include <vector>

namespace imoScene {

// ═══════════════════════════════════════════════════════════
//  默认服装组合的外部名单
// ═══════════════════════════════════════════════════════════
// 文件格式（`.local/imoScene/outfitDefault.json`，由游戏 prefab 导出）：
//   { "hidden": ["衬衫材质名", ...], "hair": 5 }
//
// 这两件事单独暴露出来是为了**能被单测**：它们曾经各有一个静默错误
// （`"hair"` 全文件搜索会误命中名单里的同名槽；`atoi("abc")` 等于 0，
// 把"全部显示"变成"选第一套"）。
struct sOUTFIT_DEFAULT {
    std::vector<std::string> aHidden;
    int32_t aHair; // -1 = 未指定

    sOUTFIT_DEFAULT();
};

// 从 JSON 文本解析（不读文件，便于测试）。缺 `"hidden"` 返回 false。
bool parseOutfitDefaultText(const std::string &text, sOUTFIT_DEFAULT &out);

// 从文件解析；失败返回 false（文件不存在也是 false，调用方决定是否提示）。
bool parseOutfitDefaultFile(const char *path, sOUTFIT_DEFAULT &out);

// 严格解析一个整数：整串必须都是数字（允许前后空白），否则返回 false 且不写 out。
// 用于环境变量 —— 不接受的输入要明确拒绝，不能像 atoi 那样静默给 0。
bool parseIntegerStrict(const char *text, int32_t &out);

// 读环境变量里的整数；未设置/非法返回 false（非法时会打印一行提示）。
bool parseIntegerEnv(const char *name, int32_t &out);

// 记录模型指针并建立规则集；可选地从 `defaultPath` 读"默认服装组合"名单。
// defaultPath 为空时用环境变量 IMO_SCENE_OUTFIT_DEFAULT，再退回内置默认路径。
void outfitInit(GN3d::cMODEL &model, const char *defaultPath = 0);

// 每帧一次（在 cameraUpdate 之后）：数字键 1–4 切换装饰组，5/6 循环变体，0 恢复全部。
void outfitUpdate();

// 当前可见网格数（HUD/调试用）。
uint32_t outfitVisibleMeshCount();

// 显隐规则集（只读；诊断/测试用）。未初始化时返回空对象。
const GN3d::cVISIBILITY_SET &outfitVisibility();

} // namespace imoScene
#endif
