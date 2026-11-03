// tests/cornellBox/cornellPathTracer.h — CPU 路径追踪参考（Cornell box）
//
// 职责：纯 CPU、无 Vulkan；用 cornellGeometry 的四边形/球做解析求交，输出线性 HDR RGB。
// 供 cornellPt 独立可执行使用，也可被引擎侧按需调用（当前只做独立参考）。
#ifndef CORNELL_PATH_TRACER_H
#define CORNELL_PATH_TRACER_H

#include "cornellGeometry.h"

#include <vector>

namespace Cornell {

struct sPtOptions {
    int aWidth;
    int aHeight;
    int aSamples;
    int aPose;
    bool aAreaLight; // true = 真面光（classic）；false = 方向光（与引擎 sun 对齐）
    unsigned int aSeed;
    sPtOptions();
};

// 渲染线性 HDR RGB（width*height*3，行主序）。成功返回 true。
bool renderPathTracer(const sCornellGeometry &geometry, const sPtOptions &options,
                      std::vector<float> *outLinearRgb);

// 线性 HDR → 曝光 + ACES + sRGB → PNG（RGB8）。成功返回 true。
bool savePngRgb(const char *path, const std::vector<float> &linearRgb, int width, int height,
                float exposure);

} // namespace Cornell
#endif // CORNELL_PATH_TRACER_H
