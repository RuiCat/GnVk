// tests/mmdViewer/mmdViewer.cpp — MMD 模型查看器（测试程序）
//
// 用法：
//   make mmdViewer
//   LD_LIBRARY_PATH=. ./build/mmdViewer
//   MMD_VIEWER_MODEL=/path/to/model.pmx LD_LIBRARY_PATH=. ./build/mmdViewer
//
// 边界：
//   * 只显示绑定姿势（不做骨骼动画/蒙皮），卡通/球面贴图与透明混合未实现；
//   * 贴图支持 .png（引擎 ImageLoadFromMem）与 .tga/.bmp（本地解码），其余格式跳过；
//   * 本程序需要 Vulkan 设备与窗口环境；编译不依赖 GPU。
#include <GN.h>
#include "GN_3Dlight.h"
#include "GN_3Dparticle.h"
#include "GN_3Ddecal.h"
#include "GN_Physics.h" // sDEBUG_LINE（P8 调试线框）
#include "weather.h"
#include "mmd/mmdFile.h"
#include "mmd/mmdMeshBuilder.h"
#include "mmd/mmdTextureDecode.h"
#include "mmd/pmdLoader.h"
#include "mmd/pmxLoader.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace {

// 相机：只有一套实现 —— 世界坐标位置 + 朝向四元数（没有轨道/欧拉角分支）。
// 鼠标水平位移绕世界 +Y、垂直位移绕相机右轴，均左乘到四元数上，避免万向锁与 roll 漂移。
const float DEFAULT_VIEW_PITCH = 0.12f;    // 默认正面视角的俯角（相机高于模型中心）
const float VIEW_PITCH_LIMIT = 1.45f;      // 俯仰上限（弧度，约 83°）
const float MOUSE_LOOK_SPEED = 0.005f;     // 鼠标灵敏度（弧度/像素）
const float KEY_LOOK_SPEED = 1.6f;         // 方向键转视角速度（弧度/秒）
const float DEFAULT_DISTANCE_FACTOR = 1.9f; // 默认观察距离 = 模型高度 × 该系数

GN3d::cMODEL gModel;
GN3d::cLIGHT *gLight = nullptr;
GN3d::WeatherSystem gWeather;
std::vector<GNg::sIMAGE *> gTextures;
std::string gModelPath;
GNg::sVEC3 gTarget = {0.0f, 0.0f, 0.0f};
float gHeight = 1.0f;
float gDelta = 0.016f;
bool gLoaded = false;
GNg::sVEC3 gCameraPosition = {0.0f, 0.0f, 0.0f};
GNg::sQUAT gCameraRotation;
// 鼠标拖拽求增量用的上一帧光标位置。不依赖引擎的 MouseM 时序：
// 每帧自己用光标绝对位置做差，按键松开/首次按下时重新锚定，避免用陈旧增量。
int gLastMouseX = 0;
int gLastMouseY = 0;
bool gLastMouseValid = false;

// ── P11-S6：云层 / 雾 / 降水参数（查看器侧表现，不改天气状态机）────
const float CLOUD_LAYER_HEIGHT = 300.0f;
const float CLOUD_TILE_SIZE = 256.0f;
const uint32_t CLOUD_TILES_PER_AXIS = 8;
const float CLOUD_NEAR_FADE = 50.0f;
const float FOG_MAX_OPACITY = 0.9f;
// 粒子池 = 屏幕密度的实际上限（稳态下池会被填满）。原先 61440/81920 太密，缩小 3~4 倍。
// 每颗在 CPU 端展开 6 个顶点（36 B/顶点）后整块上传，池子越小上传与填充压力越低。
const uint32_t RAIN_CAPACITY = 16384;
const uint32_t SNOW_CAPACITY = 24576;
// 发射率只需保证「能在寿命内填满池」：雨 16384 / 2 s ≈ 8200/s，雪 24576 / 9 s ≈ 2700/s。
// 取 60000 / 30000（× 天气强度）留足余量，池满后由发射器的补偿逻辑维持连续补充。
// 降水外观参数：发射器与叠层显示共用同一组常量，避免两处写死不一致。
// 雨丝：宽度减半（0.05–0.09 m），拉长倍数补到 20 ⇒ 长度仍是 1.0–1.8 m。
// 宽度按 30 m 处 ~1.7 px 反推；太宽会变成"白色长条"。若仍偏粗/偏细，只调这两组常量。
const float RAIN_SIZE_MIN = 0.05f;
const float RAIN_SIZE_MAX = 0.09f;
const float SNOW_SIZE_MIN = 0.15f;
const float SNOW_SIZE_MAX = 0.25f;
const float RAIN_STREAK = 20.0f;
const float SNOW_STREAK = 1.0f;
const float PRECIP_DIR_Y = -1.0f;   // 世界 Y 方向：-1 = 向下落
const float RAIN_GRAVITY_Y = -9.81f;
const float SNOW_GRAVITY_Y = -2.5f;
// 降水柱：固定在世界空间、位于角色正上方，与相机完全无关。
// 发射器是一片水平面（BOX 的 y 半范围为 0），粒子从顶部平面生成、向下落到地面，
// 越过地面后被回收盒折回柱顶 —— 方向永远是"天上 → 地面"。
const float PRECIP_SPAWN_HALF_XZ = 30.0f;
const float PRECIP_COLUMN_HEIGHT = 24.0f;
const float PRECIP_MIN_INTENSITY = 0.05f; // 低于此强度不提交该类降水

GN3d::cPARTICLE_SYSTEM gRainParticles;
GN3d::cPARTICLE_SYSTEM gSnowParticles;
bool gPrecipReady = false;
// P11-S6b：地面贴花（雨滴溅射的水渍/环）。
GN3d::cDECAL_SYSTEM gDecals;
std::vector<GN3d::sDECAL_RENDER_ITEM> gDecalItems;
bool gDecalReady = false;
float gSplashAccumulator = 0.0f;
uint32_t gSplashRngState = 0x9E3779B9u;
// P11-S6d：包围盒 + 坐标轴可视化（G 键切换）。
bool gShowGizmos = true;
// S2b 中景 billboard 云层：默认关闭。它此前因为拿到单位矩阵而从未真正显示过（等于未验证），
// 矩阵修对后暴露为大片 256 m 方形贴片、接缝明显（糊在脸上），先用 C 键单独开关、默认关，
// 天空盒（S2a）仍提供程序化云。要正式启用需要单独做贴片尺度/接缝/透明度的调参。
bool gCloudLayerEnabled = false;
// 窗口左上角的坐标信息叠层（H 键切换）：模型/地面/脚下面片/相机的实际数值。
bool gShowCoordInfo = true;
// 分系统开关（排查用）：J 关/开降水粒子，K 关/开地面溅射贴花。
bool gPrecipEnabled = true;
bool gDecalEnabled = true;
// 叠层文字输出器：独立 cTEXT_ART（旧示例 samples/04_font.cpp 的用法），不依赖 RunRec 缓冲。
GNg::cTEXT_ART gInfoText;
// 叠层显示用的降水参数快照（updatePrecipitation 每帧写入）。
GNg::sVEC3 gPrecipBoxCenter = {0.0f, 0.0f, 0.0f};
GNg::sVEC3 gPrecipBoxHalf = {0.0f, 0.0f, 0.0f};
float gRainRate = 0.0f;
float gSnowRate = 0.0f;
GNg::sVEC3 gRainSamplePos = {0.0f, 0.0f, 0.0f};
GNg::sVEC3 gRainSampleVel = {0.0f, 0.0f, 0.0f};
bool gRainSampleValid = false;
GNg::sVEC3 gRainNearestPos = {0.0f, 0.0f, 0.0f};
float gRainNearestDist = -1.0f;
// 雪样本：叠层里显示它的速度，直接判断"雪到底在不在下落"。
GNg::sVEC3 gSnowSamplePos = {0.0f, 0.0f, 0.0f};
GNg::sVEC3 gSnowSampleVel = {0.0f, 0.0f, 0.0f};
bool gSnowSampleValid = false;
// 最近一帧的 viewProj 与 framebuffer 尺寸：叠层用它把样本粒子投影到屏幕，直接看屏幕方向。
GNg::sMAT44 gLastViewProj;
bool gLastViewProjValid = false;
int gFbWidth = 1280;
int gFbHeight = 800;
// 每个粒子的四边形包围盒（V 键开关）：只画离相机最近的若干个，全量 6 万+ 不可行。
bool gParticleBoxes = false;
const uint32_t PARTICLE_DEBUG_BOX_COUNT = 16;

float clampValue(float value, float low, float high) {
    if (value < low) {
        return low;
    }
    return value > high ? high : value;
}

