// tests/cornellBox/cornellPtMain.cpp — CPU 路径追踪参考的程序入口
//
// 用法：make cornellPt
//       ./build/cornellPt --spp=64 --width=640 --height=400 --pose=0 --scene=area --out=/tmp/pt.png
// 纯 CPU：不需要 Vulkan 设备/窗口；用于给引擎 Cornell 程序提供 ground truth。
#include "cornellGeometry.h"
#include "cornellPathTracer.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

void printUsage() {
    std::printf("cornellPt — Cornell box CPU path tracer\n"
                "  --spp=N         samples per pixel (default 64)\n"
                "  --width=W       image width (default 640)\n"
                "  --height=H      image height (default 400)\n"
                "  --pose=0..3     canonical camera pose (default 0)\n"
                "  --scene=area|sun  area light or directional sun (default area)\n"
                "  --metal         use perfect mirror for the metal sphere (default: diffuse approx.)\n"
                "  --seed=N        random seed (default 1; same seed = deterministic)\n"
                "  --out=FILE      output PNG (default cornell-pt.png)\n");
}

bool parseArg(const char *arg, const char *name, const char **value) {
    const size_t length = std::strlen(name);
    if (std::strncmp(arg, name, length) != 0) return false;
    *value = arg + length;
    return true;
}

} // namespace

int main(int argc, char **argv) {
    Cornell::sPtOptions options;
    const char *outPath = "cornell-pt.png";
    bool areaLight = true;
    bool metal = false;

    for (int i = 1; i < argc; ++i) {
        const char *value = nullptr;
        if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0) {
            printUsage();
            return 0;
        } else if (parseArg(argv[i], "--spp=", &value)) {
            options.aSamples = std::atoi(value);
        } else if (parseArg(argv[i], "--width=", &value)) {
            options.aWidth = std::atoi(value);
        } else if (parseArg(argv[i], "--height=", &value)) {
            options.aHeight = std::atoi(value);
        } else if (parseArg(argv[i], "--pose=", &value)) {
            options.aPose = std::atoi(value);
        } else if (parseArg(argv[i], "--out=", &value)) {
            outPath = value;
        } else if (parseArg(argv[i], "--seed=", &value)) {
            options.aSeed = static_cast<unsigned int>(std::strtoul(value, nullptr, 10));
            if (options.aSeed == 0u) options.aSeed = 1u;
        } else if (std::strcmp(argv[i], "--metal") == 0) {
            metal = true;
        } else if (parseArg(argv[i], "--scene=", &value)) {
            if (std::strcmp(value, "sun") == 0) {
                areaLight = false;
            } else if (std::strcmp(value, "area") == 0) {
                areaLight = true;
            } else {
                std::fprintf(stderr, "[cornellPt] 未知 scene: %s\n", value);
                return 2;
            }
        } else {
            std::fprintf(stderr, "[cornellPt] 未知参数: %s\n", argv[i]);
            printUsage();
            return 2;
        }
    }

    if (options.aSamples <= 0 || options.aWidth <= 0 || options.aHeight <= 0) {
        std::fprintf(stderr, "[cornellPt] 分辨率/采样数必须 > 0\n");
        return 2;
    }
    options.aAreaLight = areaLight;

    Cornell::sCornellGeometry geometry;
    Cornell::buildCornellGeometry(areaLight, &geometry);
    // v1 默认把金属球按漫反射近似：完美镜面 + 小面光的焦散方差极大（firefly），
    // 且金属反射不属于漫反射 GI 的验收范围；--metal 可打开真实镜面。
    if (!metal) {
        geometry.aMaterials[Cornell::eMAT_METAL].aMetallic = 0.0f;
    }

    std::vector<float> linearRgb;
    const auto start = std::chrono::steady_clock::now();
    if (!Cornell::renderPathTracer(geometry, options, &linearRgb)) {
        std::fprintf(stderr, "[cornellPt] 渲染失败\n");
        return 1;
    }
    const auto end = std::chrono::steady_clock::now();
    const double seconds = std::chrono::duration<double>(end - start).count();

    int nanCount = 0;
    int infCount = 0;
    float maxValue = 0.0f;
    for (size_t i = 0; i < linearRgb.size(); ++i) {
        if (std::isnan(linearRgb[i])) ++nanCount;
        if (std::isinf(linearRgb[i])) ++infCount;
        if (linearRgb[i] > maxValue) maxValue = linearRgb[i];
    }
    std::printf("[cornellPt] linear max=%.3f nan=%d inf=%d\n", maxValue, nanCount, infCount);

    if (!Cornell::savePngRgb(outPath, linearRgb, options.aWidth, options.aHeight, 1.0f)) {
        std::fprintf(stderr, "[cornellPt] 写 PNG 失败: %s\n", outPath);
        return 1;
    }
    std::printf("[cornellPt] %dx%d, %d spp, pose=%d, scene=%s, seed=%u -> %s (%.2f s)\n",
                options.aWidth, options.aHeight, options.aSamples, options.aPose,
                areaLight ? "area" : "sun", options.aSeed, outPath, seconds);
    return 0;
}
