// tests/mmdViewer/mmdViewerWeather.cpp — MMD 查看器：天气 / 云 / 雾 / 降水接线
//
// 职责：把 WeatherSystem 的输出推进天空与雾参数、绘制中景云层、
//       创建并每帧推进固定在世界空间的雨/雪粒子柱、提交降水绘制。
//       共享数值常量的唯一定义也在本文件（被叠层与主循环引用）。

#include "mmdViewerShared.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

// ── 共享数值常量（唯一定义；声明见 mmdViewerShared.h）────────────
// ── 云 / 雾 / 降水参数（查看器侧表现，不改天气状态机）────
// 降水外观参数：发射器与叠层显示共用同一组常量，避免两处写死不一致。
// 雨丝：宽度 0.05–0.09 m，拉长倍数 20 ⇒ 长度 1.0–1.8 m。
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

// ── L5b：立方体云（世界固定的一组云块；雨雪从各自方块的底面落下）──────────
// 云不是"一整层天空"，而是若干个独立云块（方块 = 云的**包围盒/范围**，画面里不显示方块）：
// 位置/尺寸/是否降雨由固定种子的哈希决定，只随风整体平移（wrap 回分布环），相机移动时有真实视差。
// 关键：分布是环（内半径 250 m）⇒ 不会在相机头顶生成大块（首版 900 m 半径 + 160 m 边长会糊屏）；
// 数量靠**更多实例**来堆（128 实例上限）；雨雪只让最近的若干朵发射（见 updatePrecipitation）。
extern const uint32_t CLOUD_CUBE_MAX = 128; // 渲染实例上限（= GN_Vk 的 128）
const float CLOUD_CUBE_RADIUS_MIN = 200.0f; // 分布环内半径（m）：不允许贴在相机头上
const float CLOUD_CUBE_RADIUS_MAX = 900.0f; // 分布环外半径（m）
const float CLOUD_CUBE_BASE_Y_MIN = 260.0f; // 云块中心高度下限（m）：保证云顶明显高过云底
const float CLOUD_CUBE_BASE_Y_MAX = 360.0f; // 云块中心高度上限（m）
const float CLOUD_CLUSTER_HALF_MIN = 50.0f; // 一朵云（簇）的半径下限（m）
const float CLOUD_CLUSTER_HALF_MAX = 95.0f; // 一朵云（簇）的半径上限（m）
extern const uint32_t CLOUD_CLUSTER_COUNT = 40; // 云"朵"数；每朵 2–3 个盒子（≤128 盒）⇒ 铺满天空靠"多朵"
const float CLOUD_BASE_ALTITUDE = 230.0f; // 全场景云底高度（m）：密度在它以下归零 ⇒ 平底
const uint32_t CLOUD_CUBE_RAIN_CAPACITY = 256; // 每个发射器雨滴池（发射器上限 6 ⇒ 1500 粒子量级）
const uint32_t CLOUD_CUBE_SNOW_CAPACITY = 256; // 每个发射器雪花池
const float CLOUD_CUBE_RAIN_RATE = 260.0f; // 每个发射器满强度时的发射率（/s）
const float CLOUD_CUBE_SNOW_RATE = 40.0f; // 每个发射器满强度时的发射率（/s）
// 同时发射的降水云块上限：粒子总数 = 该值 × 容量，不能随云块数线性涨。
const uint32_t CLOUD_CUBE_RAIN_EMITTERS = 6;
// 远平面下限：云盖盒子的**远侧面**在 ~4.8 km（半边长 2600 的背面），必须容得下，
// 否则云盖整片被近裁剪面切掉。深度精度由 near/z 决定，far 放大不伤精度
// （24 位深度在 30 m 处仍有 ~0.3 mm）。
extern const float CLOUD_FAR_PLANE = 6000.0f;