// 把天气状态派生的天空参数推进引擎（每帧调用，云层随时间滚动）。
void applyWeather() {
    const GN3d::WeatherOutput out = gWeather.output();
    GNg::VkSkyParams sky;
    sky.sunDirection[0] = out.sky.sunDirection.x;
    sky.sunDirection[1] = out.sky.sunDirection.y;
    sky.sunDirection[2] = out.sky.sunDirection.z;
    sky.sunIntensity = out.sky.sunIntensity;
    sky.horizonColor[0] = out.sky.horizonColor.x;
    sky.horizonColor[1] = out.sky.horizonColor.y;
    sky.horizonColor[2] = out.sky.horizonColor.z;
    sky.zenithColor[0] = out.sky.zenithColor.x;
    sky.zenithColor[1] = out.sky.zenithColor.y;
    sky.zenithColor[2] = out.sky.zenithColor.z;
    sky.cloudCoverage = out.sky.cloudCoverage;
    sky.cloudSharpness = out.sky.cloudSharpness;
    sky.timeSeconds = out.sky.timeSeconds;
    sky.windX = out.precipitation.wind.x;
    sky.windZ = out.precipitation.wind.z;
    sky.exposure = 1.0f;
    GNg::GN_VkSetSky(sky);

    // 雾：weather.output() 已给出颜色/密度/高度衰减，maxOpacity 取 0.9（FOG/STORM 才有可见浓度）。
    GNg::sFOG_UBO fog;
    std::memset(&fog, 0, sizeof(fog));
    fog.aColor[0] = out.fog.color.x;
    fog.aColor[1] = out.fog.color.y;
    fog.aColor[2] = out.fog.color.z;
    fog.aColor[3] = 1.0f;
    fog.aParams[0] = out.fog.density;
    fog.aParams[1] = out.fog.heightFalloff;
    fog.aParams[2] = out.fog.baseHeight;
    fog.aParams[3] = FOG_MAX_OPACITY;
    GNg::GN_VkSetFog(fog);
}

// 相机位置（世界空间）。
GNg::sVEC3 cameraPosition() {
    return gCameraPosition;
}

// 相机前向 / 右向 / 上向：由朝向四元数旋转局部基向量得到。
void cameraBasisVectors(GNg::sVEC3 &forward, GNg::sVEC3 &right, GNg::sVEC3 &up) {
    const GNg::sVEC3 forwardLocal = {0.0f, 0.0f, -1.0f};
    const GNg::sVEC3 rightLocal = {1.0f, 0.0f, 0.0f};
    const GNg::sVEC3 upLocal = {0.0f, 1.0f, 0.0f};
    GNg::QuatRotateVec3(&forward, &forwardLocal, &gCameraRotation);
    GNg::QuatRotateVec3(&right, &rightLocal, &gCameraRotation);
    GNg::QuatRotateVec3(&up, &upLocal, &gCameraRotation);
}

// 相机注视点：沿前向 1 m 处（LookAt 的 center）。
GNg::sVEC3 cameraTarget() {
    GNg::sVEC3 forward;
    GNg::sVEC3 right;
    GNg::sVEC3 up;
    cameraBasisVectors(forward, right, up);
    GNg::sVEC3 target;
    target.x = gCameraPosition.x + forward.x;
    target.y = gCameraPosition.y + forward.y;
    target.z = gCameraPosition.z + forward.z;
    return target;
}

// 相机到模型中心的距离：远平面、降水回收盒、溅射半径共用。
float cameraDistance() {
    GNg::sVEC3 offset;
    GNg::Vec3Sub(&offset, &gCameraPosition, &gTarget);
    return std::sqrt(offset.x * offset.x + offset.y * offset.y + offset.z * offset.z);
}

// 相机基向量：forward 指向注视点，right/up 供降水 billboard 展开使用。
void cameraBasis(const GNg::sVEC3 &camera, GNg::sVEC3 &right, GNg::sVEC3 &camUp) {
    GNg::sVEC3 forward;
    const GNg::sVEC3 target = cameraTarget();
    GNg::Vec3Sub(&forward, &target, &camera);
    GNg::Vec3Normalize(&forward, &forward);
    const GNg::sVEC3 worldUp = {0.0f, 1.0f, 0.0f};
    GNg::Vec3Cross(&right, &forward, &worldUp);
    GNg::Vec3Normalize(&right, &right);
    GNg::Vec3Cross(&camUp, &right, &forward);
}

// 施加一次视角增量：yawRad 为正表示向右转（绕世界 +Y），pitchRad 为正表示向上看
// （绕相机右轴）。俯仰越界时整段丢弃垂直分量，避免翻过头。
void applyLookDelta(float yawRad, float pitchRad) {
    if (yawRad != 0.0f) {
        GNg::sQUAT yawQuat;
        const GNg::sVEC3 worldUp = {0.0f, 1.0f, 0.0f};
        GNg::QuatFromAxisAngle(&yawQuat, &worldUp, -yawRad);
        gCameraRotation = yawQuat * gCameraRotation;
        GNg::QuatNormalize(&gCameraRotation, &gCameraRotation);
    }
    if (pitchRad != 0.0f) {
        GNg::sVEC3 forwardNow;
        GNg::sVEC3 rightNow;
        GNg::sVEC3 upNow;
        cameraBasisVectors(forwardNow, rightNow, upNow);
        const float pitchNow = asinf(clampValue(-forwardNow.y, -1.0f, 1.0f));
        const float pitchNext = pitchNow + pitchRad;
        if (pitchNext <= VIEW_PITCH_LIMIT && pitchNext >= -VIEW_PITCH_LIMIT) {
            GNg::sQUAT pitchQuat;
            GNg::QuatFromAxisAngle(&pitchQuat, &rightNow, pitchRad);
            gCameraRotation = pitchQuat * gCameraRotation;
            GNg::QuatNormalize(&gCameraRotation, &gCameraRotation);
        }
    }
}

// 回到默认正面视角（统一坐标系 v2：资产正面 +Z）：相机放在模型 +Z 侧、略高于中心，
// yaw = 0 看向 -Z，俯角取负值（向下看）DEFAULT_VIEW_PITCH。
void resetCamera() {
    const float distance = gHeight * DEFAULT_DISTANCE_FACTOR;
    const float cp = cosf(DEFAULT_VIEW_PITCH);
    const float sp = sinf(DEFAULT_VIEW_PITCH);
    gCameraPosition.x = gTarget.x;
    gCameraPosition.y = gTarget.y + distance * sp;
    gCameraPosition.z = gTarget.z + distance * cp;

    GNg::sQUAT yawQuat;
    GNg::sQUAT pitchQuat;
    const GNg::sVEC3 worldUp = {0.0f, 1.0f, 0.0f};
    const GNg::sVEC3 rightAxis = {1.0f, 0.0f, 0.0f};
    GNg::QuatFromAxisAngle(&yawQuat, &worldUp, 0.0f);
    GNg::QuatFromAxisAngle(&pitchQuat, &rightAxis, -DEFAULT_VIEW_PITCH);
    gCameraRotation = pitchQuat * yawQuat;
    GNg::QuatNormalize(&gCameraRotation, &gCameraRotation);
    gLastMouseValid = false;
}

// 鼠标拖拽转视角：按住左/右键时按光标绝对位置求增量（不依赖引擎 MouseM 的帧时序，
// 松开或首次按下时重新锚定，避免把陈旧增量当成这一帧的位移）。
void updateMouseLook() {
    if (!GNg::ViewCur) {
        return;
    }
    const int mx = GNg::ViewCur->mouseMX;
    const int my = GNg::ViewCur->mouseMY;
    const bool dragging = (GN::Mouse[GM_LEFT] != 0) || (GN::Mouse[GM_RIGHT] != 0);
    if (!dragging) {
        gLastMouseValid = false;
        gLastMouseX = mx;
        gLastMouseY = my;
        return;
    }
    if (!gLastMouseValid) {
        gLastMouseValid = true;
        gLastMouseX = mx;
        gLastMouseY = my;
        return;
    }
    const int dx = mx - gLastMouseX;
    const int dy = my - gLastMouseY;
    gLastMouseX = mx;
    gLastMouseY = my;
    if (dx == 0 && dy == 0) {
        return;
    }
    // 屏幕 y 向下：鼠标上移 (dy<0) 对应抬头。
    applyLookDelta(static_cast<float>(dx) * MOUSE_LOOK_SPEED,
                   -static_cast<float>(dy) * MOUSE_LOOK_SPEED);
}

