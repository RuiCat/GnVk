// tests/mmdViewer/mmdViewer.cpp — MMD 模型查看器（测试程序）：主循环与场景装配
//
// 用法：
//   make mmdViewer
//   LD_LIBRARY_PATH=. ./build/mmdViewer
//   MMD_VIEWER_MODEL=/path/to/model.pmx LD_LIBRARY_PATH=. ./build/mmdViewer
//
// 模块划分（本文件只做主循环与场景装配）：
//   mmdViewerShared.h     — 跨模块共享状态 / 常量 / 函数声明
//   mmdViewerCamera.cpp   — 相机与视角控制
//   mmdViewerWeather.cpp  — 天气 / 云 / 雾 / 降水接线（共享常量唯一定义）
//   mmdViewerDecal.cpp    — 地面 / 贴花 / 坐标轴与模型 AABB 线框
//   mmdViewerOverlay.cpp  — 坐标信息叠层 / 粒子调试包围盒
//
// 边界：
//   * 只显示绑定姿势（不做骨骼动画/蒙皮），卡通/球面贴图未实现；
//   * 贴图支持 .png（引擎 ImageLoadFromMem）与 .tga/.bmp（本地解码），其余格式跳过；
//   * 材质默认 MASK 硬边裁剪；.png 贴图半透明像素 >5% 时该网格改走 BLEND 混合管线
//     （cMODEL::DrawPBRSorted 按网格排序，非逐三角形）；.tga/.bmp 保持 MASK；
//   * 本程序需要 Vulkan 设备与窗口环境；编译不依赖 GPU。
#include "mmdViewerShared.h"

#include "mmd/mmdFile.h"
#include "mmd/mmdMeshBuilder.h"
#include "mmd/mmdTextureDecode.h"
#include "mmd/pmdLoader.h"
#include "mmd/pmxLoader.h"

#include "GN_3Dtexture.h"
#include "vkShadowCascade.h" // L3c：CSM CPU 拟合（级联分割 / 视锥角点 / texel snapping）

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

// L3c：4 级 CSM 的光矩阵与状态。每帧由 updateShadowCascades() 拟合；
// 这些全局量同时被 mmdViewerOverlay.cpp 读取（共享头 mmdViewerShared.h 属其他写域，只读，
// 所以跨模块声明落在各自 .cpp 的使用处）。
GNg::sMAT44 gShadowCascadeVP[GN3d::SHADOW_CASCADE_MAX];
bool gShadowValid = false;
bool gShadowsEnabled = true;
uint32_t gShadowCascadeCount = GN3d::SHADOW_CASCADE_MAX; // 契约 §8：级数默认 4
float gShadowCascadeSplits[GN3d::SHADOW_CASCADE_MAX] = {0.0f, 0.0f, 0.0f, 0.0f};
float gShadowTexelWorld[GN3d::SHADOW_CASCADE_MAX] = {0.0f, 0.0f, 0.0f, 0.0f};
uint32_t gShadowMapSize = 2048;
float gShadowFar = 0.0f;
// 方向光的传播方向（指向场景的单位向量）：光矩阵沿它反推光源位置（与 cLIGHT 方向同向）。
static const GNg::sVEC3 kSunDirection = {-0.35f, -0.85f, 0.4f};
// L4：空气透视开关（定义在 mmdViewerWeather.cpp；B 键切换；叠层显示状态）。
extern bool gAtmosphereEnabled;
// L5b：立方体云开关（定义在 mmdViewerWeather.cpp；X 键切换；绘制也在该 TU）。
extern bool gCloudsEnabled;
void drawCloudCubes(const GNg::sMAT44 &viewProjection, const GNg::sVEC3 &camera);

// ── 场景状态（主循环与场景装配持有）──
GN3d::cMODEL gModel;
GN3d::cLIGHT *gLight = nullptr;
std::vector<GNg::sIMAGE *> gTextures;
std::string gModelPath;
GNg::sVEC3 gTarget = {0.0f, 0.0f, 0.0f};
float gHeight = 1.0f;
float gDelta = 0.016f;
bool gLoaded = false;
// 最近一帧的 viewProj 与 framebuffer 尺寸：叠层用它把样本粒子投影到屏幕，直接看屏幕方向。
GNg::sMAT44 gLastViewProj;
bool gLastViewProjValid = false;
int gFbWidth = 1280;
int gFbHeight = 800;

