// tests/cornellBox/cornellShared.h — Cornell Box 测试程序的跨模块声明
//
// 职责：集中声明场景/相机/HUD 之间的共享状态与入口函数；不含实现。
// 本程序是独立可执行（make cornellBox），链接 libGN_mini.so，复用引擎的延迟 GI 路径。
#ifndef CORNELL_SHARED_H
#define CORNELL_SHARED_H

#include <GN.h>

#include "cornellGeometry.h"

#include <cstdint>
#include <vector>

namespace Cornell {

// 一个可绘制网格 + 其 PBR 材质。几何在世界空间，模型矩阵用单位阵。
struct sMesh {
    std::vector<GNg::VkVertex3D> aVertices;
    std::vector<uint32_t> aIndices;
    float aAlbedo[3];
    float aRoughness;
    float aMetallic;
    float aAo;
    sMesh();
};

// 场景与光照（cornellScene.cpp）
extern std::vector<sMesh> gMeshes;
extern GN3d::cLIGHT *gLight;
extern GNg::sIMAGE *gFlatNormal; // 1×1 平面法线贴图（默认白色会被当成法线贴图，改变顶点法线）
extern int gLightMode; // 0 = 方向光（sun），1 = 顶部点光（classic 近似）

bool initScene();
void releaseScene();
void rebuildLights(int lightMode);
void drawScene(const GNg::sMAT44 &viewProj, const GNg::sVEC3 &cameraPos);

// 相机（cornellCamera.cpp）
extern GN3d::cCAMERA gCamera;
void initCamera();
void resetCamera(int poseIndex);
void updateCamera(float dt);

// HUD（cornellHud.cpp）
extern GNg::cTEXT_ART gHudText;
extern bool gShowHud;
extern bool gSsaoOn;
extern float gSsgiStrength;
extern bool gEngineGiOn;
extern bool gCpuRtPreview;
extern bool gGpuRtPreview;

void initHud();
void releaseHud();
void drawHud();

} // namespace Cornell
#endif // CORNELL_SHARED_H