// 创建雨/雪两套 BOX 粒子系统；create 会清空回收盒，故回收盒每帧在 updatePrecipitation 里重设。
bool createPrecipitationSystems() {
    // 发射器是顶部的一片水平面（y 半范围 0）：粒子从"天上的面片"往下发。
    const GNg::sVEC3 halfExtent = {PRECIP_SPAWN_HALF_XZ, 0.0f, PRECIP_SPAWN_HALF_XZ};

    GN3d::sPARTICLE_EMITTER_DESC rain;
    rain.aOrigin = {0.0f, 0.0f, 0.0f};
    rain.aDirection = {0.0f, PRECIP_DIR_Y, 0.0f}; // 向下落
    rain.aSpread = 0.1f;
    rain.aSpeedMin = 18.0f;
    rain.aSpeedMax = 22.0f;
    rain.aRate = 0.0f; // 每帧按天气强度 setRate
    rain.aMaxParticles = RAIN_CAPACITY;
    // 寿命要够落满整根柱（24 m / ~20 m/s ≈ 1.2 s，取 1.6–2.0 s 保证落到地面再回收）。
    rain.aLifeMin = 1.6f;
    rain.aLifeMax = 2.0f;
    rain.aGravity = {0.0f, RAIN_GRAVITY_Y, 0.0f};
    rain.aDrag = 0.0f;
    rain.aColorStart = GNg::sVEC4(0.82f, 0.90f, 1.0f, 0.85f);
    rain.aColorEnd = GNg::sVEC4(0.82f, 0.90f, 1.0f, 0.35f);
    rain.aBoxHalfExtent = halfExtent;
    // 宽度 5–9 cm：在 30 m 处约 2–4 px，配合 14 倍拉长才有可见雨丝。
    rain.aSizeMin = RAIN_SIZE_MIN;
    rain.aSizeMax = RAIN_SIZE_MAX;
    rain.aWind = {0.0f, 0.0f, 0.0f};
    rain.aWindResponse = 0.6f;
    rain.aShape = GN3d::ParticleShape::BOX;
    // 帧内相位打散：否则每帧一批粒子都从同一高度出发，近处看是一条条横向"片子"。
    rain.aSpreadSpawnPhase = true;
    if (!gRainParticles.create(rain)) {
        return false;
    }

    GN3d::sPARTICLE_EMITTER_DESC snow = rain;
    snow.aSpread = 0.25f;
    snow.aSpeedMin = 2.0f;
    snow.aSpeedMax = 4.0f;
    snow.aMaxParticles = SNOW_CAPACITY;
    // 雪终端速度 ~3.75 m/s：24 m 柱约 6.4 s，寿命给 8–10 s 才能落到地面。
    snow.aLifeMin = 8.0f;
    snow.aLifeMax = 10.0f;
    // 雪的重力取小值：风响应 0.4 下终端速度 ≈ g/0.4 ≈ 3.75 m/s，保持 2..4 m/s 的慢速飘落。
    snow.aGravity = {0.0f, SNOW_GRAVITY_Y, 0.0f};
    snow.aSizeMin = SNOW_SIZE_MIN;
    snow.aSizeMax = SNOW_SIZE_MAX;
    snow.aColorStart = GNg::sVEC4(0.97f, 0.98f, 1.0f, 0.95f);
    snow.aColorEnd = GNg::sVEC4(0.97f, 0.98f, 1.0f, 0.50f);
    snow.aWindResponse = 0.4f;
    if (!gSnowParticles.create(snow)) {
        return false;
    }

    gPrecipReady = true;
    return true;
}

// 模型世界 AABB 最低点（脚下地面高度）；定义见下方「地面 + 贴花」段。
float groundHeight();

// 每帧：按天气强度更新发射率/风/回收盒，再推进粒子模拟。
// 发射点与回收盒都在世界空间里钉死（围着角色），不随相机移动。
void updatePrecipitation(float dt) {
    if (!gPrecipReady) {
        return;
    }
    const GN3d::WeatherOutput out = gWeather.output();
    // 固定降水柱：顶部发射平面在角色头顶上方，回收盒是同一根柱子（底=地面，顶=发射平面）。
    const float groundY = groundHeight();
    const GNg::sVEC3 emitterPlane = {gTarget.x, groundY + PRECIP_COLUMN_HEIGHT, gTarget.z};
    const GNg::sVEC3 columnCenter = {gTarget.x, groundY + PRECIP_COLUMN_HEIGHT * 0.5f, gTarget.z};
    const GNg::sVEC3 columnHalf = {PRECIP_SPAWN_HALF_XZ, PRECIP_COLUMN_HEIGHT * 0.5f,
                                   PRECIP_SPAWN_HALF_XZ};
    const GNg::sVEC3 wind = {out.precipitation.wind.x, 0.0f, out.precipitation.wind.z};

    // 发射率按池容量反推：只需在寿命内填满池即可（见文件头 RAIN/SNOW_CAPACITY 注释）。
    gRainRate = 60000.0f * out.precipitation.rain;
    gSnowRate = 30000.0f * out.precipitation.snow;
    gRainParticles.setRate(gRainRate);
    gSnowParticles.setRate(gSnowRate);
    gRainParticles.setWind(wind, 0.6f);
    gSnowParticles.setWind(wind, 0.4f);
    gRainParticles.setOrigin(emitterPlane);
    gSnowParticles.setOrigin(emitterPlane);
    gRainParticles.setRecycleBox(columnCenter, columnHalf);
    gSnowParticles.setRecycleBox(columnCenter, columnHalf);
    gRainParticles.update(dt);
    gSnowParticles.update(dt);

    // 叠层快照：发射率/回收盒、一颗雨的实时状态，以及离相机最近的雨粒子。
    gPrecipBoxCenter = columnCenter;
    gPrecipBoxHalf = columnHalf;
    gRainSampleValid = false;
    gRainNearestDist = -1.0f;
    const GNg::sVEC3 cameraNow = cameraPosition();
    const GN3d::sPARTICLE *rainParticles = gRainParticles.particles();
    for (uint32_t i = 0; rainParticles != nullptr && i < gRainParticles.capacity(); ++i) {
        if (!gRainParticles.alive(i)) {
            continue;
        }
        const GNg::sVEC3 &pos = rainParticles[i].aPosition;
        if (!gRainSampleValid) {
            gRainSamplePos = pos;
            gRainSampleVel = rainParticles[i].aVelocity;
            gRainSampleValid = true;
        }
        const float ddx = pos.x - cameraNow.x;
        const float ddy = pos.y - cameraNow.y;
        const float ddz = pos.z - cameraNow.z;
        const float dist = std::sqrt(ddx * ddx + ddy * ddy + ddz * ddz);
        if (gRainNearestDist < 0.0f || dist < gRainNearestDist) {
            gRainNearestDist = dist;
            gRainNearestPos = pos;
        }
    }

    // 雪的样本：只取第一颗存活的，用来在叠层里核对它的速度 Y（应当为负 = 向下落）。
    gSnowSampleValid = false;
    const GN3d::sPARTICLE *snowParticles = gSnowParticles.particles();
    for (uint32_t i = 0; snowParticles != nullptr && i < gSnowParticles.capacity(); ++i) {
        if (gSnowParticles.alive(i)) {
            gSnowSamplePos = snowParticles[i].aPosition;
            gSnowSampleVel = snowParticles[i].aVelocity;
            gSnowSampleValid = true;
            break;
        }
    }
}

// 3D pass 内、天空盒之后：中景云层随天气云量/锐度/风变化。
void drawClouds(const GNg::sMAT44 &viewProjection, const GNg::sVEC3 &camera) {
    const GN3d::WeatherOutput out = gWeather.output();
    GNg::sCLOUD_DRAW_PARAMS params;
    params.aViewProj = viewProjection;
    params.aCameraPos = camera;
    params.aLayerHeight = CLOUD_LAYER_HEIGHT;
    params.aWind = {out.precipitation.wind.x, 0.0f, out.precipitation.wind.z};
    params.aTileSize = CLOUD_TILE_SIZE;
    params.aTilesPerAxis = CLOUD_TILES_PER_AXIS;
    // 云量打个折 + 不透明度 <1：否则暴风雨下三层云盘会叠成一整块灰"天花板"，看着像地面翻到天上。
    params.aCoverage = out.sky.cloudCoverage * 0.8f;
    params.aSharpness = out.sky.cloudSharpness;
    params.aOpacity = 0.85f;
    params.aTimeSeconds = out.sky.timeSeconds;
    params.aNearFade = CLOUD_NEAR_FADE;
    GNg::GN_VkDrawCloud(params);
}

void collectAliveParticles(const GN3d::cPARTICLE_SYSTEM &system,
                           std::vector<GN3d::sPARTICLE> &out) {
    const GN3d::sPARTICLE *particles = system.particles();
    const uint32_t capacity = system.capacity();
    if (!particles) {
        return;
    }
    out.clear();
    for (uint32_t i = 0; i < capacity; ++i) {
        if (system.alive(i)) {
            out.push_back(particles[i]);
        }
    }
}

// 提交一类降水（雨丝 streak≈14 / 雪团 streak=1，形状由片元按 streak 选）。
void submitPrecipitation(const GNg::sMAT44 &viewProjection, const GNg::sVEC3 &right,
                         const GNg::sVEC3 &camUp, const std::vector<GN3d::sPARTICLE> &particles,
                         float streakScale) {
    if (particles.empty()) {
        return;
    }
    GNg::sPRECIP_DRAW_PARAMS params;
    params.aViewProj = viewProjection;
    params.aCameraRight = right;
    params.aCameraUp = camUp;
    params.aParticles = particles.data();
    params.aCount = static_cast<uint32_t>(particles.size());
    params.aStreakScale = streakScale;
    params.aAlphaScale = 1.0f;
    if (!GNg::GN_VkDrawPrecipitation(params)) {
        // 提交被拒（管线创建失败 / 不在 3D pass 内 / 参数非法）：每秒最多打一次，便于定位"画不出来"。
        static uint32_t rejectedFrames = 0;
        ++rejectedFrames;
        if ((rejectedFrames % 60u) == 1u) {
            std::fprintf(stderr, "[mmdViewer] 降水提交被拒绝: count=%u streak=%.1f\n",
                         params.aCount, streakScale);
        }
    }
}