namespace {

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

// 统计 PNG 贴图里半透明像素（0 < alpha < 255）的占比；> 5% ⇒ 该网格材质改走 BLEND 混合管线。
// 非 .png、读取失败或解码失败都返回 false（调用方保持 MASK 硬边裁剪）。
bool pngHasTranslucentPixels(const std::string &path) {
    if (lowerExtension(path) != ".png") {
        return false;
    }
    std::vector<uint8_t> bytes;
    std::string error;
    if (!mmd::readFileBytes(path, bytes, &error) || bytes.empty()) {
        return false;
    }
    GN3d::sTEXTURE_IMAGE image;
    if (!GN3d::decodePngMemory(bytes.data(), bytes.size(), image)) {
        return false;
    }
    const size_t pixelCount = image.aPixels.size() / 4;
    if (pixelCount == 0) {
        return false;
    }
    size_t translucent = 0;
    for (size_t i = 0; i < pixelCount; ++i) {
        const uint8_t alpha = image.aPixels[i * 4 + 3];
        if (alpha > 0 && alpha < 255) {
            ++translucent;
        }
    }
    return translucent * 100 > pixelCount * 5; // 比例 > 5%
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
    std::map<std::string, bool> translucentCache; // PNG 半透明判定按路径缓存（同一贴图可能被多材质共享）
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
            GN3d::sMESH &mesh = gModel.aMeshes[bindings[i].meshIndex];
            mesh.aAlbedoTex = image;
            // 仅 PNG 且半透明像素 >5% 时升为 BLEND；其它扩展名/解码失败保持构建期给的 MASK。
            std::map<std::string, bool>::iterator cached = translucentCache.find(fullPath);
            bool translucent = false;
            if (cached != translucentCache.end()) {
                translucent = cached->second;
            } else {
                translucent = pngHasTranslucentPixels(fullPath);
                translucentCache[fullPath] = translucent;
            }
            if (mesh.aMaterial && translucent) {
                mesh.aMaterial->aAlphaMode = GN3d::eALPHA_BLEND;
            }
        }
    }

    gLight = new GN3d::cLIGHT();
    GNg::sVEC3 ambient = {0.35f, 0.35f, 0.38f};
    gLight->SetAmbient(ambient, 1.0f);
    GNg::sVEC3 sunDir = kSunDirection;
    GNg::sVEC3 sunColor = {1.0f, 0.97f, 0.92f};
    gLight->AddDirectional(sunDir, sunColor, 1.2f, true); // L3：方向光投影（K 键可关）
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
                "方向键/J L 转视角, F 自由/轨道, R 重置, G 坐标轴/包围盒, H 坐标信息, "
                "O 降水, P 贴花, V 粒子盒, C 云层, T FXAA, K 阴影(4 级 CSM), B 空气透视\n");
    std::printf("[mmdViewer] 轨道模式: 左/右键拖动绕模型旋转, 滚轮缩放, 中键拖动平移, F 回自由\n");

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

// L3c：阴影拟合用的「场景 AABB」= 模型世界 AABB + 地面承接区。
// 模块会把每级视锥切片与它求交，若只给模型盒，模型之外的地面片段不在拟合盒内，
// 地面上就看不到模型投影。地面承接区取模型盒四周 1.5×模型高、下沿压到地面。
static GN3d::sAABB shadowSceneAABB() {
    GN3d::sAABB box = gModel.aWorldAABB;
    const float margin = gHeight > 0.0f ? gHeight * 1.5f : 1.0f;
    box.aMin.x -= margin;
    box.aMax.x += margin;
    box.aMin.z -= margin;
    box.aMax.z += margin;
    const float ground = groundHeight();
    if (ground < box.aMin.y) {
        box.aMin.y = ground;
    }
    return box;
}