namespace {

const float FOG_MAX_OPACITY = 0.9f;
const float PRECIP_MIN_INTENSITY = 0.05f; // 低于此强度不提交该类降水

// 云块：世界固定布局（含是否降雨的标志）。
// L5b：一朵云 = 一个**簇**（2–3 个重叠盒子）+ 其中第一个盒子兼作降水发射器。
struct sCLOUD_CELL {
    float aX; // 基准世界 x（漂移在此之上叠加）
    float aZ;
    float aY; // 中心高度
    float aHalf; // 半边长
    float aWeight; // 0..1：原始哈希权重（同簇共享 ⇒ 整簇一起显隐）
    float aRank; // 0..1：按权重排的秩（保证覆盖率 c 恰好显示约 c*N 个盒子）
    float aTint; // 0.94..1.0：每簇的亮度微差
    float aEmitHalf; // >0 = 该盒是该簇的降水发射器，用整个簇的半径
    bool aRains; // 该簇是否降雨（固定哈希决定）
};

sCLOUD_CELL s_cloudCells[CLOUD_CUBE_MAX];
uint32_t s_cloudCellCount = 0; // 实际盒子数（簇展开后，≤ CLOUD_CUBE_MAX）
bool s_cloudCellsReady = false;
uint32_t s_cellRng = 20261101u;

// 固定种子线性同余：布局与种子有关、与帧号无关 ⇒ 每次运行云的位置一致。
float nextCellRandom() {
    s_cellRng = s_cellRng * 1664525u + 1013904223u;
    return (float)(s_cellRng >> 8) / 16777216.0f;
}

// 一次性生成云块布局（幂等）。
// 每个簇：环带取一个中心 + 2–3 个成员盒子（0.62–1.0 倍簇半径，彼此重叠）。
// 密度场只依赖世界坐标（见 gn3d_cloud_cube.frag），所以同一簇的成员叠起来是**一朵**云。
void buildCloudCells() {
    if (s_cloudCellsReady) {
        return;
    }
    uint32_t cell = 0;
    for (uint32_t cluster = 0; cluster < CLOUD_CLUSTER_COUNT; ++cluster) {
        if (cell + 3u > CLOUD_CUBE_MAX) {
            break;
        }
        const float angle = nextCellRandom() * 6.28318530718f;
        // 在环带 [Rmin, Rmax] 上按面积均匀取点：r² 均匀 ⇒ 不会挤在内圈。
        const float radiusSq = CLOUD_CUBE_RADIUS_MIN * CLOUD_CUBE_RADIUS_MIN +
                               (CLOUD_CUBE_RADIUS_MAX * CLOUD_CUBE_RADIUS_MAX -
                                CLOUD_CUBE_RADIUS_MIN * CLOUD_CUBE_RADIUS_MIN) *
                                   nextCellRandom();
        const float radius = std::sqrt(radiusSq);
        const float clusterX = std::cos(angle) * radius;
        const float clusterZ = std::sin(angle) * radius;
        const float clusterHalf =
            CLOUD_CLUSTER_HALF_MIN + (CLOUD_CLUSTER_HALF_MAX - CLOUD_CLUSTER_HALF_MIN) *
                                         nextCellRandom();
        const float clusterY = CLOUD_CUBE_BASE_Y_MIN +
                               (CLOUD_CUBE_BASE_Y_MAX - CLOUD_CUBE_BASE_Y_MIN) * nextCellRandom();
        const float weight = nextCellRandom();
        const float tint = 0.94f + 0.06f * nextCellRandom();
        const bool rains = nextCellRandom() < 0.6f;
        const uint32_t members = 2u + (nextCellRandom() < 0.5f ? 1u : 0u);
        const uint32_t firstMember = cell;
        for (uint32_t m = 0; m < members; ++m) {
            sCLOUD_CELL &member = s_cloudCells[cell];
            member.aX = clusterX + (nextCellRandom() - 0.5f) * clusterHalf * 0.9f;
            member.aZ = clusterZ + (nextCellRandom() - 0.5f) * clusterHalf * 0.9f;
            member.aY = clusterY + (nextCellRandom() - 0.5f) * 12.0f; // 同一簇云底基本齐平
            member.aHalf = clusterHalf * (0.62f + 0.38f * nextCellRandom());
            member.aWeight = weight;
            member.aTint = tint;
            member.aEmitHalf = 0.0f;
            member.aRains = false;
            ++cell;
        }
        // 发射器 = 簇的第一个盒子，footprint 用整簇半径（雨从整朵云底下落）。
        s_cloudCells[firstMember].aEmitHalf = clusterHalf;
        s_cloudCells[firstMember].aRains = rains;
    }
    s_cloudCellCount = cell;
    // 秩：保证"覆盖率 = 0.15 也能看见 1–2 朵、覆盖率 = 1 时全见"。
    for (uint32_t i = 0; i < s_cloudCellCount; ++i) {
        uint32_t below = 0;
        for (uint32_t j = 0; j < s_cloudCellCount; ++j) {
            if (s_cloudCells[j].aWeight < s_cloudCells[i].aWeight) {
                ++below;
            }
        }
        s_cloudCells[i].aRank = ((float)below + 0.5f) / (float)s_cloudCellCount;
    }
    s_cloudCellsReady = true;
}

// 漂移：整组云按风速平移，超出分布范围后 wrap 回来（保持"世界固定"的外观）。
float wrapCoordinate(float value, float extent) {
    const float span = extent * 2.0f;
    float wrapped = std::fmod(value + extent, span);
    if (wrapped < 0.0f) {
        wrapped += span;
    }
    return wrapped - extent;
}

// 云块当前世界 x（含风漂移）。
float cloudCellWorldX(uint32_t index, float windX, float timeSeconds) {
    const sCLOUD_CELL &cell = s_cloudCells[index];
    return wrapCoordinate(cell.aX + windX * timeSeconds * 0.25f, CLOUD_CUBE_RADIUS_MAX);
}

// 云块当前世界 z（含风漂移）。
float cloudCellWorldZ(uint32_t index, float windZ, float timeSeconds) {
    const sCLOUD_CELL &cell = s_cloudCells[index];
    return wrapCoordinate(cell.aZ + windZ * timeSeconds * 0.25f, CLOUD_CUBE_RADIUS_MAX);
}

// 该云块此刻是否显示：秩阈值由天气覆盖率驱动（覆盖率越大云越多，且至少 1 块）。
bool cloudCellVisible(uint32_t index, float coverage) {
    return s_cloudCells[index].aRank < coverage;
}

// 覆盖率越高云块越大：雨天从"几朵"变成"满天"；0.85× .. 1.30×。
// 覆盖率越高云块越大：晴天 0.85×，满覆盖 ~1.9×。
// 必须放大到这个量级：10 朵离散椭球要**互相重叠**才能连成一片云盖（否则满天永远有空隙）。
float cloudCellSizeScale(float coverage) {
    return 0.85f + 1.05f * coverage;
}

// 天气驱动的阴雨程度：雨/雪最重，雾次之，云量给底子；云体压暗与密度增强都用它。
float weatherOvercast(const GN3d::WeatherOutput &out) {
    const float raw = std::fmax(out.precipitation.rain, out.precipitation.snow) * 0.85f +
                      gWeather.state().fog * 0.35f + out.sky.cloudCoverage * 0.35f;
    return std::fmin(1.0f, std::fmax(0.0f, raw));
}

} // namespace

