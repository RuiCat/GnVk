#pragma once
#ifndef GN_ATMOSPHERE_PARAMS_H
#define GN_ATMOSPHERE_PARAMS_H
// ╔══════════════════════════════════════════════════════════╗
// ║  GN_mini — 大气散射 CPU 基座（L4a，Hillaire 2020 参数）   ║
// ╚══════════════════════════════════════════════════════════╝
//
// 职责：大气模型的 CPU 侧契约与参考实现——Rayleigh/Mie/Ozone 系数与尺度高度、
// 单次散射光学深度积分、透射率 LUT（RGBA16F 上传用的 float 布局）与双线性采样。
// 本层不碰 Vulkan、不分配 GPU 资源；与 docs/LIGHTING_PLAN.md §4.6（L4）配套。
//
// 参数取自 Hillaire 2020 "A Scalable and Production Ready Sky and Atmosphere Rendering
// Technique"（库内 docs/papers/pdf/hillaire2020-sky-atmosphere.pdf）；LUT 参数化先用
// 线性 (altitude, mu)，Hillaire 的非线性 uv 映射待 GPU 接线时替换（只影响采样精度）。
//
// 命名：函数 camelCase、类型 PascalCase、常量 UPPER_SNAKE。
#include <cstddef>
#include <cstdint>

namespace GN3d {

// 透射率 LUT 默认尺寸（Hillaire 2020：256×64，行=v(高度)、列=u(mu)）。
constexpr int ATMOSPHERE_TRANSMITTANCE_WIDTH = 256;
constexpr int ATMOSPHERE_TRANSMITTANCE_HEIGHT = 64;
// 射线积分的采样数（CPU LUT 生成用；GPU shader 侧可另调）。
constexpr int ATMOSPHERE_TRANSMITTANCE_STEPS = 40;

// 大气参数（SI 单位：米、1/米）。默认值即地球晴天模型。
struct sATMOSPHERE_PARAMS {
    float groundRadius;          // 行星半径（m），默认 6_360_000
    float atmosphereRadius;      // 大气顶半径（m），默认 6_420_000
    float rayleighScattering[3]; // Rayleigh 散射系数（1/m，RGB）
    float rayleighScaleHeight;   // Rayleigh 尺度高度（m），默认 8_000
    float mieScattering;         // Mie 散射系数（1/m）
    float mieAbsorption;         // Mie 吸收系数（1/m）
    float mieScaleHeight;        // Mie 尺度高度（m），默认 1_200
    float mieAnisotropy;         // Mie 相位各向异性 g ∈ (-1,1)
    float ozoneAbsorption[3];    // Ozone 吸收系数（1/m，RGB）
    float ozoneCenterHeight;     // Ozone 帐篷中心高度（m），默认 25_000
    float ozoneWidth;            // Ozone 帐篷半宽（m），默认 15_000
    float sunIntensity;          // 太阳辐照度（无量纲），默认 20
};

// 地球默认参数（Hillaire 2020 附表值）。
sATMOSPHERE_PARAMS makeDefaultAtmosphereParams();

// 校验参数：半径/尺度高度为正、散射/吸收非负、mieAnisotropy ∈ (-1,1)、全部有限。
// 非法输入返回 false，并把钳制后的结果写入 corrected（可为 nullptr）。
bool validateAtmosphereParams(const sATMOSPHERE_PARAMS& params,
                              sATMOSPHERE_PARAMS* corrected = nullptr);

// 单射线光学深度积分（不写 GPU）：origin 在行星中心坐标系（行星中心为原点）。
// outTransmittance 必须指向 3 个 float；ray 命中地面时输出全 0（不做地面反弹）。
// 返回 false 表示参数/方向非法（输出写 0）。dir 可为非单位向量，内部归一化。
bool computeTransmittance(const sATMOSPHERE_PARAMS& params, const float origin[3],
                          const float dir[3], float outTransmittance[3]);

// 生成透射率 LUT：rgba 为 width*height*4 个 float（RGB=透射率，A=1）。
// 行 v=高度（0=地面，1=大气顶），列 u=mu（0=-1 向下，1=+1 天顶）。
// 空指针/尺寸 <=0 返回 false 并清零。
bool computeTransmittanceLut(const sATMOSPHERE_PARAMS& params, int width, int height,
                             float* rgba);

// 双线性采样透射率 LUT（height01=高度归一化 [0,1]（0=地面，1=大气顶），
// mu=视线与天顶夹角余弦，均钳制到合法域）。输出 3 个 float；空指针返回 false 并写 0。
bool evalTransmittanceLut(const float* rgba, int width, int height, float height01, float mu,
                          float outTransmittance[3]);

// 与大气顶球面的射线求交：返回最大正根 t（未命中返回 -1）；可选输出是否命中地面。
float intersectAtmosphere(const sATMOSPHERE_PARAMS& params, const float origin[3],
                          const float dir[3], bool* hitsGround);

// ── L4b：单次散射天空（CPU 参考；GPU sky-view LUT 接线前先锁数值）──────────
// sky-view LUT 默认尺寸与积分步数（Hillaire 2020 量级；u=相对太阳方位角，v=天顶角）。
constexpr int ATMOSPHERE_SKYVIEW_LUT_WIDTH = 192;
constexpr int ATMOSPHERE_SKYVIEW_LUT_HEIGHT = 108;
constexpr int ATMOSPHERE_SKYVIEW_STEPS = 32;

// 单方向天空辐亮度（单次散射 + 透射率 LUT 查表；viewDir/sunDir 需为世界空间 y-up）。
// 视线命中地面/无大气交会时输出 0（本层不做地面反弹）。空指针返回 false。
bool computeSkyRadiance(const sATMOSPHERE_PARAMS& params, const float* transmittanceLut,
                        int lutWidth, int lutHeight, const float viewDir[3],
                        const float sunDir[3], float outRgb[3]);

// 生成 sky-view LUT：rgba 为 width*height*4 个 float（RGB=辐亮度，A=1）。
// u∈[0,1] 映射相对太阳方位角 [-π,π]，v∈[0,1] 映射天顶角（0=天顶，1=地平线）。
bool computeSkyViewLut(const sATMOSPHERE_PARAMS& params, const float* transmittanceLut,
                       int lutWidth, int lutHeight, const float sunDir[3], int width,
                       int height, float* rgba);

// 采样 sky-view LUT（与 computeSkyViewLut 同一参数化；越界钳制）。空指针返回 false。
bool evalSkyViewLut(const float* rgba, int width, int height, const float viewDir[3],
                    const float sunDir[3], float outRgb[3]);

// L4d 准备：天气/天空参数 → 大气参数的映射输入。
// cloudCoverage/fogDensity ∈ [0,1]（越界钳制）；sunIntensityScale ∈ [0,4]。
struct sATMOSPHERE_WEATHER_INPUT {
    float cloudCoverage;
    float fogDensity;
    float sunIntensityScale;
};

// 把天气强度映射为大气参数：云/雾提高 Mie 散射与吸收、降低太阳强度；结果经 validate 钳制。
// 晴空（全 0 / scale=1）返回 base 的等价参数。用于天空 shader 接线时把 WeatherSystem 接到 L4。
sATMOSPHERE_PARAMS mapWeatherToAtmosphere(const sATMOSPHERE_PARAMS& base,
                                          const sATMOSPHERE_WEATHER_INPUT& weather);

} // namespace GN3d

#endif // GN_ATMOSPHERE_PARAMS_H