// L3c：每帧按相机视锥 + 场景 AABB 拟合 4 级 CSM（分割/角点/正交拟合/texel snapping 全在
// vkShadowCascade.cpp）。结果同时写进 sLIGHT_UBO（着色器选级用）与本文件的 shadow pass 循环。
// far 口径（契约 §8，Lead 已放宽）：far = max(AABB 对角, 相机到 AABB 最远角距离 + 0.5)。
// 只取对角线时，相机站在场景外会让 4 级切片全落在相机与模型之间 ⇒ 阴影整片消失。
static void updateShadowCascades(const GNg::sMAT44 &cameraView, float aspect) {
    gShadowValid = false;
    if (!gLight || !gModel.aWorldAABB.IsValid() || !(aspect > 0.0f)) {
        return;
    }
    const GNg::sVEC3 extent = gModel.aWorldAABB.Extent();
    const float diagonal =
        2.0f * std::sqrt(extent.x * extent.x + extent.y * extent.y + extent.z * extent.z);
    const GNg::sVEC3 camera = cameraPosition();
    const GNg::sVEC3 &lo = gModel.aWorldAABB.aMin;
    const GNg::sVEC3 &hi = gModel.aWorldAABB.aMax;
    float farthestCorner = 0.0f;
    for (uint32_t corner = 0; corner < 8; ++corner) {
        const GNg::sVEC3 point = {(corner & 1u) ? hi.x : lo.x, (corner & 2u) ? hi.y : lo.y,
                                  (corner & 4u) ? hi.z : lo.z};
        const float dx = point.x - camera.x;
        const float dy = point.y - camera.y;
        const float dz = point.z - camera.z;
        const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (distance > farthestCorner) {
            farthestCorner = distance;
        }
    }
    float farZ = diagonal > farthestCorner + 0.5f ? diagonal : farthestCorner + 0.5f;
    if (!(farZ > 1.0f)) {
        farZ = 1.0f; // 极小模型兜底：远处云层/地面不参与阴影拟合
    }

    GN3d::sSHADOW_CASCADE_DESC desc;
    desc.aCameraView = cameraView;
    desc.aFovYRad = gCamera.aFovY * GN3d::CAMERA_DEG_TO_RAD;
    desc.aAspect = aspect;
    desc.aNear = 0.1f; // 契约 §8 固定近平面
    desc.aFar = farZ;
    desc.aLightDirection = kSunDirection;
    desc.aSceneAABB = shadowSceneAABB();
    // 级数不得超过引擎实际分配的阴影层数（Lead 接线：4 层 D32 数组，GN_VkShadowCascadeCount）。
    uint32_t cascadeCount = gShadowCascadeCount;
    const uint32_t layerCount = GNg::GN_VkShadowCascadeCount();
    if (cascadeCount > layerCount) {
        cascadeCount = layerCount;
    }
    if (cascadeCount < 1u) {
        cascadeCount = 1u;
    }
    desc.aCascadeCount = cascadeCount;
    desc.aShadowMapSize = gShadowMapSize;
    desc.aSplitLambda = 0.5f; // 契约 §8：对数/均匀各半（Lauritzen 固定分割）
    desc.aTexelSnapping = true;

    GN3d::sSHADOW_CASCADE_RESULT result;
    if (!GN3d::buildShadowCascades(desc, &result)) {
        return;
    }
    gShadowCascadeCount = result.aCount;
    gShadowFar = farZ;
    for (uint32_t i = 0; i < GN3d::SHADOW_CASCADE_MAX; ++i) {
        gShadowCascadeVP[i] = result.aLightViewProj[i];
        gShadowCascadeSplits[i] = result.aSplits[i];
        gShadowTexelWorld[i] = result.aTexelWorld[i];
    }
    // sLIGHT_UBO：每级 lightVP + 级数/分割/texel 世界尺寸（法线偏移用）。
    for (uint32_t i = 0; i < result.aCount; ++i) {
        gLight->SetShadowLightVP(result.aLightViewProj[i], i);
    }
    gLight->SetShadowMapSize(static_cast<float>(result.aShadowMapSize));
    gLight->SetShadowCascades(result.aCount, result.aSplits, result.aTexelWorld);
    gShadowValid = true;
}

} // namespace

