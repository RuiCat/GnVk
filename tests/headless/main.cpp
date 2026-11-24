// tests/headless/main.cpp — 无窗口（离屏）渲染验收
//
// 与 tests/gpu 的区别：gpuCheck 需要一个可见窗口（走 GLFW surface + swapchain），
// 本程序调用 GN_StartRunHeadless，完全不碰窗口系统（连 glfwInit 都不调），
// 由引擎自建离屏图渲染，再用 GN_GrabFrame 回读像素做断言。
// 因此在没有 X11/Wayland、没有 /dev/dri 的容器里也能跑（配 lavapipe 软件 Vulkan）。
//
// 画四个不同颜色的象限（blendmode=0 不做混合，颜色可精确断言），
// 回读后按「图像行」采样四个象限中心，验证：
//   * 渲染真的发生了（不是全黑/全清屏色）
//   * 2D 几何位置正确（四个象限各自颜色对得上，能抓到 Y 翻转之类的投影错误）
//   * alpha=255（清屏为不透明）
//
// 用法: ./build/headlessCheck [宽 高 帧数 输出png] [frame]
//
// 第二个模式（末位参数写 frame）验的是 P1 的「编排提取不改画面」：
// 同一帧分别用 cRENDER_FRAME（新路径）与裸 sFRAME_3D_SCOPE（旧路径，示例原先的写法）
// 各渲染一次，回读像素逐字节比较。容器里没有模型资产也能跑——天空盒就足够证明
// 「进 pass/画天空盒/收 pass」这三步的顺序与状态没有变。
#include <GN.h>

#include <png.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {

int gWidth = 640;
int gHeight = 400;
int gFrames = 8;
const char *gOutPath = "headless.png";
int gFailures = 0;

// ── P1 帧编排等价性 ──
// 前两帧走 cRENDER_FRAME，后两帧走裸 sFRAME_3D_SCOPE；在第 1、3 帧各回读一次像素再比对。
// 两帧都取「第 2 帧」是为了让第一帧的惰性初始化（管线/描述符）不影响结果。
bool gFrameParity = false;
int gFrameIndex = 0;
std::vector<unsigned char> gPixelsNew;
std::vector<unsigned char> gPixelsOld;
int gPixelWidth = 0;
int gPixelHeight = 0;
long gParityDiff = -1;

// 四个象限颜色。引擎 2D 坐标系 y=0 在屏幕顶部、y 向下增大（Vulkan NDC 的 -1 是顶部，
// 所以 y_ndc = 2y/h - 1 正好得到 y=0 在顶行）；回读图像的行 0 也在顶部，两者同一朝向。
const unsigned int kColorTL = 0xFFFF0000u; // 红
const unsigned int kColorTR = 0xFF00FF00u; // 绿
const unsigned int kColorBL = 0xFF0000FFu; // 蓝
const unsigned int kColorBR = 0xFFFFFFFFu; // 白

bool writeRgbPng(const char *path, const unsigned char *rgba, int width, int height) {
    FILE *file = std::fopen(path, "wb");
    if (!file) {
        std::fprintf(stderr, "[headless] 无法创建 %s\n", path);
        return false;
    }
    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    if (!png) {
        std::fclose(file);
        return false;
    }
    png_infop info = png_create_info_struct(png);
    if (!info) {
        png_destroy_write_struct(&png, nullptr);
        std::fclose(file);
        return false;
    }
    if (setjmp(png_jmpbuf(png))) {
        png_destroy_write_struct(&png, &info);
        std::fclose(file);
        return false;
    }
    png_init_io(png, file);
    png_set_IHDR(png, info, static_cast<png_uint_32>(width), static_cast<png_uint_32>(height), 8,
                 PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
                 PNG_FILTER_TYPE_DEFAULT);
    png_write_info(png, info);
    std::vector<png_byte> row(static_cast<size_t>(width) * 3u);
    for (int y = 0; y < height; ++y) {
        const unsigned char *src = rgba + static_cast<size_t>(y) * width * 4u;
        for (int x = 0; x < width; ++x) {
            row[static_cast<size_t>(x) * 3u + 0] = src[x * 4 + 0];
            row[static_cast<size_t>(x) * 3u + 1] = src[x * 4 + 1];
            row[static_cast<size_t>(x) * 3u + 2] = src[x * 4 + 2];
        }
        png_write_row(png, row.data());
    }
    png_write_end(png, nullptr);
    png_destroy_write_struct(&png, &info);
    std::fclose(file);
    return true;
}