// 降水模块有 4 个顶点缓冲槽位（按帧奇偶 + 帧内调用序号分配）⇒ 同帧可分别提交雨和雪，
// 各自用自己的 streak（雨丝 vs 雪团），不需要再合并成一种形状。
void drawPrecipitation(const GNg::sMAT44 &viewProjection, const GNg::sVEC3 &right,
                       const GNg::sVEC3 &camUp) {
    if (!gPrecipReady || !gPrecipEnabled) {
        return;
    }
    const GN3d::WeatherOutput out = gWeather.output();
    const bool rainActive = out.precipitation.rain > PRECIP_MIN_INTENSITY;
    const bool snowActive = out.precipitation.snow > PRECIP_MIN_INTENSITY;
    if (!rainActive && !snowActive) {
        return;
    }

    std::vector<GN3d::sPARTICLE> particles;
    if (snowActive) {
        collectAliveParticles(gSnowParticles, particles);
        submitPrecipitation(viewProjection, right, camUp, particles, SNOW_STREAK);
    }
    if (rainActive) {
        collectAliveParticles(gRainParticles, particles);
        submitPrecipitation(viewProjection, right, camUp, particles, RAIN_STREAK);
    }
}

// ── P11-S6b：地面 + 贴花 ────
// 地面高度取模型世界 AABB 的最低点（模型站在地面上）。
float groundHeight() {
    if (gModel.aWorldAABB.IsValid()) {
        return gModel.aWorldAABB.aMin.y;
    }
    return 0.0f;
}

// 固定种子 xorshift32：同一天气序列的溅射位置可重复。
float splashRandom() {
    gSplashRngState ^= gSplashRngState << 13;
    gSplashRngState ^= gSplashRngState >> 17;
    gSplashRngState ^= gSplashRngState << 5;
    return (float)(gSplashRngState & 0xFFFFFFu) / 16777216.0f;
}

void createDecals() {
    if (!gDecals.create(512)) {
        std::fprintf(stderr, "[mmdViewer] 贴花池创建失败：雨天地面不会有水渍\n");
        return;
    }
    gDecalItems.resize(gDecals.capacity());
    gDecalReady = true;
}

// 地面：一块 400 m 的正方形，走 **PBR 管线**（`GN_VkDraw3DPBR`）。gpuCheck 的
// ground.placement 验证过这条路径；绕序 0,2,1 / 0,3,2 是 CCW（法线 +Y），
// 与 U2b 后 PBR 的 frontFace=CW 匹配（CCW 内容 + 投影 Y 翻转 ⇒ 帧缓冲 CW）。
void drawGround() {
    const float y = groundHeight() - 0.02f;
    const float half = 200.0f;
    const float xs[4] = {-half, half, half, -half};
    const float zs[4] = {-half, -half, half, half};
    float vertices[4 * 11];
    std::memset(vertices, 0, sizeof(vertices));
    for (int i = 0; i < 4; ++i) {
        float *v = vertices + i * 11;
        v[0] = xs[i];
        v[1] = y;
        v[2] = zs[i];
        // 法线 +Y：地面几何绕序是 CCW（0,2,1 → 法线 +Y）；旧的 -Y 是 PBR 负高度视口
        // 时期的镜像补丁，已在 U2 统一为正高度视口 + CCW 世界后废弃。
        v[3] = 0.0f;
        v[4] = 1.0f;
        v[5] = 0.0f;
        v[6] = 1.0f; // 切线 +X（布局：pos0-2 / normal3-5 / tangent6-8 / uv9-10）
        v[9] = (xs[i] + half) / (2.0f * half);
        v[10] = (zs[i] + half) / (2.0f * half);
    }
    // 地面走 PBR 管线；0,2,1 / 0,3,2 已是 CCW（法线 +Y），配合 U2b 的 PBR frontFace=CW。
    const uint32_t indices[6] = {0, 2, 1, 0, 3, 2};
    GNg::sMAT44 identity;
    GNg::Mat44Identity(&identity);
    const GNg::sMAT44 *model = &identity;
    GNg::GN_VkDraw3DPBR(vertices, 4, indices, 6, model, 0.45f, 0.46f, 0.42f, 0.95f, 0.0f, 1.0f);
}

// 脚下面片的半边长：绘制、包围盒与坐标叠层共用同一口径，避免三处各算一套。
float footQuadHalf() {
    return (gHeight > 1.0f ? gHeight : 1.0f) * 0.35f;
}

// 脚下一块普通四边形（走**前向管线**，和角色同一条路径），用来把「地面」和「普通场景面片」
// 分开看：它在脚下 ⇒ 前向管线与相机矩阵是对的，跑到头顶的只是只走 PBR 管线的地面；
// 它也在头顶 ⇒ 问题才在相机/矩阵。颜色用高饱和红，便于和灰色地面对照。
void drawFootQuad() {
    const float y = groundHeight() + 0.02f;
    const float half = footQuadHalf();
    const float xs[4] = {gTarget.x - half, gTarget.x + half, gTarget.x + half, gTarget.x - half};
    const float zs[4] = {gTarget.z - half, gTarget.z - half, gTarget.z + half, gTarget.z + half};
    float vertices[4 * 11];
    std::memset(vertices, 0, sizeof(vertices));
    for (int i = 0; i < 4; ++i) {
        float *v = vertices + i * 11;
        v[0] = xs[i];
        v[1] = y;
        v[2] = zs[i];
        v[3] = 0.0f; // 法线 +Y：前向管线是标准竖直约定
        v[4] = 1.0f;
        v[5] = 0.0f;
        v[6] = 1.0f; // 切线 +X（布局：pos0-2 / normal3-5 / tangent6-8 / uv9-10）
        v[9] = (i == 1 || i == 2) ? 1.0f : 0.0f;
        v[10] = (i >= 2) ? 1.0f : 0.0f;
    }
    // 与地面相同的绕序：正/反面判定在「PBR 负高度视口 + CW」和「前向正高度视口 + CCW」下等价。
    const uint32_t indices[6] = {0, 2, 1, 0, 3, 2};
    GNg::sMAT44 identity;
    GNg::Mat44Identity(&identity);
    GNg::GN_VkDraw3D(vertices, 4, indices, 6, &identity, 1.0f, 0.2f, 0.15f, 0.25f, 0.25f, 0.25f,
                     8.0f, nullptr);
}

// 雨滴溅射：按雨强在角色周围的地面生成水渍 / 溅射环（单帧上限 64 条，避免爆发卡顿）。
void spawnSplashes(float dt) {
    if (!gDecalReady) {
        return;
    }
    const GN3d::WeatherOutput out = gWeather.output();
    const float groundY = groundHeight() + 0.02f;
    const float distance = cameraDistance() > 1.0f ? cameraDistance() : 1.0f;
    const float radius = distance * 0.6f + 4.0f;
    gSplashAccumulator += 220.0f * out.precipitation.rain * dt;
    uint32_t budget = 64;
    while (gSplashAccumulator >= 1.0f && budget > 0) {
        gSplashAccumulator -= 1.0f;
        --budget;
        const float angle = splashRandom() * 6.2831853f;
        const float dist = radius * std::sqrt(splashRandom());
        GN3d::sDECAL decal;
        std::memset(&decal, 0, sizeof(decal));
        decal.aPosition = {gTarget.x + std::cos(angle) * dist, groundY,
                           gTarget.z + std::sin(angle) * dist};
        decal.aSize = 0.25f + 0.45f * splashRandom();
        decal.aColor = GNg::sVEC4(0.78f, 0.84f, 0.95f, 0.55f);
        decal.aLife = 0.5f + 0.6f * splashRandom();
        decal.aKind = (splashRandom() < 0.35f) ? (uint32_t)GN3d::DecalKind::SPLASH_RING
                                                : (uint32_t)GN3d::DecalKind::SPOT;
        gDecals.spawn(decal);
    }
}

void updateDecals(float dt) {
    if (!gDecalReady) {
        return;
    }
    spawnSplashes(dt);
    gDecals.update(dt);
}

void drawDecals(const GNg::sMAT44 &viewProjection) {
    if (!gDecalEnabled || !gDecalReady || gDecalItems.empty()) {
        return;
    }
    const uint32_t count = GN3d::gatherDecalRenderItems(gDecals, gDecalItems.data(),
                                                        (uint32_t)gDecalItems.size());
    if (count == 0) {
        return;
    }
    GNg::sDECAL_DRAW_PARAMS params;
    params.aViewProj = viewProjection;
    params.aItems = gDecalItems.data();
    params.aCount = count;
    params.aAlphaScale = 1.0f;
    GNg::GN_VkDrawDecals(params);
}

