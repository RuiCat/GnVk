#pragma once
#ifndef GN_IMOSCENE_SHADOW_H
#define GN_IMOSCENE_SHADOW_H
// tests/imoScene/imoSceneShadow.h — 方向光 CSM 阴影（只让角色投影）。
//
// 边界：只做「加阴影方向光 + 每帧拟合级联 + 画深度图」；不碰房间/角色模型本身，
//       也不决定绘制顺序（主循环在 3D pass 之前调用）。
#include "GN.h"
#include "GN_3Dlight.h"
#include "GN_3Dmodel.h"
#include "GN_Spatial.h"

namespace imoScene {

// 阴影配置：默认开启；IMO_SCENE_SHADOWS=0 关闭（调试/回退）。强度/方向可用环境变量微调。
struct sShadowOptions {
    bool aEnabled;
    float aIntensity;
    GNg::sVEC3 aDirection; // 光线传播方向（单位向量，主要朝下）
    uint32_t aCascadeCount;
    uint32_t aMapSize;

    sShadowOptions();
};

// 从环境变量读阴影配置。
sShadowOptions shadowOptionsFromEnv();

// 给光源加一盏带阴影的弱方向光（方向近似吸顶灯），成功返回 true。
// 失败（光源数量已满）时返回 false，调用方按"无阴影"继续。
bool shadowAttachLight(GN3d::cLIGHT *light, const sShadowOptions &options);

// 每帧按本帧相机拟合级联并写回 light UBO；casterBox = 角色世界 AABB
// （会向四周扩展出阴影承接区），fovYRad 为垂直视场角（弧度）。返回是否拟合成功。
bool shadowFit(GN3d::cLIGHT *light, const GNg::sMAT44 &cameraView, const GNg::sVEC3 &cameraPos,
               float fovYRad, float aspect, const GN3d::sAABB &casterBox,
               const sShadowOptions &options);

// 画阴影深度图：每级一次 Begin/End，只画角色（房间不投影，否则封闭室内被墙挡死）。
void shadowDrawCaster(const GN3d::cMODEL &caster);

// 只读访问上次拟合成功的级联（诊断/CPU 探针用；越界返回单位阵）。
GNg::sMAT44 shadowCascadeViewProj(uint32_t index);
uint32_t shadowCascadeCount();

} // namespace imoScene
#endif // GN_IMOSCENE_SHADOW_H