extern "C" {

GN_CALL void AppInit() {
    gLoaded = loadModel();
    // 用户裁决 A：查看器走前向 PBR + GI IBL（引擎初始化已完成、GI UBO 已由 init3DEnvironment 建好）；
    // 不启用延迟专用的 SSAO/SSGI/探针（查看器无 G-Buffer）。
    GNg::GN_VkEnableGI(true);
    // L3c：阴影矩阵不在这里算——相机每帧移动，updateShadowCascades() 在 AppDisplay2D 里
    // 用本帧的 view 矩阵按 4 级 CSM 拟合。
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
    const float nearZ = clampValue(gHeight * 0.01f, 0.05f, 1.0f);
    // 远平面至少要容下立方体云块的分布范围（L5b：半径 900 m），否则小模型时云块被整片裁掉。
    float farZ = cameraDistance() * 8.0f + gHeight * 4.0f;
    if(farZ < CLOUD_FAR_PLANE) {
        farZ = CLOUD_FAR_PLANE;
    }
    gCamera.SetNF(nearZ, farZ); // near/far 仍由查看器按模型高度/云层下限给，相机只负责组矩阵
    if(!gCamera.BuildViewProj(aspect, &view, &projection, &viewProjection)) {
        return; // aspect 非法时本帧不画（宽度/高度已校验 > 0，正常不会走到这里）
    }
    gLastViewProj = viewProjection;
    gLastViewProjValid = true;

    // L3c：按本帧相机拟合 4 级 CSM（写 sLIGHT_UBO + shadow pass 用的光矩阵）。
    updateShadowCascades(view, aspect);

    // L3c：方向光阴影（在 3D pass 之前；只投 OPAQUE 网格，K 键切换）。
    // 每级一次 Begin/End（cascadeIndex 选择 0..3 的深度层），逐级绘制同一批投影网格。
    if (gShadowsEnabled && gShadowValid) {
        for (uint32_t i = 0; i < gShadowCascadeCount; ++i) {
            GNg::GN_VkShadowPassBegin(&gShadowCascadeVP[i], i);
            gModel.DrawShadow();
            GNg::GN_VkShadowPassEnd();
        }
    }

    GNg::GN_VkBegin3DPass(&viewProjection, &camera, gLight->GetUBO(), sizeof(GN3d::sLIGHT_UBO));
    GNg::GN_VkDrawSkybox(&viewProjection);
    // L5b：立方体云（独立云块）——天空盒之后、几何之前；前景几何随后自然覆盖云。
    drawCloudCubes(viewProjection, camera);
    drawGround(); // 不透明地面：先画，供模型与贴花做深度/投影基准
    gModel.DrawPBRSorted(camera); // 先 OPAQUE/MASK，再 BLEND 由远到近（PNG 半透明材质）
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
    // 旧 S2b 中景 billboard 云层键（C）已停用：云由 L5 体积云接管。
    if (GN::Key1[GK_V]) { // 开关粒子四边形包围盒（只画最近 16 个）
        gParticleBoxes = !gParticleBoxes;
    }
    if (GN::Key1[GK_R]) { // 回到默认正面视角
        resetCamera();
    }
    // L0c 亮度/Bloom 标定：9/0 调曝光 EV（±0.25），7/8 调 Bloom 强度（±0.05）；数值显示在左上叠层。
    if (GN::Key1[GK_9] || GN::Key1[GK_0]) {
        GN3d::sPOST_UBO post;
        if (GNg::GN_VkGetPost(&post)) {
            GNg::GN_VkSetExposure(post.ev + (GN::Key1[GK_9] ? -0.25f : 0.25f));
        }
    }
    if (GN::Key1[GK_7] || GN::Key1[GK_8]) {
        GN3d::sPOST_UBO post;
        if (GNg::GN_VkGetPost(&post)) {
            const float step = GN::Key1[GK_7] ? -0.05f : 0.05f;
            float intensity = post.bloomIntensity + step;
            if (intensity < 0.0f) intensity = 0.0f;
            if (intensity > 1.0f) intensity = 1.0f;
            post.bloomIntensity = intensity;
            GNg::GN_VkSetPost(post);
        }
    }
    if (GN::Key1[GK_T]) { // L2a：Toggle FXAA（前向已有 4× MSAA，默认关）
        GN3d::sPOST_UBO post;
        if (GNg::GN_VkGetPost(&post)) {
            GNg::GN_VkSetFxaa(post.fxaa < 0.5f);
        }
    }
    if (GN::Key1[GK_K]) { // L3c：Toggle 方向光阴影（同步改 light UBO 的 shadowType）
        gShadowsEnabled = !gShadowsEnabled;
        if (gLight && gLight->aCount > 0) {
            gLight->aData.lights[0].direction[3] = gShadowsEnabled ? 1.0f : 0.0f;
            gLight->aDirty = true;
        }
    }
    if (GN::Key1[GK_B]) { // L4：Toggle 空气透视（sFOG_UBO.aAtmosphere2.w；关 = 旧指数雾 fallback）
        gAtmosphereEnabled = !gAtmosphereEnabled;
    }
    if (GN::Key1[GK_X]) { // L5b：Toggle 立方体云（默认开；开启时天空 FBM 云让位）
        gCloudsEnabled = !gCloudsEnabled;
    }

    // 自由飞行平移 + 键盘/鼠标转视角（顺序：平移 → 键盘转视角 → 鼠标拖拽）。
    updateCameraInput(gDelta);
    updatePrecipitation(gDelta);
    updateDecals(gDelta);
}

} // extern "C"

GN_MAIN;
GN_InitLoadCallBackFunc(AppInit, AppUnInit);
GN_InitLoopCallBackFunc(AppDisplay2D, AppDisplay3D, AppRun);
GN_StartRun("MMD Viewer (PMD/PMX)", 1280, 800, 0, false, true, true, 0, 0, 0, 0);
GN_EXIT;