struct sPixel {
    int r, g, b, a;
};

sPixel sample(const unsigned char *rgba, int width, int x, int y) {
    const unsigned char *p = rgba + (static_cast<size_t>(y) * width + x) * 4u;
    sPixel px;
    px.r = p[0];
    px.g = p[1];
    px.b = p[2];
    px.a = p[3];
    return px;
}

bool near(int actual, int expected, int tol) {
    return actual >= expected - tol && actual <= expected + tol;
}

void checkPixel(const char *name, sPixel px, unsigned int expect, int tol) {
    const int er = (int)((expect >> 16) & 0xffu);
    const int eg = (int)((expect >> 8) & 0xffu);
    const int eb = (int)(expect & 0xffu);
    const bool ok = near(px.r, er, tol) && near(px.g, eg, tol) && near(px.b, eb, tol);
    std::printf("[%s] %-24s RGBA=%d,%d,%d,%d expected=%d,%d,%d(±%d)\n", ok ? "PASS" : "FAIL", name,
                px.r, px.g, px.b, px.a, er, eg, eb, tol);
    if (!ok) ++gFailures;
}

// 回读一帧像素到 out（失败返回 false 并已计入 failures）。
bool grabInto(std::vector<unsigned char> &out, const char *label) {
    int w = 0;
    int h = 0;
    int size = 0;
    unsigned char *rgba = GN_GrabFrame(&w, &h, &size);
    if (!rgba || w <= 0 || h <= 0) {
        std::printf("[FAIL] grab                    GN_GrabFrame 返回空（%s）\n", label);
        ++gFailures;
        return false;
    }
    out.assign(rgba, rgba + static_cast<size_t>(w) * (size_t)h * 4u);
    gPixelWidth = w;
    gPixelHeight = h;
    GN_GrabFrameFree(rgba);
    return true;
}

// 帧编排等价性：第 1 帧（cRENDER_FRAME）与第 3 帧（裸 sFRAME_3D_SCOPE）各存一份，
// 第 3 帧后逐字节比对。两帧都隔一帧取，是为了让首帧的惰性初始化（管线/描述符）不影响结果。
void onFrameParity(int index) {
    if (index == 1) {
        grabInto(gPixelsNew, "cRENDER_FRAME");
        return;
    }
    if (index != 3) return;
    if (!grabInto(gPixelsOld, "sFRAME_3D_SCOPE")) return;
    if (gPixelsNew.empty() || gPixelsNew.size() != gPixelsOld.size()) {
        std::printf("[FAIL] parity.size            新 %zu vs 旧 %zu\n", gPixelsNew.size(),
                    gPixelsOld.size());
        ++gFailures;
        return;
    }
    gParityDiff = 0;
    long lit = 0;
    for (size_t i = 0; i + 3 < gPixelsNew.size(); i += 4) {
        const int sum = (int)gPixelsNew[i] + (int)gPixelsNew[i + 1] + (int)gPixelsNew[i + 2];
        if (sum > 30) ++lit;
        if (gPixelsNew[i] != gPixelsOld[i] || gPixelsNew[i + 1] != gPixelsOld[i + 1] ||
            gPixelsNew[i + 2] != gPixelsOld[i + 2] || gPixelsNew[i + 3] != gPixelsOld[i + 3]) {
            ++gParityDiff;
        }
    }
    // 只证明"两边一样"是不够的：两张全黑图也完全一样。这里同时要求不是空画面。
    const long pixels = (long)(gPixelsNew.size() / 4u);
    std::printf("[%s] %-24s lit=%ld/%ld (期望 >10%%)\n", lit * 10 > pixels ? "PASS" : "FAIL",
                "frame.notBlank", lit, pixels);
    if (!(lit * 10 > pixels)) ++gFailures;
    std::printf("[%s] %-24s diffBytes=%ld/%zu\n", gParityDiff == 0 ? "PASS" : "FAIL",
                "frame.parity", gParityDiff, gPixelsNew.size());
    if (gParityDiff != 0) ++gFailures;
    if (writeRgbPng(gOutPath, gPixelsNew.data(), gPixelWidth, gPixelHeight)) {
        std::printf("[info] cRENDER_FRAME 路径已写出 %s\n", gOutPath);
    }
}

