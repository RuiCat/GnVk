// tests/mmdViewer/mmdViewerShared.h — MMD 查看器跨模块共享声明
//
// 职责：集中声明拆分后各 .cpp 之间共享的全局状态、共享数值常量与函数原型。
// 本文件只放声明与 extern：不写函数实现，也不定义可变状态
// （共享常量的唯一定义在 mmdViewerWeather.cpp）。
#ifndef MMD_VIEWER_SHARED_H
#define MMD_VIEWER_SHARED_H

#include <GN.h>
#include "GN_3Ddecal.h"
#include "weather.h"

#include <cstdint>
#include <string>
#include <vector>

// ── 共享状态：定义分别在各模块 .cpp ──────────────────────────────
extern GN3d::cMODEL gModel;                  // mmdViewer.cpp（场景装配）
extern GN3d::cLIGHT *gLight;                 // mmdViewer.cpp
extern std::vector<GNg::sIMAGE *> gTextures; // mmdViewer.cpp
extern std::string gModelPath;               // mmdViewer.cpp
extern GNg::sVEC3 gTarget;                   // mmdViewer.cpp
extern float gHeight;                        // mmdViewer.cpp
extern bool gLoaded;                         // mmdViewer.cpp
extern GNg::sMAT44 gLastViewProj;            // mmdViewer.cpp（叠层投影用）
extern bool gLastViewProjValid;              // mmdViewer.cpp
extern int gFbWidth;                         // mmdViewer.cpp
extern int gFbHeight;                        // mmdViewer.cpp

// 天气 / 降水（mmdViewerWeather.cpp）
extern GN3d::WeatherSystem gWeather;
extern GN3d::cPARTICLE_SYSTEM gRainParticles;
extern GN3d::cPARTICLE_SYSTEM gSnowParticles;
extern bool gPrecipReady;
extern bool gPrecipEnabled;
extern bool gCloudLayerEnabled;
extern GNg::sVEC3 gPrecipBoxCenter;
extern GNg::sVEC3 gPrecipBoxHalf;
extern float gRainRate;
extern float gSnowRate;
extern GNg::sVEC3 gRainSamplePos;
extern GNg::sVEC3 gRainSampleVel;
extern bool gRainSampleValid;
extern GNg::sVEC3 gRainNearestPos;
extern float gRainNearestDist;
extern GNg::sVEC3 gSnowSamplePos;
extern GNg::sVEC3 gSnowSampleVel;
extern bool gSnowSampleValid;

// 地面 / 贴花 / 调试线框开关（mmdViewerDecal.cpp）
extern bool gDecalEnabled;
extern bool gShowGizmos;

// 叠层与粒子调试开关（mmdViewerOverlay.cpp）
extern GNg::cTEXT_ART gInfoText;
extern bool gShowCoordInfo;
extern bool gParticleBoxes;

// ── 共享数值常量：唯一定义在 mmdViewerWeather.cpp ────────────────
// 这些值被天气模块与叠层/主循环共用，两处必须一致，所以只定义一份。
extern const float CLOUD_LAYER_HEIGHT;
extern const float CLOUD_TILE_SIZE;
extern const float RAIN_SIZE_MIN;
extern const float RAIN_SIZE_MAX;
extern const float SNOW_SIZE_MIN;
extern const float SNOW_SIZE_MAX;
extern const float RAIN_STREAK;
extern const float SNOW_STREAK;
extern const float PRECIP_DIR_Y;
extern const float RAIN_GRAVITY_Y;
extern const float SNOW_GRAVITY_Y;
extern const float PRECIP_SPAWN_HALF_XZ;
extern const float PRECIP_COLUMN_HEIGHT;

// ── 相机与视角（mmdViewerCamera.cpp）──
extern GN3d::cCAMERA gCamera; // 全查看器唯一的相机实例（定义在 mmdViewerCamera.cpp）
float clampValue(float value, float low, float high);
GNg::sVEC3 cameraPosition();
void cameraBasisVectors(GNg::sVEC3 &forward, GNg::sVEC3 &right, GNg::sVEC3 &up);
GNg::sVEC3 cameraTarget();
float cameraDistance();
void cameraBasis(const GNg::sVEC3 &camera, GNg::sVEC3 &right, GNg::sVEC3 &camUp);
void resetCamera();
void updateCameraInput(float dt);

// ── 天气 / 云 / 雾 / 降水（mmdViewerWeather.cpp）──
void applyWeather();
bool createPrecipitationSystems();
void updatePrecipitation(float dt);
void drawClouds(const GNg::sMAT44 &viewProjection, const GNg::sVEC3 &camera);
void drawPrecipitation(const GNg::sMAT44 &viewProjection, const GNg::sVEC3 &right,
                       const GNg::sVEC3 &camUp);

// ── 地面 / 贴花 / 坐标轴（mmdViewerDecal.cpp）──
float groundHeight();
void drawGround();
float footQuadHalf();
void drawFootQuad();
void createDecals();
void updateDecals(float dt);
void drawDecals(const GNg::sMAT44 &viewProjection);
void drawGizmos();

// ── 叠层 / 粒子调试包围盒（mmdViewerOverlay.cpp）──
void drawCoordOverlay();
void drawParticleDebugBoxes(const GNg::sVEC3 &right, const GNg::sVEC3 &camUp);

#endif // MMD_VIEWER_SHARED_H
