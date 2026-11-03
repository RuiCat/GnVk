// tests/cornellBox/cornellCpuRtProbe.cpp — CPU 软件光追预览的纯 CPU 自检
//
// 用法：make cornellRtProbe
// 从 canonical pose 0 渲染 320×200，打印均值/主色；用于在没有 GPU 时确认
// renderCpuRt 确实产生了非黑、带红/绿墙色的图像。
#include "cornellCpuRt.h"
#include "cornellGeometry.h"
#include "cornellPng.h"

#include <cmath>
#include <cstdio>
#include <vector>

namespace {

float dotf(const float a[3], const float b[3]) {
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

void crossf(const float a[3], const float b[3], float out[3]) {
    out[0] = a[1] * b[2] - a[2] * b[1];
    out[1] = a[2] * b[0] - a[0] * b[2];
    out[2] = a[0] * b[1] - a[1] * b[0];
}

void normalizef(float v[3]) {
    const float len = std::sqrt(dotf(v, v));
    if (len > 1e-8f) {
        v[0] /= len;
        v[1] /= len;
        v[2] /= len;
    }
}

} // namespace

int main() {
    const Cornell::sPose &pose = Cornell::CORNELL_POSES[2];
    const float eye[3] = {pose.aPosition.x, pose.aPosition.y, pose.aPosition.z};
    float fwd[3] = {pose.aFocus.x - eye[0], pose.aFocus.y - eye[1], pose.aFocus.z - eye[2]};
    normalizef(fwd);
    const float worldUp[3] = {0.0f, 1.0f, 0.0f};
    float right[3];
    crossf(fwd, worldUp, right);
    normalizef(right);
    float up[3];
    crossf(right, fwd, up);

    Cornell::sCornellGeometry debugGeometry;
    Cornell::buildCornellGeometry(false, &debugGeometry);
    for (size_t i = 0; i < debugGeometry.aMaterials.size(); ++i) {
        const Cornell::sMaterial &m = debugGeometry.aMaterials[i];
        std::printf("[cornellRtProbe] material %zu metallic=%.2f albedo=(%.2f,%.2f,%.2f)\n",
                    i, m.aMetallic, m.aAlbedo[0], m.aAlbedo[1], m.aAlbedo[2]);
    }

    std::vector<unsigned char> rgba;
    if (!Cornell::renderCpuRt(320, 200, eye, fwd, right, up, 45.0f, 0, &rgba)) {
        std::fprintf(stderr, "[cornellRtProbe] renderCpuRt 失败\n");
        return 1;
    }
    double sum[3] = {0.0, 0.0, 0.0};
    int nonBlack = 0;
    const int pixels = 320 * 200;
    for (int i = 0; i < pixels; ++i) {
        const unsigned char *p = &rgba[static_cast<size_t>(i) * 4u];
        for (int c = 0; c < 3; ++c) sum[c] += p[c];
        if (p[0] + p[1] + p[2] > 30) ++nonBlack;
    }
    std::printf("[cornellRtProbe] mean=(%.1f, %.1f, %.1f) nonBlack=%.1f%%\n",
                sum[0] / pixels, sum[1] / pixels, sum[2] / pixels,
                100.0 * nonBlack / pixels);
    Cornell::writeRgbPng(".local/cornellbox/cpu-rt-probe.png", rgba, 320, 200);
    if (nonBlack < pixels / 10) {
        std::fprintf(stderr, "[cornellRtProbe] 图像几乎全黑\n");
        return 1;
    }
    return 0;
}