void onFrame(int index) {
    if (gFrameParity) {
        onFrameParity(index);
        return;
    }

    int w = 0, h = 0, size = 0;
    unsigned char *rgba = GN_GrabFrame(&w, &h, &size);
    if (!rgba || w <= 0 || h <= 0) {
        std::printf("[FAIL] grab                    GN_GrabFrame 返回空\n");
        ++gFailures;
        return;
    }
    std::printf("[info] 回读 %dx%d (%d 字节，RGBA8)\n", w, h, size);

    const int tol = 4; // 8bit sRGB 直出，容差只吸收取整误差
    checkPixel("quad.topLeft", sample(rgba, w, w / 4, h / 4), kColorTL, tol);
    checkPixel("quad.topRight", sample(rgba, w, w * 3 / 4, h / 4), kColorTR, tol);
    checkPixel("quad.bottomLeft", sample(rgba, w, w / 4, h * 3 / 4), kColorBL, tol);
    checkPixel("quad.bottomRight", sample(rgba, w, w * 3 / 4, h * 3 / 4), kColorBR, tol);

    const sPixel corner = sample(rgba, w, 0, 0);
    std::printf("[%s] %-24s alpha=%d expected=255\n", corner.a == 255 ? "PASS" : "FAIL",
                "clear.alpha", corner.a);
    if (corner.a != 255) ++gFailures;

    // 全黑检测：四个象限至少三个分量非零，否则说明根本没渲染（只清了屏）
    long lit = 0;
    for (int y = 0; y < h; y += 8) {
        for (int x = 0; x < w; x += 8) {
            const sPixel px = sample(rgba, w, x, y);
            if (px.r + px.g + px.b > 30) ++lit;
        }
    }
    const long total = (long)((h + 7) / 8) * ((w + 7) / 8);
    std::printf("[%s] %-24s lit=%ld/%ld (期望 >90%%)\n", lit * 10 > total * 9 ? "PASS" : "FAIL",
                "frame.notBlank", lit, total);
    if (!(lit * 10 > total * 9)) ++gFailures;

    if (writeRgbPng(gOutPath, rgba, w, h)) {
        std::printf("[info] 已写出 %s\n", gOutPath);
    } else {
        std::printf("[FAIL] png.write               %s 写入失败\n", gOutPath);
        ++gFailures;
    }
    GN_GrabFrameFree(rgba);
}

} // namespace