GN3d::WeatherSystem gWeather;
// L5b：每块云一个发射器（雨/雪各一套，按下标与 s_cloudCells 对齐）。
GN3d::cPARTICLE_SYSTEM gRainCells[CLOUD_CUBE_MAX];
GN3d::cPARTICLE_SYSTEM gSnowCells[CLOUD_CUBE_MAX];
// 叠层显示用的聚合计数（每帧在 updatePrecipitation 里重算）。
uint32_t gRainAlive = 0;
uint32_t gRainCapacity = 0;
uint32_t gSnowAlive = 0;
uint32_t gSnowCapacity = 0;
bool gPrecipReady = false;
// 分系统开关（排查用）：J 关/开降水粒子，K 关/开地面溅射贴花。
bool gPrecipEnabled = true;
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
// L4：空气透视开关（B 键切换；默认开 = 走大气散射路径，关 = 旧指数雾 fallback）。
// 定义只在这里：mmdViewer.cpp 与 mmdViewerOverlay.cpp 各自在自己的 TU 里 extern 它
// （共享头 mmdViewerShared.h 属其他写域，只读）。
bool gAtmosphereEnabled = true;
// L5b：云开关（X 键切换；默认**开**）。云由立方体云块绘制。
bool gCloudsEnabled = true;
// 叠层快照：最近一帧提交的云块数与天气覆盖率。
uint32_t gCloudCubeCount = 0;
float gCloudCubeCoverage = 0.0f;
// 叠层显示用的大气快照（applyWeather 每帧写入；太阳方向指向太阳、与天空同源）。
GNg::sVEC3 gAtmosphereSunDir = {0.0f, 1.0f, 0.0f};
float gAtmosphereSunIntensity = 0.0f;
float gAtmosphereGroundRadius = 0.0f;
float gAtmosphereTopRadius = 0.0f;
float gAtmosphereCameraHeight = 0.0f;

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
    // L4/L5 亮度标定：sky-view LUT 辐亮度在 ACES+曝光 1 下偏暗（曾"天空发黑"）；
    // 2.5× 会偏白/过曝，收到 1.8×（与云散射同一线性域，桌面可继续微调这一处）。
    sky.atmosphereIntensity = 1.8f;
    // L5：体积云开启时关掉天空 FBM 云，避免两层云叠加（体积云在 3D pass 内单独 raymarch）。
    if (gCloudsEnabled) {
        sky.cloudCoverage = 0.0f;
        sky.cloudSharpness = 0.0f;
    }
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

    // ── L4：空气透视与天空同源（大气参数由天气映射，太阳方向用天气系统的）──────
    // 参数基座 = 默认地球大气；云量/雾提升 Mie 浑浊度、按云量衰减太阳强度。
    // 注意：sATMOSPHERE_WEATHER_INPUT.fogDensity 是 0..1 的归一化量，不能直接塞
    // weather 的 fog.density（那是 1/m，0.0005..0.03）。
    {
        const GN3d::sATMOSPHERE_PARAMS base = GN3d::makeDefaultAtmosphereParams();
        GN3d::sATMOSPHERE_WEATHER_INPUT weatherInput;
        weatherInput.cloudCoverage = out.sky.cloudCoverage;
        weatherInput.fogDensity = gWeather.state().fog;
        weatherInput.sunIntensityScale = out.sky.sunIntensity;
        const GN3d::sATMOSPHERE_PARAMS atmosphere =
            GN3d::mapWeatherToAtmosphere(base, weatherInput);

        const float sunDirection[3] = {out.sky.sunDirection.x, out.sky.sunDirection.y,
                                       out.sky.sunDirection.z};
        GNg::GN_VkSetAtmosphere(atmosphere, sunDirection);

        // 契约 §3.1（64 B）：aAtmosphere=太阳方向+强度，aAtmosphere2=Rg/Rt/相机高度/开关。
        fog.aAtmosphere[0] = out.sky.sunDirection.x;
        fog.aAtmosphere[1] = out.sky.sunDirection.y;
        fog.aAtmosphere[2] = out.sky.sunDirection.z;
        fog.aAtmosphere[3] = atmosphere.sunIntensity;
        float cameraHeight = cameraPosition().y - groundHeight();
        if (!(cameraHeight > 0.0f)) {
            cameraHeight = 0.0f;
        }
        fog.aAtmosphere2[0] = atmosphere.groundRadius;
        fog.aAtmosphere2[1] = atmosphere.atmosphereRadius;
        fog.aAtmosphere2[2] = cameraHeight;
        fog.aAtmosphere2[3] = gAtmosphereEnabled ? 1.0f : 0.0f;

        // 叠层快照（overlay 在自己的 TU 里 extern 这组变量）
        gAtmosphereSunDir = out.sky.sunDirection;
        gAtmosphereSunIntensity = atmosphere.sunIntensity;
        gAtmosphereGroundRadius = atmosphere.groundRadius;
        gAtmosphereTopRadius = atmosphere.atmosphereRadius;
        gAtmosphereCameraHeight = cameraHeight;
    }
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