// ── P11-S6d：包围盒 + 坐标轴 ────
// 世界坐标轴：+X 红、+Y 绿、+Z 蓝（长度随模型高度缩放），画在原点。
void drawAxes() {
    const float len = (gHeight > 1.0f ? gHeight : 1.0f) * 0.6f;
    const GNg::sVEC3 origin = {0.0f, 0.0f, 0.0f};
    GN3d::sDEBUG_LINE axis[1];
    axis[0].aStart = origin;
    axis[0].aEnd = {len, 0.0f, 0.0f};
    GNg::GN_VkDrawDebugLines(axis, 1, 0xFFFF0000); // +X 红
    axis[0].aEnd = {0.0f, len, 0.0f};
    GNg::GN_VkDrawDebugLines(axis, 1, 0xFF00FF00); // +Y 绿
    axis[0].aEnd = {0.0f, 0.0f, len};
    GNg::GN_VkDrawDebugLines(axis, 1, 0xFF0000FF); // +Z 蓝
}

// 模型世界 AABB 的 12 条棱（黄色线框）；无效盒子不画。
void drawModelAabb() {
    if (!gModel.aWorldAABB.IsValid()) {
        return;
    }
    const GNg::sVEC3 &lo = gModel.aWorldAABB.aMin;
    const GNg::sVEC3 &hi = gModel.aWorldAABB.aMax;
    const GNg::sVEC3 corners[8] = {
        {lo.x, lo.y, lo.z}, {hi.x, lo.y, lo.z}, {hi.x, hi.y, lo.z}, {lo.x, hi.y, lo.z},
        {lo.x, lo.y, hi.z}, {hi.x, lo.y, hi.z}, {hi.x, hi.y, hi.z}, {lo.x, hi.y, hi.z}};
    const uint32_t edges[12][2] = {{0, 1}, {1, 2}, {2, 3}, {3, 0}, {4, 5}, {5, 6},
                                   {6, 7}, {7, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
    GN3d::sDEBUG_LINE lines[12];
    for (int i = 0; i < 12; ++i) {
        lines[i].aStart = corners[edges[i][0]];
        lines[i].aEnd = corners[edges[i][1]];
    }
    GNg::GN_VkDrawDebugLines(lines, 12, 0xFFFFFF00); // 黄色 AABB
}

// 每颗粒子四个角点构成的四边形 → 世界 AABB 的 12 条棱。
// 只取离相机最近的 PARTICLE_DEBUG_BOX_COUNT 个：全量 6~8 万 = 百万级线段，不可行。
// 顶点公式与 vkPrecipitation.cpp 的 writeParticleQuad 对齐（forward = right × up）。
// 雨、雪各调用一次（streakScale 区分雨丝/雪团）：雪天雨池为空，只画雨会看不到任何盒子。
void drawParticleDebugBoxesFor(const GN3d::cPARTICLE_SYSTEM &system, float streakScale,
                               const GNg::sVEC3 &right, const GNg::sVEC3 &camUp) {
    if (!gParticleBoxes || !gPrecipReady) {
        return;
    }
    const GN3d::sPARTICLE *particles = system.particles();
    if (particles == nullptr) {
        return;
    }
    GNg::sVEC3 forward;
    GNg::Vec3Cross(&forward, &right, &camUp);
    const float fl = std::sqrt(forward.x * forward.x + forward.y * forward.y + forward.z * forward.z);
    if (fl <= 1.0e-6f) {
        return;
    }
    forward.x /= fl;
    forward.y /= fl;
    forward.z /= fl;

    const GNg::sVEC3 camera = cameraPosition();
    uint32_t best[PARTICLE_DEBUG_BOX_COUNT];
    float bestDist[PARTICLE_DEBUG_BOX_COUNT];
    uint32_t found = 0;
    uint32_t worst = 0;
    for (uint32_t i = 0; i < system.capacity(); ++i) {
        if (!system.alive(i)) {
            continue;
        }
        const GNg::sVEC3 &p = particles[i].aPosition;
        const float dx = p.x - camera.x;
        const float dy = p.y - camera.y;
        const float dz = p.z - camera.z;
        const float dist = dx * dx + dy * dy + dz * dz;
        if (found < PARTICLE_DEBUG_BOX_COUNT) {
            best[found] = i;
            bestDist[found] = dist;
            ++found;
            if (found == PARTICLE_DEBUG_BOX_COUNT) {
                worst = 0;
                for (uint32_t k = 1; k < found; ++k) {
                    if (bestDist[k] > bestDist[worst]) worst = k;
                }
            }
        } else if (dist < bestDist[worst]) {
            best[worst] = i;
            bestDist[worst] = dist;
            for (uint32_t k = 1; k < found; ++k) {
                if (bestDist[k] > bestDist[worst]) worst = k;
            }
        }
    }

    const uint32_t edges[12][2] = {{0, 1}, {1, 2}, {2, 3}, {3, 0}, {4, 5}, {5, 6},
                                   {6, 7}, {7, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
    GN3d::sDEBUG_LINE lines[PARTICLE_DEBUG_BOX_COUNT * 12];
    uint32_t lineCount = 0;
    for (uint32_t n = 0; n < found; ++n) {
        const GN3d::sPARTICLE &pt = particles[best[n]];
        GNg::sVEC3 sd = pt.aVelocity;
        float sl = std::sqrt(sd.x * sd.x + sd.y * sd.y + sd.z * sd.z);
        if (sl <= 1.0e-6f) {
            sd = camUp;
            sl = std::sqrt(sd.x * sd.x + sd.y * sd.y + sd.z * sd.z);
        }
        if (sl <= 1.0e-6f) {
            continue;
        }
        sd.x /= sl;
        sd.y /= sl;
        sd.z /= sl;
        GNg::sVEC3 side;
        GNg::Vec3Cross(&side, &sd, &forward);
        float wl = std::sqrt(side.x * side.x + side.y * side.y + side.z * side.z);
        if (wl <= 1.0e-6f) {
            side = right;
            wl = std::sqrt(side.x * side.x + side.y * side.y + side.z * side.z);
        }
        if (wl <= 1.0e-6f) {
            continue;
        }
        side.x /= wl;
        side.y /= wl;
        side.z /= wl;

        const float halfWidth = pt.aSize * 0.5f;
        const float halfLength = halfWidth * streakScale;
        GNg::sVEC3 corners[4];
        for (int q = 0; q < 4; ++q) {
            const float sideSign = (q == 0 || q == 3) ? -1.0f : 1.0f;
            const float alongSign = (q < 2) ? 1.0f : -1.0f;
            corners[q].x = pt.aPosition.x + side.x * sideSign * halfWidth + sd.x * alongSign * halfLength;
            corners[q].y = pt.aPosition.y + side.y * sideSign * halfWidth + sd.y * alongSign * halfLength;
            corners[q].z = pt.aPosition.z + side.z * sideSign * halfWidth + sd.z * alongSign * halfLength;
        }
        GNg::sVEC3 lo = corners[0];
        GNg::sVEC3 hi = corners[0];
        for (int q = 1; q < 4; ++q) {
            lo.x = corners[q].x < lo.x ? corners[q].x : lo.x;
            lo.y = corners[q].y < lo.y ? corners[q].y : lo.y;
            lo.z = corners[q].z < lo.z ? corners[q].z : lo.z;
            hi.x = corners[q].x > hi.x ? corners[q].x : hi.x;
            hi.y = corners[q].y > hi.y ? corners[q].y : hi.y;
            hi.z = corners[q].z > hi.z ? corners[q].z : hi.z;
        }
        const GNg::sVEC3 box[8] = {
            {lo.x, lo.y, lo.z}, {hi.x, lo.y, lo.z}, {hi.x, hi.y, lo.z}, {lo.x, hi.y, lo.z},
            {lo.x, lo.y, hi.z}, {hi.x, lo.y, hi.z}, {hi.x, hi.y, hi.z}, {lo.x, hi.y, hi.z}};
        for (int e = 0; e < 12; ++e) {
            lines[lineCount].aStart = box[edges[e][0]];
            lines[lineCount].aEnd = box[edges[e][1]];
            ++lineCount;
        }
    }
    if (lineCount > 0) {
        GNg::GN_VkDrawDebugLines(lines, lineCount, 0xFFFF9900); // 橙色：粒子四边形 AABB
    }
}

// V 键：雨、雪两套粒子系统各画最近的若干四边形 AABB（旧版只画雨，雪天看不到任何盒子）。
void drawParticleDebugBoxes(const GNg::sVEC3 &right, const GNg::sVEC3 &camUp) {
    if (!gParticleBoxes || !gPrecipReady) {
        return;
    }
    drawParticleDebugBoxesFor(gRainParticles, RAIN_STREAK, right, camUp);
    drawParticleDebugBoxesFor(gSnowParticles, SNOW_STREAK, right, camUp);
}

// 世界点 → 屏幕像素（返回 false = 在相机后方/退化）。用于把样本粒子投影成屏幕坐标。
bool projectPointToScreen(const GNg::sMAT44 &vp, const GNg::sVEC3 &p, float *outX, float *outY) {
    const float *m = vp.m;
    const float cx = m[0] * p.x + m[4] * p.y + m[8] * p.z + m[12];
    const float cy = m[1] * p.x + m[5] * p.y + m[9] * p.z + m[13];
    const float cw = m[3] * p.x + m[7] * p.y + m[11] * p.z + m[15];
    if (cw <= 1.0e-6f) {
        return false;
    }
    *outX = (cx / cw * 0.5f + 0.5f) * static_cast<float>(gFbWidth);
    *outY = (cy / cw * 0.5f + 0.5f) * static_cast<float>(gFbHeight);
    return true;
}

// 坐标信息叠层：把模型 / 400 m 地面 / 脚下面片 / 相机的实际世界坐标打到窗口左上角。
// 文本走引擎 RunRec 的 2D 文本层，在帧末由 GF_FrameCycleProc2 统一绘制（H 键开关）。
void drawCoordOverlay() {
    if (!gShowCoordInfo || !GNg::ViewCur) {
        return;
    }
    int lineHeight = static_cast<int>(gInfoText.GetFontRangeY()) + 2;
    if (lineHeight < 8) {
        lineHeight = 18;
    }
    const int x = GNg::ViewCur->ortho_left + 16;
    int y = GNg::ViewCur->ortho_top + 16;
    char line[256];

    const GNg::sVEC3 &lo = gModel.aWorldAABB.aMin;
    const GNg::sVEC3 &hi = gModel.aWorldAABB.aMax;
    if (gModel.aWorldAABB.IsValid()) {
        std::snprintf(line, sizeof(line), "model AABB  min(%.2f %.2f %.2f)  max(%.2f %.2f %.2f)",
                      lo.x, lo.y, lo.z, hi.x, hi.y, hi.z);
    } else {
        std::snprintf(line, sizeof(line), "model AABB  (invalid)");
    }
    gInfoText.OutText(line, 0, x, y, 0, 0, 0xFFFFFFFF, GNg::eBLEND_NORMAL);
    y += lineHeight;

    std::snprintf(line, sizeof(line), "ground quad  y=%.3f  x/z=[-200.00, 200.00]  (400 x 400)",
                  groundHeight() - 0.02f);
    gInfoText.OutText(line, 0, x, y, 0, 0, 0xFF00FFFF, GNg::eBLEND_NORMAL);
    y += lineHeight;

    const float half = footQuadHalf();
    std::snprintf(line, sizeof(line),
                  "foot quad    y=%.3f  half=%.2f  x=[%.2f, %.2f]  z=[%.2f, %.2f]",
                  groundHeight() + 0.02f, half, gTarget.x - half, gTarget.x + half,
                  gTarget.z - half, gTarget.z + half);
    gInfoText.OutText(line, 0, x, y, 0, 0, 0xFFFF00FF, GNg::eBLEND_NORMAL);
    y += lineHeight;

    const GNg::sVEC3 camera = cameraPosition();
    GNg::sVEC3 forward;
    GNg::sVEC3 right;
    GNg::sVEC3 up;
    cameraBasisVectors(forward, right, up);
    std::snprintf(line, sizeof(line), "camera  pos(%.2f %.2f %.2f)  fwd(%.2f %.2f %.2f)",
                  camera.x, camera.y, camera.z, forward.x, forward.y, forward.z);
    gInfoText.OutText(line, 0, x, y, 0, 0, 0xFFFFFFFF, GNg::eBLEND_NORMAL);
    y += lineHeight;

    std::snprintf(line, sizeof(line), "dist=%.2f", cameraDistance());
    gInfoText.OutText(line, 0, x, y, 0, 0, 0xFFFFFFFF, GNg::eBLEND_NORMAL);
    y += lineHeight;

    std::snprintf(line, sizeof(line),
                  "rain   alive=%u/%u  rate=%.0f/s  size=%.3f..%.3f  streak=%.0f",
                  gRainParticles.activeCount(), gRainParticles.capacity(), gRainRate, RAIN_SIZE_MIN,
                  RAIN_SIZE_MAX, RAIN_STREAK);
    gInfoText.OutText(line, 0, x, y, 0, 0, 0xFF66DDFF, GNg::eBLEND_NORMAL);
    y += lineHeight;

    std::snprintf(line, sizeof(line),
                  "snow   alive=%u/%u  rate=%.0f/s  size=%.3f..%.3f  streak=%.0f",
                  gSnowParticles.activeCount(), gSnowParticles.capacity(), gSnowRate, SNOW_SIZE_MIN,
                  SNOW_SIZE_MAX, SNOW_STREAK);
    gInfoText.OutText(line, 0, x, y, 0, 0, 0xFFFFFFFF, GNg::eBLEND_NORMAL);
    y += lineHeight;

    std::snprintf(line, sizeof(line),
                  "emitter dir(0, %.0f, 0)  gravity y rain=%.2f snow=%.2f  plane xz=+-%.0f  "
                  "column=%.0f m (world-fixed)",
                  PRECIP_DIR_Y, RAIN_GRAVITY_Y, SNOW_GRAVITY_Y, PRECIP_SPAWN_HALF_XZ,
                  PRECIP_COLUMN_HEIGHT);
    gInfoText.OutText(line, 0, x, y, 0, 0, 0xFFFFFFFF, GNg::eBLEND_NORMAL);
    y += lineHeight;

    std::snprintf(line, sizeof(line), "recycle center(%.1f %.1f %.1f)  half(%.1f %.1f %.1f)",
                  gPrecipBoxCenter.x, gPrecipBoxCenter.y, gPrecipBoxCenter.z, gPrecipBoxHalf.x,
                  gPrecipBoxHalf.y, gPrecipBoxHalf.z);
    gInfoText.OutText(line, 0, x, y, 0, 0, 0xFFFFFFFF, GNg::eBLEND_NORMAL);
    y += lineHeight;

    if (gRainSampleValid) {
        std::snprintf(line, sizeof(line), "sample pos(%.2f %.2f %.2f)  vel(%.2f %.2f %.2f)",
                      gRainSamplePos.x, gRainSamplePos.y, gRainSamplePos.z, gRainSampleVel.x,
                      gRainSampleVel.y, gRainSampleVel.z);
    } else {
        std::snprintf(line, sizeof(line), "sample (no alive rain particle)");
    }
    gInfoText.OutText(line, 0, x, y, 0, 0, 0xFFAAFFAA, GNg::eBLEND_NORMAL);
    y += lineHeight;

    // 把样本粒子投影到屏幕：直接给出它每秒在屏幕上往哪走（dy > 0 = 屏幕向下）。
    if (gRainSampleValid && gLastViewProjValid) {
        const float dtLook = 0.1f;
        const GNg::sVEC3 nextPos = {gRainSamplePos.x + gRainSampleVel.x * dtLook,
                                    gRainSamplePos.y + gRainSampleVel.y * dtLook,
                                    gRainSamplePos.z + gRainSampleVel.z * dtLook};
        float sx = 0.0f;
        float sy = 0.0f;
        float nx = 0.0f;
        float ny = 0.0f;
        if (projectPointToScreen(gLastViewProj, gRainSamplePos, &sx, &sy) &&
            projectPointToScreen(gLastViewProj, nextPos, &nx, &ny)) {
            const bool inView = sx >= 0.0f && sx <= static_cast<float>(gFbWidth) && sy >= 0.0f &&
                                sy <= static_cast<float>(gFbHeight);
            std::snprintf(line, sizeof(line),
                          "sample screen(%.0f, %.0f) fb(%d x %d) %s  dy=%+.1f px/0.1s (>0 = 向下)",
                          sx, sy, gFbWidth, gFbHeight, inView ? "in-view" : "off-screen", ny - sy);
        } else {
            std::snprintf(line, sizeof(line), "sample screen (behind camera)");
        }
    } else {
        std::snprintf(line, sizeof(line), "sample screen (n/a)");
    }
    gInfoText.OutText(line, 0, x, y, 0, 0, 0xFFFFFF88, GNg::eBLEND_NORMAL);
    y += lineHeight;

    if (gRainNearestDist >= 0.0f) {
        std::snprintf(line, sizeof(line), "nearest rain %.2f m at (%.2f %.2f %.2f)  boxes=%s x%u",
                      gRainNearestDist, gRainNearestPos.x, gRainNearestPos.y, gRainNearestPos.z,
                      gParticleBoxes ? "on" : "off", PARTICLE_DEBUG_BOX_COUNT);
    } else {
        std::snprintf(line, sizeof(line), "nearest rain (none alive)");
    }
    gInfoText.OutText(line, 0, x, y, 0, 0, 0xFFFFAA55, GNg::eBLEND_NORMAL);
    y += lineHeight;

    if (gSnowSampleValid) {
        std::snprintf(line, sizeof(line), "snow sample pos(%.2f %.2f %.2f)  vel(%.2f %.2f %.2f)",
                      gSnowSamplePos.x, gSnowSamplePos.y, gSnowSamplePos.z, gSnowSampleVel.x,
                      gSnowSampleVel.y, gSnowSampleVel.z);
    } else {
        std::snprintf(line, sizeof(line), "snow sample (none alive)");
    }
    gInfoText.OutText(line, 0, x, y, 0, 0, 0xFFCCDDFF, GNg::eBLEND_NORMAL);
}

void drawGizmos() {
    if (!gShowGizmos) {
        return;
    }
    drawAxes();
    drawModelAabb();
}

std::string lowerExtension(const std::string &path) {
    const size_t dot = path.find_last_of('.');
    if (dot == std::string::npos) {
        return std::string();
    }
    std::string ext = path.substr(dot);
    for (size_t i = 0; i < ext.size(); ++i) {
        ext[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(ext[i])));
    }
    return ext;
}

bool endsWithNoCase(const std::string &text, const char *suffix) {
    const size_t length = std::strlen(suffix);
    if (text.size() < length) {
        return false;
    }
    for (size_t i = 0; i < length; ++i) {
        const char a = static_cast<char>(std::tolower(static_cast<unsigned char>(text[text.size() - length + i])));
        const char b = static_cast<char>(std::tolower(static_cast<unsigned char>(suffix[i])));
        if (a != b) {
            return false;
        }
    }
    return true;
}

// 把模型里的相对贴图路径拼到模型所在目录；兼容 Windows 反斜杠与盘符。
std::string resolveTexturePath(const std::string &modelPath, const std::string &texturePath) {
    std::string path = texturePath;
    for (size_t i = 0; i < path.size(); ++i) {
        if (path[i] == '\\') {
            path[i] = '/';
        }
    }
    if (path.size() > 1 && path[1] == ':') {
        path = path.substr(2);
    }
    if (!path.empty() && path[0] == '/') {
        return path;
    }
    const size_t slash = modelPath.find_last_of('/');
    if (slash == std::string::npos) {
        return path;
    }
    return modelPath.substr(0, slash + 1) + path;
}

GNg::sIMAGE *loadTexture(const std::string &path) {
    const std::string ext = lowerExtension(path);
    if (ext == ".png") {
        std::vector<uint8_t> bytes;
        std::string error;
        if (!mmd::readFileBytes(path, bytes, &error)) {
            std::fprintf(stderr, "[mmdViewer] 贴图读取失败: %s (%s)\n", path.c_str(), error.c_str());
            return nullptr;
        }
        GNg::sIMAGE *image = new GNg::sIMAGE();
        if (!GNg::ImageLoadFromMem(bytes.data(), static_cast<int>(bytes.size()), image)) {
            std::fprintf(stderr, "[mmdViewer] PNG 解码失败: %s\n", path.c_str());
            delete image;
            return nullptr;
        }
        return image;
    }
    if (ext == ".tga" || ext == ".bmp") {
        std::vector<uint8_t> rgba;
        int width = 0;
        int height = 0;
        std::string error;
        if (!mmd::decodeTgaOrBmp(path, rgba, width, height, &error)) {
            std::fprintf(stderr, "[mmdViewer] 贴图解码失败: %s (%s)\n", path.c_str(), error.c_str());
            return nullptr;
        }
        GNg::sIMAGE *image = new GNg::sIMAGE();
        image->DataFill(rgba.data(), GNg::CR_RGBA, width, height);
        if (image->vkImage == VK_NULL_HANDLE) {
            std::fprintf(stderr, "[mmdViewer] 贴图上传失败: %s\n", path.c_str());
            delete image;
            return nullptr;
        }
        return image;
    }
    std::fprintf(stderr, "[mmdViewer] 跳过不支持的贴图格式: %s\n", path.c_str());
    return nullptr;
}

bool loadModel() {
    const char *fromEnv = std::getenv("MMD_VIEWER_MODEL");
    gModelPath = (fromEnv && *fromEnv) ? fromEnv : "tests/うさうさ/うさうさ.pmx";
    std::printf("[mmdViewer] 加载模型: %s\n", gModelPath.c_str());

    std::string error;
    std::vector<mmd::MeshTextureBinding> bindings;
    bool ok = false;
    if (endsWithNoCase(gModelPath, ".pmd")) {
        mmd::PmdModel model;
        ok = mmd::loadPmdFile(gModelPath, model, &error) && mmd::buildPmdModel(model, gModel, bindings, &error);
    } else {
        mmd::PmxModel model;
        ok = mmd::loadPmxFile(gModelPath, model, &error) && mmd::buildPmxModel(model, gModel, bindings, &error);
    }
    if (!ok) {
        std::fprintf(stderr, "[mmdViewer] 模型加载失败: %s\n", error.c_str());
        return false;
    }

    gModel.computeWorldAABB();
    if (gModel.aWorldAABB.IsValid()) {
        const GNg::sVEC3 center = gModel.aWorldAABB.Center();
        gTarget = center;
        gHeight = gModel.aWorldAABB.aMax.y - gModel.aWorldAABB.aMin.y;
    }
    if (gHeight < 0.01f) {
        gHeight = 1.0f;
    }
    resetCamera();

    std::map<std::string, GNg::sIMAGE *> cache;
    for (size_t i = 0; i < bindings.size(); ++i) {
        const std::string fullPath = resolveTexturePath(gModelPath, bindings[i].texturePath);
        GNg::sIMAGE *image = nullptr;
        std::map<std::string, GNg::sIMAGE *>::iterator found = cache.find(fullPath);
        if (found != cache.end()) {
            image = found->second;
        } else {
            image = loadTexture(fullPath);
            cache[fullPath] = image;
            if (image) {
                gTextures.push_back(image);
            }
        }
        if (image && bindings[i].meshIndex < gModel.aMeshCount) {
            gModel.aMeshes[bindings[i].meshIndex].aAlbedoTex = image;
        }
    }

    gLight = new GN3d::cLIGHT();
    GNg::sVEC3 ambient = {0.35f, 0.35f, 0.38f};
    gLight->SetAmbient(ambient, 1.0f);
    GNg::sVEC3 sunDir = {-0.35f, -0.85f, 0.4f};
    GNg::sVEC3 sunColor = {1.0f, 0.97f, 0.92f};
    gLight->AddDirectional(sunDir, sunColor, 1.2f, false);
    GNg::sVEC3 fillPos = {gTarget.x + gHeight, gTarget.y + gHeight, gTarget.z + gHeight * 2.0f};
    GNg::sVEC3 fillColor = {0.45f, 0.5f, 0.6f};
    gLight->AddPoint(fillPos, gHeight * 8.0f, fillColor, 0.6f);

    uint32_t triangleCount = 0;
    for (uint32_t i = 0; i < gModel.aMeshCount; ++i) {
        triangleCount += gModel.aMeshes[i].aIndexCount / 3;
    }
    std::printf("[mmdViewer] 网格 %u 个，三角形 %u 个，贴图 %zu 张，模型高度 %.2f\n",
                gModel.aMeshCount, triangleCount, gTextures.size(), gHeight);
    std::printf("[mmdViewer] 模型世界 AABB: y=[%.3f, %.3f]，地面 y=%.3f\n",
                gModel.aWorldAABB.aMin.y, gModel.aWorldAABB.aMax.y, groundHeight());
    std::printf("[mmdViewer] 操作: 左/右键拖动转视角, WASD 平移, Q/E 升降, Shift 加速, "
                "方向键/J L 转视角, R 重置, G 坐标轴/包围盒, H 坐标信息, O 降水, P 贴花, "
                "V 粒子盒, C 云层\n");
    std::printf("[mmdViewer] 天气: 1 晴 2 阴 3 雨 4 雪 5 雾 6 暴风雨（3 秒过渡）\n");
    // 诊断用启动天气：MMD_VIEWER_WEATHER=snow|rain|fog|storm|cloudy|clear。
    // 便于脚本化复现（无需手按 1..6），也便于截图核对降水是否真的画出来。
    GN3d::WeatherPreset startPreset = GN3d::WeatherPreset::CLEAR;
    if (const char *weatherEnv = std::getenv("MMD_VIEWER_WEATHER")) {
        const std::string weatherName(weatherEnv);
        if (weatherName == "snow") {
            startPreset = GN3d::WeatherPreset::SNOW;
        } else if (weatherName == "rain") {
            startPreset = GN3d::WeatherPreset::RAIN;
        } else if (weatherName == "fog") {
            startPreset = GN3d::WeatherPreset::FOG;
        } else if (weatherName == "storm") {
            startPreset = GN3d::WeatherPreset::STORM;
        } else if (weatherName == "cloudy") {
            startPreset = GN3d::WeatherPreset::CLOUDY;
        }
    }
    gWeather.reset(startPreset);
    std::printf("[mmdViewer] 起始天气预设=%d（MMD_VIEWER_WEATHER 可覆盖）\n",
                static_cast<int>(startPreset));
    applyWeather();
    if (!createPrecipitationSystems()) {
        std::fprintf(stderr, "[mmdViewer] 降水粒子系统创建失败：雨/雪天气将无降水\n");
    }
    createDecals();
    // 坐标叠层用的文字输出器（与旧示例 samples/04_font.cpp 的用法一致）：
    // Init(nullptr) 建立默认结构，SetFreeTypeFont 换成系统 FreeType 字体。
    gInfoText.Init(nullptr);
    gInfoText.SetFreeTypeFont(16);
    return true;
}

} // namespace

extern "C" {

GN_CALL void AppInit() {
    gLoaded = loadModel();
}

GN_CALL void AppUnInit() {
    for (size_t i = 0; i < gTextures.size(); ++i) {
        delete gTextures[i];
    }
    gTextures.clear();
    gModel.Release();
    gInfoText.Release();
    delete gLight;
    gLight = nullptr;
}

GN_CALL void AppDisplay2D(unsigned int nTime) {
    (void)nTime;
    if (!gLoaded || !gLight) {
        return;
    }

    const GNg::sVEC3 camera = cameraPosition();
    GNg::sVEC3 right;
    GNg::sVEC3 camUp;
    cameraBasis(camera, right, camUp);
    GNg::sVEC3 up = {0.0f, 1.0f, 0.0f};

    float aspect = 1280.0f / 800.0f;
    if (::GT_MainWindow) {
        int width = 0;
        int height = 0;
        glfwGetFramebufferSize(::GT_MainWindow, &width, &height);
        if (width > 0 && height > 0) {
            aspect = static_cast<float>(width) / static_cast<float>(height);
            gFbWidth = width;
            gFbHeight = height;
        }
    }

    GNg::sMAT44 view;
    GNg::sMAT44 projection;
    GNg::sMAT44 viewProjection;
    const GNg::sVEC3 lookAt = cameraTarget();
    GNg::Mat44LookAtRH(&view, &camera, &lookAt, &up);
    const float nearZ = clampValue(gHeight * 0.01f, 0.05f, 1.0f);
    // 远平面至少要容下 300 m 的中景云层（P11-S2b），否则小模型时云层被整片裁掉。
    float farZ = cameraDistance() * 8.0f + gHeight * 4.0f;
    const float cloudFarZ = CLOUD_LAYER_HEIGHT + 2.0f * CLOUD_TILE_SIZE;
    if(farZ < cloudFarZ) {
        farZ = cloudFarZ;
    }
    GNg::Mat44PerspectiveVK(&projection, 0.785398f, aspect, nearZ, farZ);
    GNg::Mat44Mul(&viewProjection, &projection, &view);
    gLastViewProj = viewProjection;
    gLastViewProjValid = true;

    GNg::GN_VkBegin3DPass(&viewProjection, &camera, gLight->GetUBO(), sizeof(GN3d::sLIGHT_UBO));
    GNg::GN_VkDrawSkybox(&viewProjection);
    // drawClouds / drawDecals / drawPrecipitation 都收 `const sMAT44&`：
    // 传 &viewProjection 会被 sMAT44(bool) 隐式接住变成单位矩阵（已把该构造函数改为 explicit）。
    if (gCloudLayerEnabled) {
        drawClouds(viewProjection, camera); // 天空盒之后、模型之前；C 键开关，默认关
    }
    drawGround(); // 不透明地面：先画，供模型与贴花做深度/投影基准
    gModel.Draw();
    drawDecals(viewProjection); // 贴花贴在地面上（深度测试开、写关）
    drawPrecipitation(viewProjection, right, camUp);
    drawGizmos(); // 坐标轴 + 模型世界 AABB（G 键切换）
    drawParticleDebugBoxes(right, camUp); // 最近若干粒子的四边形 AABB（V 键切换）
    GNg::GN_VkEnd3DPass();
    GNg::GN_VkBegin2DPass(); // 重开 2D 叠层通道（loadOp=LOAD，保留 3D 画面）
    drawCoordOverlay(); // 左上角坐标信息叠层（H 键切换）
}

GN_CALL void AppDisplay3D(unsigned int nTime) {
    (void)nTime;
}

GN_CALL void AppRun(unsigned int nTime) {
    static unsigned int lastTime = 0;
    gDelta = static_cast<float>(nTime - lastTime) / 1000.0f;
    lastTime = nTime;
    if (gDelta <= 0.0f || gDelta > 0.5f) {
        gDelta = 0.016f;
    }
    if (!gLoaded) {
        return;
    }

    if (GN::Key1[GK_1]) {
        gWeather.setPreset(GN3d::WeatherPreset::CLEAR, 3.0f);
    }
    if (GN::Key1[GK_2]) {
        gWeather.setPreset(GN3d::WeatherPreset::CLOUDY, 3.0f);
    }
    if (GN::Key1[GK_3]) {
        gWeather.setPreset(GN3d::WeatherPreset::RAIN, 3.0f);
    }
    if (GN::Key1[GK_4]) {
        gWeather.setPreset(GN3d::WeatherPreset::SNOW, 3.0f);
    }
    if (GN::Key1[GK_5]) {
        gWeather.setPreset(GN3d::WeatherPreset::FOG, 3.0f);
    }
    if (GN::Key1[GK_6]) {
        gWeather.setPreset(GN3d::WeatherPreset::STORM, 3.0f);
    }
    gWeather.update(gDelta);
    applyWeather();

    if (GN::Key1[GK_G]) { // 切换坐标轴 + 包围盒显示
        gShowGizmos = !gShowGizmos;
    }
    if (GN::Key1[GK_H]) { // 切换左上角坐标信息叠层
        gShowCoordInfo = !gShowCoordInfo;
    }
    if (GN::Key1[GK_O]) { // 单独开关降水粒子（排查用；避开 J/L 的转视角键）
        gPrecipEnabled = !gPrecipEnabled;
    }
    if (GN::Key1[GK_P]) { // 单独开关地面溅射贴花（排查用）
        gDecalEnabled = !gDecalEnabled;
    }
    if (GN::Key1[GK_C]) { // 开关 S2b 中景 billboard 云层（默认关，见 gCloudLayerEnabled 注释）
        gCloudLayerEnabled = !gCloudLayerEnabled;
    }
    if (GN::Key1[GK_V]) { // 开关粒子四边形包围盒（只画最近 16 个）
        gParticleBoxes = !gParticleBoxes;
    }
    if (GN::Key1[GK_R]) { // 回到默认正面视角
        resetCamera();
    }

    // 自由飞行平移：W/S 沿视线前后、A/D 沿视线右向、Q/E 垂直升降、Shift 加速。
    {
        GNg::sVEC3 forward;
        GNg::sVEC3 rightNow;
        GNg::sVEC3 upNow;
        cameraBasisVectors(forward, rightNow, upNow);
        (void)upNow;
        const float speed = (gHeight > 1.0f ? gHeight : 1.0f) * 0.9f * gDelta *
                            (GN::Key[GK_SHIFT] ? 3.0f : 1.0f);
        if (GN::Key[GK_W]) {
            gCameraPosition.x += forward.x * speed;
            gCameraPosition.y += forward.y * speed;
            gCameraPosition.z += forward.z * speed;
        }
        if (GN::Key[GK_S]) {
            gCameraPosition.x -= forward.x * speed;
            gCameraPosition.y -= forward.y * speed;
            gCameraPosition.z -= forward.z * speed;
        }
        if (GN::Key[GK_A]) {
            gCameraPosition.x -= rightNow.x * speed;
            gCameraPosition.y -= rightNow.y * speed;
            gCameraPosition.z -= rightNow.z * speed;
        }
        if (GN::Key[GK_D]) {
            gCameraPosition.x += rightNow.x * speed;
            gCameraPosition.y += rightNow.y * speed;
            gCameraPosition.z += rightNow.z * speed;
        }
        if (GN::Key[GK_Q]) {
            gCameraPosition.y -= speed;
        }
        if (GN::Key[GK_E]) {
            gCameraPosition.y += speed;
        }
    }

    // 键盘转视角：方向键 / J L（鼠标不可用时始终可用的一条路径）。
    {
        const float lookStep = KEY_LOOK_SPEED * gDelta;
        float lookYaw = 0.0f;
        float lookPitch = 0.0f;
        if (GN::Key[GK_LEFT] || GN::Key[GK_J]) {
            lookYaw -= lookStep;
        }
        if (GN::Key[GK_RIGHT] || GN::Key[GK_L]) {
            lookYaw += lookStep;
        }
        if (GN::Key[GK_UP]) {
            lookPitch += lookStep;
        }
        if (GN::Key[GK_DOWN]) {
            lookPitch -= lookStep;
        }
        applyLookDelta(lookYaw, lookPitch);
    }

    // 鼠标拖拽转视角（按住左键或右键）。
    updateMouseLook();
    updatePrecipitation(gDelta);
    updateDecals(gDelta);
}

} // extern "C"

GN_MAIN;
GN_InitLoadCallBackFunc(AppInit, AppUnInit);
GN_InitLoopCallBackFunc(AppDisplay2D, AppDisplay3D, AppRun);
GN_StartRun("MMD Viewer (PMD/PMX)", 1280, 800, 0, false, true, true, 0, 0, 0, 0);
GN_EXIT;
