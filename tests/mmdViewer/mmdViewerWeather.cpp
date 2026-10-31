// tests/mmdViewer/mmdViewerWeather.cpp — MMD 查看器：天气 / 云 / 雾 / 降水接线
//
// 职责：把 WeatherSystem 的输出推进天空与雾参数、绘制中景云层、
//       创建并每帧推进固定在世界空间的雨/雪粒子柱、提交降水绘制。
//       共享数值常量的唯一定义也在本文件（被叠层与主循环引用）。

#include "mmdViewerShared.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

// ── 共享数值常量（唯一定义；声明见 mmdViewerShared.h）────────────
// ── P11-S6：云层 / 雾 / 降水参数（查看器侧表现，不改天气状态机）────
extern const float CLOUD_LAYER_HEIGHT = 300.0f;
extern const float CLOUD_TILE_SIZE = 256.0f;
// 降水外观参数：发射器与叠层显示共用同一组常量，避免两处写死不一致。
// 雨丝：宽度减半（0.05–0.09 m），拉长倍数补到 20 ⇒ 长度仍是 1.0–1.8 m。
// 宽度按 30 m 处 ~1.7 px 反推；太宽会变成"白色长条"。若仍偏粗/偏细，只调这两组常量。
extern const float RAIN_SIZE_MIN = 0.05f;
extern const float RAIN_SIZE_MAX = 0.09f;
extern const float SNOW_SIZE_MIN = 0.15f;
extern const float SNOW_SIZE_MAX = 0.25f;
extern const float RAIN_STREAK = 20.0f;
extern const float SNOW_STREAK = 1.0f;
extern const float PRECIP_DIR_Y = -1.0f; // 世界 Y 方向：-1 = 向下落
extern const float RAIN_GRAVITY_Y = -9.81f;
extern const float SNOW_GRAVITY_Y = -2.5f;
// 降水柱：固定在世界空间、位于角色正上方，与相机完全无关。
// 发射器是一片水平面（BOX 的 y 半范围为 0），粒子从顶部平面生成、向下落到地面，
// 越过地面后被回收盒折回柱顶 —— 方向永远是"天上 → 地面"。
// 柱高取 64 m：远看时柱顶（发射面，地面 +64 m）与柱底（地面）都落在常见相机距离与雾的
// 可见范围之外，上下两条硬边界就看不出来（24 m 时近处能明显看到一条悬空的"雪带"）。
// 代价是同样粒子数摊在更长的柱里、密度下降；粒子池容量 / 发射率的真机调参另行处理。
// 雨/雪寿命 = 单趟落地时间 + 少量余量：落地 wrap 回柱顶后才寿终，
// 寿终点在柱顶附近（不在可见高度消失）。
extern const float PRECIP_SPAWN_HALF_XZ = 30.0f;
extern const float PRECIP_COLUMN_HEIGHT = 64.0f;

namespace {

const uint32_t CLOUD_TILES_PER_AXIS = 8;
const float CLOUD_NEAR_FADE = 50.0f;
const float FOG_MAX_OPACITY = 0.9f;
// 粒子池 = 屏幕密度的实际上限（稳态下池会被填满）。原先 61440/81920 太密，缩小 3~4 倍。
// 每颗在 CPU 端展开 6 个顶点（36 B/顶点）后整块上传，池子越小上传与填充压力越低。
const uint32_t RAIN_CAPACITY = 16384;
const uint32_t SNOW_CAPACITY = 24576;
const float PRECIP_MIN_INTENSITY = 0.05f; // 低于此强度不提交该类降水

} // namespace

GN3d::WeatherSystem gWeather;
GN3d::cPARTICLE_SYSTEM gRainParticles;
GN3d::cPARTICLE_SYSTEM gSnowParticles;
bool gPrecipReady = false;
// 分系统开关（排查用）：J 关/开降水粒子，K 关/开地面溅射贴花。
bool gPrecipEnabled = true;
// S2b 中景 billboard 云层：默认关闭。它此前因为拿到单位矩阵而从未真正显示过（等于未验证），
// 矩阵修对后暴露为大片 256 m 方形贴片、接缝明显（糊在脸上），先用 C 键单独开关、默认关，
// 天空盒（S2a）仍提供程序化云。要正式启用需要单独做贴片尺度/接缝/透明度的调参。
bool gCloudLayerEnabled = false;
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