// 只有"每簇的发射器盒子"（aEmitHalf > 0）需要粒子系统：实例上限到了 128，
// 若每个盒子都建一套，内存与每帧扫描都会线性膨胀。
bool createPrecipitationSystems() {
    buildCloudCells();
    const float groundY = groundHeight();
    for (uint32_t i = 0; i < s_cloudCellCount; ++i) {
        const sCLOUD_CELL &cell = s_cloudCells[i];
        if (cell.aEmitHalf <= 0.0f) {
            continue;
        }
        const float bottomY = std::fmax(cell.aY - cell.aHalf, CLOUD_BASE_ALTITUDE); // 可见云底
        const float fallHeight = std::fmax(bottomY - groundY, 20.0f);
        const float halfXZ = cell.aEmitHalf * 0.8f;
        // 寿命 = 单趟落地时间 + 余量：落地 wrap 回云底后才寿终（旧版 64 m 柱的同一思路，
        // 这里柱高来自云块自身高度）。雨：初速 18–22 m/s + 重力；雪：终端速度 ≈ 2.5/0.4 m/s。
        const float rainFall = (-20.0f + std::sqrt(400.0f + 2.0f * 9.81f * fallHeight)) / 9.81f;
        const float snowFall = fallHeight / 6.25f;

        GN3d::sPARTICLE_EMITTER_DESC rain;
        rain.aOrigin = {0.0f, bottomY, 0.0f};
        rain.aDirection = {0.0f, PRECIP_DIR_Y, 0.0f};
        rain.aSpread = 0.1f;
        rain.aSpeedMin = 18.0f;
        rain.aSpeedMax = 22.0f;
        rain.aRate = 0.0f;
        rain.aMaxParticles = CLOUD_CUBE_RAIN_CAPACITY;
        rain.aLifeMin = rainFall + 0.5f;
        rain.aLifeMax = rainFall + 0.8f;
        rain.aGravity = {0.0f, RAIN_GRAVITY_Y, 0.0f};
        rain.aDrag = 0.0f;
        rain.aColorStart = GNg::sVEC4(0.82f, 0.90f, 1.0f, 0.85f);
        rain.aColorEnd = GNg::sVEC4(0.82f, 0.90f, 1.0f, 0.35f);
        rain.aBoxHalfExtent = {halfXZ, 0.0f, halfXZ};
        rain.aSizeMin = RAIN_SIZE_MIN;
        rain.aSizeMax = RAIN_SIZE_MAX;
        rain.aWind = {0.0f, 0.0f, 0.0f};
        rain.aWindResponse = 0.6f;
        rain.aShape = GN3d::ParticleShape::BOX;
        rain.aSpreadSpawnPhase = true;
        if (!gRainCells[i].create(rain)) {
            return false;
        }

        GN3d::sPARTICLE_EMITTER_DESC snow = rain;
        snow.aSpread = 0.25f;
        snow.aSpeedMin = 2.0f;
        snow.aSpeedMax = 4.0f;
        snow.aMaxParticles = CLOUD_CUBE_SNOW_CAPACITY;
        snow.aLifeMin = snowFall + 1.0f;
        snow.aLifeMax = snowFall + 1.4f;
        snow.aGravity = {0.0f, SNOW_GRAVITY_Y, 0.0f};
        snow.aSizeMin = SNOW_SIZE_MIN;
        snow.aSizeMax = SNOW_SIZE_MAX;
        snow.aColorStart = GNg::sVEC4(0.97f, 0.98f, 1.0f, 0.95f);
        snow.aColorEnd = GNg::sVEC4(0.97f, 0.98f, 1.0f, 0.50f);
        snow.aWindResponse = 0.4f;
        if (!gSnowCells[i].create(snow)) {
            return false;
        }
    }
    gPrecipReady = true;
    return true;
}

