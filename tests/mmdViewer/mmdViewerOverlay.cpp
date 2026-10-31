// tests/mmdViewer/mmdViewerOverlay.cpp — MMD 查看器：坐标信息叠层与粒子调试包围盒
//
// 职责：左上角坐标信息叠层（H 键）的文字排版与样本粒子屏幕投影，
//       以及 V 键的「最近若干粒子四边形 AABB」调试线框。
//       不涉及相机输入、天气状态机与场景绘制（那些在各自模块里）。

#include "mmdViewerShared.h"
#include "GN_Physics.h" // sDEBUG_LINE（粒子四边形调试线框）

#include <cmath>
#include <cstdio>

namespace {

// 每个粒子的四边形包围盒（V 键开关）：只画离相机最近的若干个，全量 6 万+ 不可行。
const uint32_t PARTICLE_DEBUG_BOX_COUNT = 16;

} // namespace

// 窗口左上角的坐标信息叠层（H 键切换）：模型/地面/脚下面片/相机的实际数值。
bool gShowCoordInfo = true;
// 叠层文字输出器：独立 cTEXT_ART（旧示例 samples/04_font.cpp 的用法），不依赖 RunRec 缓冲。
GNg::cTEXT_ART gInfoText;
// 粒子四边形包围盒开关（V 键切换）。
bool gParticleBoxes = false;

namespace {

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

} // namespace

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

namespace {

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

} // namespace

// V 键：雨、雪两套粒子系统各画最近的若干四边形 AABB（旧版只画雨，雪天看不到任何盒子）。
void drawParticleDebugBoxes(const GNg::sVEC3 &right, const GNg::sVEC3 &camUp) {
    if (!gParticleBoxes || !gPrecipReady) {
        return;
    }
    drawParticleDebugBoxesFor(gRainParticles, RAIN_STREAK, right, camUp);
    drawParticleDebugBoxesFor(gSnowParticles, SNOW_STREAK, right, camUp);
}