extern "C" {

GN_CALL void AppInit() {}
GN_CALL void AppUnInit() {}

GN_CALL void AppRun(uint nTime) {
    (void)nTime;
}

GN_CALL void AppDisplay3D(uint nTime) {
    (void)nTime; // 本验收只验 2D 路径（3D 见 gpuCheck / cornellBox）
}

GN_CALL void AppDisplay2D(uint nTime) {
    (void)nTime;
    if (gFrameParity) {
        // 固定相机：两帧之间除了「用哪条路径进 pass」以外没有任何差异。
        static GN3d::cCAMERA camera;
        static bool cameraReady = false;
        if (!cameraReady) {
            camera.Init();
            camera.SetFov(45.0f);
            camera.SetNF(0.1f, 100.0f);
            camera.SetOrbit(GNg::sVEC3{0.0f, 1.0f, 0.0f}, 6.0f, 0.0f, 0.15f);
            cameraReady = true;
        }
        GNg::sMAT44 view;
        GNg::sMAT44 projection;
        GNg::sMAT44 viewProjection;
        const float aspect = (float)gWidth / (float)gHeight;
        if (!camera.BuildViewProj(aspect, &view, &projection, &viewProjection)) {
            ++gFailures;
            return;
        }
        const GNg::sVEC3 eye = camera.GetCoordV();
        const bool useNewPath = (gFrameIndex < 2);
        ++gFrameIndex;
        if (useNewPath) {
            GNg::cRENDER_FRAME frame;
            GNg::sRENDER_FRAME_DESC desc;
            desc.aViewProj = &viewProjection;
            desc.aCameraPos = &eye;
            desc.aLightUBO = nullptr; // 无光源 UBO：3D pass 用零光，两条路径完全一致
            desc.aLightUBOSize = 0;
            if (!frame.Begin(desc)) {
                std::printf("[FAIL] frame.begin            cRENDER_FRAME::Begin 失败\n");
                ++gFailures;
                return;
            }
            frame.End();
        } else {
            GNg::sFRAME_3D_SCOPE scope(&viewProjection, &eye, nullptr, 0, true);
            if (!scope.Valid()) {
                std::printf("[FAIL] frame.begin            sFRAME_3D_SCOPE 失败\n");
                ++gFailures;
            }
        }
        return;
    }
    const int cx = gWidth / 2;
    const int cy = gHeight / 2;
    // blendmode=0（不混合）→ 像素就是顶点色，可精确断言
    GNg::ImageDraw(0, 0, 0, cx, cy, kColorTL, 0, 0, 0, 0, 0, 0);
    GNg::ImageDraw(0, cx, 0, cx, cy, kColorTR, 0, 0, 0, 0, 0, 0);
    GNg::ImageDraw(0, 0, cy, cx, cy, kColorBL, 0, 0, 0, 0, 0, 0);
    GNg::ImageDraw(0, cx, cy, cx, cy, kColorBR, 0, 0, 0, 0, 0, 0);
}

} // extern "C"

int main(int argc, char **argv) {
    // 注意：不走 GN_MAIN/GN_EXIT 宏——那套会调 GT_AppArgsSet → glfwInit，
    // 无窗口环境下会直接失败。这里只要 Vulkan 设备，不需要窗口系统。
    if (argc > 1) gWidth = std::atoi(argv[1]);
    if (argc > 2) gHeight = std::atoi(argv[2]);
    if (argc > 3) gFrames = std::atoi(argv[3]);
    if (argc > 4) gOutPath = argv[4];
    if (argc > 5) gFrameParity = (std::strcmp(argv[5], "frame") == 0);
    if (gFrameParity && gFrames < 4) gFrames = 4; // 需要 4 帧：每条路径各 2 帧（首帧只做初始化）
    if (gFrames < 1) gFrames = 1;

    std::printf("[info] 无窗口渲染验收 %dx%d，%d 帧%s\n", gWidth, gHeight, gFrames,
                gFrameParity ? "（帧编排等价性）" : "");

    GN_InitLoadCallBackFunc(AppInit, AppUnInit);
    GN_InitLoopCallBackFunc(AppDisplay2D, AppDisplay3D, AppRun);

    const bool ok = GN_StartRunHeadless((short)gWidth, (short)gHeight, gFrames, onFrame);
    if (!ok) {
        // 退出码 2 = SKIP（没有 Vulkan 设备），与 tests/gpu 的约定一致；SKIP 不算通过。
        std::printf("[SKIP] headless.init            GN_StartRunHeadless 失败（无可用 Vulkan 设备）\n");
        return 2;
    }
    std::printf("---- HEADLESS SUMMARY: %s (%d failures) ----\n", gFailures == 0 ? "PASS" : "FAIL",
                gFailures);
    return gFailures == 0 ? 0 : 1;
}
