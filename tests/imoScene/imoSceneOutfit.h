#pragma once
#ifndef GN_IMOSCENE_OUTFIT_H
#define GN_IMOSCENE_OUTFIT_H
// tests/imoScene/imoSceneOutfit.h — 网格级换装控制（按名字分组显隐）。
//
// 边界：只改 sMESH::aVisible，不重建模型/材质；分组按网格名匹配（见 .cpp 的 kGroups）。
#include "GN.h"
#include "GN_3Dmodel.h"

namespace imoScene {

// 记录模型指针并初始化分组状态（默认全部可见）。
void outfitInit(GN3d::cMODEL &model);

// 每帧一次（在 cameraUpdate 之后）：数字键 1–4 切换装饰组，0 恢复全部。
void outfitUpdate();

// 当前可见网格数（HUD/调试用）。
uint32_t outfitVisibleMeshCount();

} // namespace imoScene
#endif