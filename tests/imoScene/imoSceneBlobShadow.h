#pragma once
#ifndef GN_IMOSCENE_BLOBSHADOW_H
#define GN_IMOSCENE_BLOBSHADOW_H
// tests/imoScene/imoSceneBlobShadow.h — 贴地软阴影（假接触阴影）。
//
// 为什么不用 CSM：引擎的阴影 pass 在真机首帧触发 GPU 页错误（见 CHANGELOG 2026-11-21），
// 在修好引擎之前，用一张径向渐变 PNG 平铺在脚下做接地感，零引擎改动、零阴影图开销。
// 边界：只加载/释放一张贴图并画一个四边形，不参与光照与相机计算。
#include "GN.h"
#include "GN_3Dmodel.h"

namespace imoScene {

// 贴地阴影贴图与开关（默认资产 assets/imoScene/blob_shadow.png；IMO_SCENE_BLOB=0 关闭）。
struct sBlobShadow {
    GNg::sIMAGE *aTexture;
    float aRadius; // 世界半径（米）

    sBlobShadow();
};

// 读取配置并解码贴图；失败返回 false（调用方按“无贴地阴影”继续）。
bool blobShadowLoad(sBlobShadow &out, const char *defaultPath);

// 释放贴图（可重复调用）。
void blobShadowRelease(sBlobShadow &out);

// 在 (centerX, y, centerZ) 贴地画一张带 alpha 的软阴影（PBR 混合管线，深度写关）。
void blobShadowDraw(const sBlobShadow &shadow, float centerX, float centerZ);

} // namespace imoScene
#endif // GN_IMOSCENE_BLOBSHADOW_H