namespace {

// 把一套粒子系统里存活的粒子追加到 out（多块云合并成一次提交）。
void appendAliveParticles(const GN3d::cPARTICLE_SYSTEM &system,
                          std::vector<GN3d::sPARTICLE> &out) {
    const GN3d::sPARTICLE *particles = system.particles();
    const uint32_t capacity = system.capacity();
    if (!particles) {
        return;
    }
    for (uint32_t i = 0; i < capacity; ++i) {
        if (system.alive(i)) {
            out.push_back(particles[i]);
        }
    }
}

// 提交一类降水（雨丝 streak≈20 / 雪团 streak=1，形状由片元按 streak 选）。
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
// L5b：多块云各自发射，这里按类型合并成**一次**提交（槽位只有 4 个）。
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
    // 只有最近 CLOUD_CUBE_RAIN_EMITTERS 个发射器会产粒子 ⇒ 预留数量按它算，别按 128 个盒子算。
    if (snowActive) {
        particles.reserve(CLOUD_CUBE_RAIN_EMITTERS * CLOUD_CUBE_SNOW_CAPACITY);
        for (uint32_t i = 0; i < s_cloudCellCount; ++i) {
            appendAliveParticles(gSnowCells[i], particles);
        }
        submitPrecipitation(viewProjection, right, camUp, particles, SNOW_STREAK);
    }
    if (rainActive) {
        particles.clear();
        particles.reserve(CLOUD_CUBE_RAIN_EMITTERS * CLOUD_CUBE_RAIN_CAPACITY);
        for (uint32_t i = 0; i < s_cloudCellCount; ++i) {
            appendAliveParticles(gRainCells[i], particles);
        }
        submitPrecipitation(viewProjection, right, camUp, particles, RAIN_STREAK);
    }
}