namespace {
// G3 重烘状态：上次烘焙使用的天气目标状态 + pending 标志。
GN3d::WeatherState s_lastBakedTarget = {};
bool s_hasBakedTarget = false;
bool s_rebakePending = true; // 启动首帧先烘一次
} // namespace

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

    // ── G3：环境与解析天空同源（按需重烘）────────────────────────────
    // 策略：只在「天气目标状态变化 + 过渡结束」时重烘一次，绝不每帧；
    // 过渡中途连续切换只会在最终目标稳定后烘一次；启动首帧烘一次（初始环境由
    // GN_VkInit3D 用默认天空烘过，这里换成实际初始天气）。天空 timeSeconds 连续变化
    // 不触发重烘（动态 IBL 已登记为后续项）；失败清 pending、保留旧环境，下次切换再试。
    {
        const GN3d::WeatherState &target = gWeather.target();
        if (!s_hasBakedTarget ||
            std::memcmp(&target, &s_lastBakedTarget, sizeof(GN3d::WeatherState)) != 0) {
            s_lastBakedTarget = target;
            s_hasBakedTarget = true;
            s_rebakePending = true;
        }
        if (s_rebakePending && !gWeather.isTransitioning()) {
            // applyWeather 由 AppRun 在 3D pass 之前调用；GN_VkRebakeEnvironment 内部再兜底检查。
            if (!GNg::GN_Vk.initialized) {
                // 引擎还没初始化：保留 pending，等初始化后的帧再试（不会每帧真正重烘）。
            } else if (GNg::GN_VkRebakeEnvironment()) {
                s_rebakePending = false;
            } else {
                s_rebakePending = false;
                std::fprintf(stderr, "[mmdViewer] 环境重烘失败，保留旧环境\n");
            }
        }
    }
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
    // 寿命 = 单趟「柱顶→地面」落地时间 + 少量余量，让寿终发生在落地 wrap 回柱顶之后，
    // 避免在可见高度半空消失（回收只 wrap 位置、不重置 age：GN_3Dparticle.cpp）。
    // 落地时间（初速向下 18–22 m/s、重力 9.81 m/s²）：64 = v·t + 9.81·t²/2
    //   v=18 ⇒ t = (-18 + √(18² + 2·9.81·64)) / 9.81 ≈ 2.22 s
    //   v=22 ⇒ t = (-22 + √(22² + 2·9.81·64)) / 9.81 ≈ 2.01 s
    // 取 2.3–2.6 s（余量 0.08–0.59 s）：寿终落在第二趟开头，离柱顶约 1.5–15 m
    // （离地约 49–63 m，柱顶 64 m 附近），不在可见高度消失。容量/发射率真机再调。
    rain.aLifeMin = 2.3f;
    rain.aLifeMax = 2.6f;
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
    // 雪终端速度 ≈3.75 m/s（见下方风响应 0.4 的注释）：64 m / 3.75 m/s ≈ 17.07 s 落地；
    // 取 17.5–18.0 s（余量 0.43–0.93 s），寿终同样发生在 wrap 回柱顶之后，
    // 离柱顶约 1.6–3.5 m（离地约 60–62 m），不在可见高度消失。容量/发射率真机再调。
    snow.aLifeMin = 17.5f;
    snow.aLifeMax = 18.0f;
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

namespace {

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

} // namespace

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

// 每帧：按天气强度更新发射率/风/回收盒，再推进粒子模拟。
// 发射点与回收盒都在世界空间里钉死（围着角色），不随相机移动。
void updatePrecipitation(float dt) {
    if (!gPrecipReady) {
        return;
    }
    const GN3d::WeatherOutput out = gWeather.output();
    // 固定降水柱：顶部发射平面在角色头顶上方，回收盒是同一根柱子（底=地面，顶=发射平面）。
    // 柱高只由 PRECIP_COLUMN_HEIGHT 推导：发射平面 = 地面 + 柱高，柱心 = 地面 + 柱高/2，
    // 柱半高 = 柱高/2；setRecycleBox 与叠层快照 gPrecipBoxCenter/gPrecipBoxHalf 都用这组值。
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