// 每帧：每块云按天气强度更新发射率/风/回收盒，再推进它自己的粒子模拟。
// 发射面 = 云块底面；回收盒 = 该云块正下方的整根降水柱（落地后 wrap 回云底）。
void updatePrecipitation(float dt) {
    if (!gPrecipReady) {
        return;
    }
    buildCloudCells();
    const GN3d::WeatherOutput out = gWeather.output();
    const float groundY = groundHeight();
    const float timeSeconds = out.sky.timeSeconds;
    const float coverage = out.sky.cloudCoverage;
    const GNg::sVEC3 wind = {out.precipitation.wind.x, 0.0f, out.precipitation.wind.z};

    gRainAlive = 0;
    gRainCapacity = 0;
    gSnowAlive = 0;
    gSnowCapacity = 0;
    gRainRate = 0.0f;
    gSnowRate = 0.0f;
    gPrecipBoxCenter = {0.0f, 0.0f, 0.0f};
    gPrecipBoxHalf = {0.0f, 0.0f, 0.0f};

    // 只让**离相机最近的** CLOUD_CUBE_RAIN_EMITTERS 块（可见且会降雨）发射：
    // 云块数上到 32 后，若每块都发射，粒子总量与顶点量会线性膨胀。
    bool emitRain[CLOUD_CUBE_MAX] = {};
    {
        uint32_t nearest[CLOUD_CUBE_RAIN_EMITTERS] = {};
        float nearestDist[CLOUD_CUBE_RAIN_EMITTERS] = {};
        uint32_t nearestCount = 0;
        const GNg::sVEC3 camPos = cameraPosition();
        for (uint32_t i = 0; i < s_cloudCellCount; ++i) {
            if (!cloudCellVisible(i, coverage) || !s_cloudCells[i].aRains) {
                continue;
            }
            const float dx = s_cloudCells[i].aX - camPos.x;
            const float dz = s_cloudCells[i].aZ - camPos.z;
            const float distSq = dx * dx + dz * dz;
            uint32_t insertAt = nearestCount;
            for (uint32_t k = 0; k < nearestCount; ++k) {
                if (distSq < nearestDist[k]) {
                    insertAt = k;
                    break;
                }
            }
            if (insertAt >= CLOUD_CUBE_RAIN_EMITTERS) {
                continue;
            }
            const uint32_t last = nearestCount < CLOUD_CUBE_RAIN_EMITTERS
                                      ? nearestCount
                                      : CLOUD_CUBE_RAIN_EMITTERS - 1;
            for (uint32_t k = last; k > insertAt; --k) {
                nearest[k] = nearest[k - 1];
                nearestDist[k] = nearestDist[k - 1];
            }
            nearest[insertAt] = i;
            nearestDist[insertAt] = distSq;
            if (nearestCount < CLOUD_CUBE_RAIN_EMITTERS) {
                ++nearestCount;
            }
        }
        for (uint32_t k = 0; k < nearestCount; ++k) {
            emitRain[nearest[k]] = true;
        }
    }

    for (uint32_t i = 0; i < s_cloudCellCount; ++i) {
        const sCLOUD_CELL &cell = s_cloudCells[i];
        const bool active = emitRain[i];
        const float x = cloudCellWorldX(i, wind.x, timeSeconds);
        const float z = cloudCellWorldZ(i, wind.z, timeSeconds);
        // 发射器盒子用整个簇的半径：雨从整朵云底下落，而不是簇里某个子盒子。
        // 尺寸随覆盖率放大（与 drawCloudCubes 的云块同倍率），否则雨天云变大而雨柱不变。
        const float sizeScale = cloudCellSizeScale(coverage);
        const float bottomY = std::fmax(cell.aY - cell.aHalf * sizeScale, CLOUD_BASE_ALTITUDE);
        const float emitHalf =
            (cell.aEmitHalf > 0.0f ? cell.aEmitHalf : cell.aHalf) * sizeScale;
        const float halfXZ = emitHalf * 0.8f;
        const float columnHalfY = std::fmax((bottomY - groundY) * 0.5f, 0.5f);
        const GNg::sVEC3 columnCenter = {x, (bottomY + groundY) * 0.5f, z};
        const GNg::sVEC3 columnHalf = {halfXZ, columnHalfY, halfXZ};
        const float rainRate = active ? CLOUD_CUBE_RAIN_RATE * out.precipitation.rain : 0.0f;
        const float snowRate = active ? CLOUD_CUBE_SNOW_RATE * out.precipitation.snow : 0.0f;

        gRainCells[i].setOrigin({x, bottomY, z});
        gRainCells[i].setRecycleBox(columnCenter, columnHalf);
        gRainCells[i].setWind(wind, 0.6f);
        gRainCells[i].setRate(rainRate);
        gRainCells[i].update(dt);

        gSnowCells[i].setOrigin({x, bottomY, z});
        gSnowCells[i].setRecycleBox(columnCenter, columnHalf);
        gSnowCells[i].setWind(wind, 0.4f);
        gSnowCells[i].setRate(snowRate);
        gSnowCells[i].update(dt);

        gRainAlive += gRainCells[i].activeCount();
        gRainCapacity += gRainCells[i].capacity();
        gSnowAlive += gSnowCells[i].activeCount();
        gSnowCapacity += gSnowCells[i].capacity();
        gRainRate += rainRate;
        gSnowRate += snowRate;
        // 叠层只画第一块正在降雨的云（多柱时单框会误导，这里保持最小改动）。
        if (active && gPrecipBoxHalf.y <= 0.0f) {
            gPrecipBoxCenter = columnCenter;
            gPrecipBoxHalf = columnHalf;
        }
    }

    // 叠层快照：一颗雨的实时状态 + 离相机最近的雨粒子。
    gRainSampleValid = false;
    gRainNearestDist = -1.0f;
    const GNg::sVEC3 cameraNow = cameraPosition();
    for (uint32_t c = 0; c < s_cloudCellCount; ++c) {
        const GN3d::sPARTICLE *rainParticles = gRainCells[c].particles();
        const uint32_t capacity = gRainCells[c].capacity();
        for (uint32_t i = 0; rainParticles != nullptr && i < capacity; ++i) {
            if (!gRainCells[c].alive(i)) {
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
    }

    // 雪的样本：只取第一颗存活的，用来在叠层里核对它的速度 Y（应当为负 = 向下落）。
    gSnowSampleValid = false;
    for (uint32_t c = 0; c < s_cloudCellCount && !gSnowSampleValid; ++c) {
        const GN3d::sPARTICLE *snowParticles = gSnowCells[c].particles();
        for (uint32_t i = 0; snowParticles != nullptr && i < gSnowCells[c].capacity(); ++i) {
            if (gSnowCells[c].alive(i)) {
                gSnowSamplePos = snowParticles[i].aPosition;
                gSnowSampleVel = snowParticles[i].aVelocity;
                gSnowSampleValid = true;
                break;
            }
        }
    }
}

// 3D pass 内、天空盒之后：云块（方块 = 云的包围盒/范围，画面不显示方块）。
// 显示数量由天气覆盖率决定（覆盖率越大云块越多）；每块云是独立体积，不是一整层天空。
void drawCloudCubes(const GNg::sMAT44 &viewProjection, const GNg::sVEC3 &camera) {
    if (!gCloudsEnabled) {
        gCloudCubeCount = 0;
        return;
    }
    buildCloudCells();
    const GN3d::WeatherOutput out = gWeather.output();
    const float coverage = out.sky.cloudCoverage;
    const float timeSeconds = out.sky.timeSeconds;
    const float overcast = weatherOvercast(out);
    const float sizeScale = cloudCellSizeScale(coverage);

    GNg::sCLOUD_CUBE_INSTANCE instances[CLOUD_CUBE_MAX];
    uint32_t count = 0;
    for (uint32_t i = 0; i < s_cloudCellCount; ++i) {
        if (!cloudCellVisible(i, coverage)) {
            continue;
        }
        const sCLOUD_CELL &cell = s_cloudCells[i];
        instances[count].aCenter[0] = cloudCellWorldX(i, out.precipitation.wind.x, timeSeconds);
        instances[count].aCenter[1] = cell.aY;
        instances[count].aCenter[2] = cloudCellWorldZ(i, out.precipitation.wind.z, timeSeconds);
        instances[count].aHalfSize = cell.aHalf * sizeScale;
        // 云块本色近白、略带冷调；每块一个 0.94..1.0 的微差（避免所有云一个色）。
        // 阴雨由 shader 按 overcast 压暗压灰；密度随阴雨略增（乌云更实）。
        instances[count].aTint[0] = cell.aTint;
        instances[count].aTint[1] = cell.aTint;
        instances[count].aTint[2] = cell.aTint * 1.02f;
        instances[count].aAlpha = 0.80f + 0.20f * overcast;
        ++count;
    }
    // 云盖（阴天整片云层）：一块覆盖全天空的大盒子，半边长取**负值**作为云盖标记；
    // 片元只步进"云底→云顶"的薄板，不会在大盒子里空跑。密度随 overcast 淡入。
    if (overcast > 0.02f && count < CLOUD_CUBE_MAX) {
        GNg::sCLOUD_CUBE_INSTANCE &deck = instances[count];
        deck.aCenter[0] = 0.0f;
        deck.aCenter[1] = CLOUD_BASE_ALTITUDE + 210.0f;
        deck.aCenter[2] = 0.0f;
        deck.aHalfSize = -2600.0f; // 负 = 云盖；覆盖到约 85° 仰角
        deck.aTint[0] = 0.88f;
        deck.aTint[1] = 0.90f;
        deck.aTint[2] = 0.94f;
        deck.aAlpha = overcast;
        ++count;
    }
    if (count == 0) {
        gCloudCubeCount = 0;
        return;
    }

    // 半透明云必须按**由远到近**提交：管线是 alpha 混合且不写深度，顺序错了后画的云会盖到先画的云上。
    // 云盖是"最底层"，永远先画（否则它会盖住所有云团）。
    std::sort(instances, instances + count,
              [&camera](const GNg::sCLOUD_CUBE_INSTANCE &a, const GNg::sCLOUD_CUBE_INSTANCE &b) {
                  const float ax = a.aCenter[0] - camera.x;
                  const float ay = a.aCenter[1] - camera.y;
                  const float az = a.aCenter[2] - camera.z;
                  const float bx = b.aCenter[0] - camera.x;
                  const float by = b.aCenter[1] - camera.y;
                  const float bz = b.aCenter[2] - camera.z;
                  const float da = a.aHalfSize < 0.0f ? 1.0e18f : ax * ax + ay * ay + az * az;
                  const float db = b.aHalfSize < 0.0f ? 1.0e18f : bx * bx + by * by + bz * bz;
                  return da > db;
              });

    GNg::sCLOUD_CUBE_PARAMS params;
    params.aViewProj = viewProjection;
    params.aCameraPos = camera;
    params.aSunDir = out.sky.sunDirection;
    params.aSunIntensity = out.sky.sunIntensity;
    params.aAmbient = out.sky.zenithColor;
    params.aFogColor = out.fog.color;
    params.aFogDensity = out.fog.density;
    params.aBaseAltitude = CLOUD_BASE_ALTITUDE; // 全场景云底：所有云在同一个高度被削平
    params.aOvercast = overcast; // 阴雨程度：雨/雪天云体压暗压灰（乌云）
    params.aInstances = instances;
    params.aCount = count;
    if (GNg::GN_VkDrawCloudCubes(params)) {
        // 叠层复用这两个字段：layers = 本次提交的云块数。
        gCloudCubeCount = count;
        gCloudCubeCoverage = coverage;
    }
}

// L5b：体积云整层已被立方体云取代（用户要求"不要一整层盖住天空"），相关渲染器/着色器/噪声
// 生成与 208 B UBO 已全部删除；这里不再保留回退路径。

