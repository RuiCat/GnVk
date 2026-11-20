// tests/imoScene/imoSceneMain.cpp — imo_chan 单角色场景：入口、光照、地面与主循环。
//
// 职责：引擎回调装配（加载 glTF 场景模型 → 上传 GPU → 每帧 3D pass 绘制），
//       相机的输入/取景在 imoSceneCamera，转换与上传在 imoSceneConvert / imoSceneUpload。
// 用法：make imoScene && LD_LIBRARY_PATH=. ./build/imoScene
//       IMO_SCENE_MODEL=/path/to/model.glb 可覆盖默认模型路径。
// 边界：只显示导入姿势（蒙皮已 CPU 烘焙），不做动画播放、阴影与后处理参数调节。

#include "imoSceneCamera.h"
#include "imoSceneRoom.h"
#include "imoSceneUpload.h"

#include "GN_3Dlight.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

// 默认模型：make imoSceneAsset 生成到本机状态目录（.local/ 不随仓库分发）。
// 游戏里穿好衣服的角色在 Clothes_imo_chan 根下；imo_chan 根带的是 kemo 裸体变体（inactive）。
const char *DEFAULT_MODEL_PATH = ".local/imoScene/clothes_imo_chan.glb";
// 游戏数据里这些对象是 inactive 的变体（kemo 裸体、Jirai 镜片/饰针），默认不画。
const char *DEFAULT_HIDE_FILTER = "Kemo_Body,Jirai";
// 地面尺寸（米）与高度：模型已重定心到脚底 y=0。
const float GROUND_HALF_SIZE = 30.0f;
const float GROUND_Y = -0.002f;
// 主循环 dt 上限：窗口拖拽/断点会给出异常大的 nTime 差值。
const float MAX_FRAME_SECONDS = 0.1f;

imoScene::sSceneModel gScene;   // 角色
imoScene::sSceneModel gRoom;    // 房间背景（游戏场景静态网格，可选）
GN3d::cLIGHT *gLight = 0;
bool gLoaded = false;
bool gRoomLoaded = false;
unsigned int gLastTick = 0;

// 一块 60×60 m 的地面（PBR 管线；法线 +Y、UV 铺满一块）。
void drawGround() {
    const float half = GROUND_HALF_SIZE;
    const float xs[4] = {-half, half, half, -half};
    const float zs[4] = {-half, -half, half, half};
    float vertices[4 * 11];
    std::memset(vertices, 0, sizeof(vertices));
    for (int i = 0; i < 4; ++i) {
        float *v = vertices + i * 11;
        v[0] = xs[i];
        v[1] = GROUND_Y;
        v[2] = zs[i];
        v[3] = 0.0f;
        v[4] = 1.0f;
        v[5] = 0.0f;
        v[6] = 1.0f; // 切线 +X（布局：pos / normal / tangent / uv）
        v[9] = (xs[i] + half) / (2.0f * half);
        v[10] = (zs[i] + half) / (2.0f * half);
    }
    const uint32_t indices[6] = {0, 2, 1, 0, 3, 2};
    GNg::sMAT44 identity;
    GNg::Mat44Identity(&identity);
    GNg::GN_VkDraw3DPBR(vertices, 4, indices, 6, &identity, 0.45f, 0.46f, 0.42f, 0.95f, 0.0f,
                        1.0f);
}

// 太阳 + 冷色补光；不启用阴影（本场景只验证几何/材质/相机链路）。
void initLights() {
    gLight = new GN3d::cLIGHT();
    if (std::getenv("IMO_SCENE_ALBEDO") != 0) {
        // 诊断：关 GI、去掉方向光，只用白色环境光 ×1 ⇒ 画面上是“贴图原色”，用来区分
        // 「纹理没绑上/曝光过亮」和「alpha 镂空」两类问题。
        const GNg::sVEC3 white = {1.0f, 1.0f, 1.0f};
        gLight->SetAmbient(white, 1.0f);
        return;
    }
    // 房间模式走室内布光：无阴影的方向光只会把整个房间照成一片平，故不加太阳；
    // 环境亮度由 applyRoomAmbient() 的 SH9 统一给（见 AppInit），这里只放暖色吸顶灯。
    if (gRoomLoaded) {
        const GNg::sVEC3 indoorAmbient = {0.30f, 0.28f, 0.25f};
        gLight->SetAmbient(indoorAmbient, 1.0f);
        const char *lampEnv = std::getenv("IMO_SCENE_LAMPS");
        const float lampScale = (lampEnv != 0) ? static_cast<float>(std::atof(lampEnv)) : 1.0f;
        const GNg::sVEC3 lampColor = {1.0f, 0.90f, 0.78f};
        const GNg::sVEC3 lampAboveChar = {0.0f, 2.3f, 0.3f};
        const GNg::sVEC3 lampAboveKitchen = {2.5f, 2.3f, -3.2f};
        const GNg::sVEC3 lampEntrance = {4.6f, 2.2f, 0.4f};
        gLight->AddPoint(lampAboveChar, 8.0f, lampColor, 2.6f * lampScale);
        gLight->AddPoint(lampAboveKitchen, 8.0f, lampColor, 1.8f * lampScale);
        gLight->AddPoint(lampEntrance, 7.0f, lampColor, 1.1f * lampScale);
        return;
    }
    // 真机截图偏白：环境光与太阳都收敛到“白衣服仍能分辨褶皱”的量级，不再加点光补亮。
    const GNg::sVEC3 ambient = {0.30f, 0.32f, 0.36f};
    gLight->SetAmbient(ambient, 1.0f);
    const GNg::sVEC3 sunDirection = {-0.35f, -0.85f, 0.4f};
    const GNg::sVEC3 sunColor = {1.0f, 0.97f, 0.92f};
    gLight->AddDirectional(sunDirection, sunColor, 0.8f, false);
}

// 环境变量 → 转换选项（IMO_SCENE_SHOW / IMO_SCENE_HIDE，逗号分隔的子串）。
imoScene::sConvertOptions convertOptionsFromEnv() {
    imoScene::sConvertOptions options;
    const char *showFilter = std::getenv("IMO_SCENE_SHOW");
    if (showFilter != 0) {
        options.aShowFilter = showFilter;
    }
    const char *hideFilter = std::getenv("IMO_SCENE_HIDE");
    options.aHideFilter = (hideFilter != 0) ? hideFilter : DEFAULT_HIDE_FILTER;
    options.aForceOpaque = std::getenv("IMO_SCENE_FORCE_OPAQUE") != 0;
    return options;
}

// IMO_SCENE_LIST=1 时打印网格→材质清单，便于用 IMO_SCENE_HIDE/SHOW 选择服装。
void printMeshListIfRequested(const imoScene::sModelData &data) {
    if (std::getenv("IMO_SCENE_LIST") == 0) {
        return;
    }
    for (size_t index = 0; index < data.aMeshes.size(); ++index) {
        const imoScene::sMeshData &mesh = data.aMeshes[index];
        const char *material = (mesh.aMaterial >= 0 &&
                                static_cast<size_t>(mesh.aMaterial) < data.aMaterials.size())
                                   ? data.aMaterials[static_cast<size_t>(mesh.aMaterial)].aName.c_str()
                                   : "-";
        std::printf("[imoScene] mesh %2zu %-34s mat=%s\n", index, mesh.aName.c_str(), material);
    }
}

// 只读自检：IMO_SCENE_INSPECT=1 时只走 CPU 转换并退出，不创建窗口、不初始化 Vulkan。
// 返回 -1 = 未请求自检；>=0 = 自检退出码（0 成功 / 2 失败）。
int runInspectIfRequested() {
    const char *mode = std::getenv("IMO_SCENE_INSPECT");
    if (mode == 0 || mode[0] == '\0' || mode[0] == '0') {
        return -1;
    }
    const bool roomMode = std::strncmp(mode, "room", 4) == 0;
    const char *fromEnv = std::getenv(roomMode ? "IMO_SCENE_ROOM" : "IMO_SCENE_MODEL");
    // 默认房间路径是临时 std::string 的成员，先落到本地再取指针，避免悬垂。
    const std::string roomDefault = roomMode ? imoScene::roomOptionsFromEnv().aPath : std::string();
    const char *path = DEFAULT_MODEL_PATH;
    if (fromEnv != 0 && fromEnv[0] != '\0') {
        path = fromEnv;
    } else if (roomMode) {
        path = roomDefault.c_str();
    }
    std::printf("[imoScene] inspect %s: %s\n", roomMode ? "房间" : "模型", path);
    imoScene::sModelData data;
    std::string error;
    const imoScene::sConvertOptions options =
        roomMode ? imoScene::roomConvertOptions() : convertOptionsFromEnv();
    if (!imoScene::buildModelFromGltf(path, data, error, options)) {
        std::fprintf(stderr, "[imoScene] inspect 失败: %s\n", error.c_str());
        return 2;
    }
    std::printf("[imoScene] inspect meshes=%zu verts=%u tris=%u textures=%zu materials=%zu\n",
                data.aMeshes.size(), data.aVertexCount, data.aTriangleCount, data.aTextures.size(),
                data.aMaterials.size());
    std::printf("[imoScene] inspect height=%.3f zeroWeight=%u skipped=%u filtered=%u\n",
                data.aMax.y - data.aMin.y, data.aZeroWeightVertices, data.aSkippedPrimitives,
                data.aFilteredMeshes);
    std::printf("[imoScene] inspect AABB x=[%.2f, %.2f] y=[%.2f, %.2f] z=[%.2f, %.2f]\n",
                data.aMin.x, data.aMax.x, data.aMin.y, data.aMax.y, data.aMin.z, data.aMax.z);
    // 场景 alpha 全靠重判：打印三种模式的实际分布，验证玻璃/软垫没有被误判成 OPAQUE。
    uint32_t opaque = 0;
    uint32_t mask = 0;
    uint32_t blend = 0;
    for (size_t index = 0; index < data.aMaterials.size(); ++index) {
        const GN3d::eALPHA_MODE alphaMode = data.aMaterials[index].aAlphaMode;
        if (alphaMode == GN3d::eALPHA_MASK) {
            ++mask;
        } else if (alphaMode == GN3d::eALPHA_BLEND) {
            ++blend;
        } else {
            ++opaque;
        }
    }
    std::printf("[imoScene] inspect alpha OPAQUE=%u MASK=%u BLEND=%u\n", opaque, mask, blend);
    printMeshListIfRequested(data);
    return 0;
}

// 加载 → 上传 → 取景。失败只报错不抛异常，主循环保持空白而非崩溃。
bool loadScene() {
    const char *fromEnv = std::getenv("IMO_SCENE_MODEL");
    const char *path = (fromEnv != 0 && fromEnv[0] != '\0') ? fromEnv : DEFAULT_MODEL_PATH;
    std::printf("[imoScene] 加载模型: %s\n", path);
    std::fprintf(stderr, "[imoScene] AppInit: 开始读取/转换 glTF\n");

    imoScene::sModelData data;
    std::string error;
    if (!imoScene::buildModelFromGltf(path, data, error, convertOptionsFromEnv())) {
        std::fprintf(stderr, "[imoScene] 加载失败: %s\n", error.c_str());
        std::fprintf(stderr, "[imoScene] 先生成资产: make imoSceneAsset（需要 Blender）\n");
        return false;
    }
    std::fprintf(stderr, "[imoScene] AppInit: glTF 转换完成（%zu 网格），开始 GPU 上传\n",
                 data.aMeshes.size());
    imoScene::sUploadOptions options;
    const char *noTextures = std::getenv("IMO_SCENE_NO_TEXTURES");
    options.aLoadTextures = !(noTextures != 0 && noTextures[0] == '1');
    const char *maxMeshes = std::getenv("IMO_SCENE_MAX_MESHES");
    if (maxMeshes != 0) {
        options.aMaxMeshes = static_cast<uint32_t>(std::atoi(maxMeshes));
    }
    std::fprintf(stderr, "[imoScene] AppInit: 上传选项 贴图=%s 网格上限=%u\n",
                 options.aLoadTextures ? "开" : "关", options.aMaxMeshes);
    if (!imoScene::uploadSceneModel(data, gScene, error, options)) {
        std::fprintf(stderr, "[imoScene] 上传失败: %s\n", error.c_str());
        return false;
    }
    std::fprintf(stderr, "[imoScene] AppInit: GPU 上传完成（%u 网格），准备首帧\n",
                 gScene.aModel.aMeshCount);
    const float height = data.aMax.y - data.aMin.y;
    std::printf("[imoScene] 网格 %zu 个，顶点 %u，三角形 %u，贴图 %zu 张，材质 %zu 个\n",
                data.aMeshes.size(), data.aVertexCount, data.aTriangleCount, data.aTextures.size(),
                data.aMaterials.size());
    std::printf("[imoScene] AABB y=[%.3f, %.3f] 高度 %.3f m；零权重顶点 %u，跳过 primitive %u，"
                "过滤网格 %u\n",
                data.aMin.y, data.aMax.y, height, data.aZeroWeightVertices,
                data.aSkippedPrimitives, data.aFilteredMeshes);
    printMeshListIfRequested(data);
    for (size_t index = 0; index < data.aMaterials.size(); ++index) {
        const imoScene::sMaterialData &material = data.aMaterials[index];
        std::printf("[imoScene] 材质 %2zu %-40s alpha=%s tex=%d\n", index,
                    material.aName.c_str(), imoScene::alphaModeName(material.aAlphaMode),
                    material.aAlbedoTexture);
    }
    const GNg::sVEC3 focus = {0.0f, height * 0.55f, 0.0f};
    imoScene::cameraInit(focus, height);

    // 房间是可选的第二份模型：缺资产/加载失败只告警，仍然显示角色（不整体失败）。
    const imoScene::sRoomOptions roomOptions = imoScene::roomOptionsFromEnv();
    if (roomOptions.aEnabled) {
        std::fprintf(stderr, "[imoScene] AppInit: 加载房间 %s\n", roomOptions.aPath.c_str());
        if (imoScene::loadRoomModel(roomOptions, gRoom, error)) {
            gRoomLoaded = true;
            std::printf("[imoScene] 房间: %u 网格，%zu 贴图，AABB y=[%.2f, %.2f]\n",
                        gRoom.aModel.aMeshCount, gRoom.aTextures.size(), gRoom.aMin.y,
                        gRoom.aMax.y);
        } else {
            std::fprintf(stderr, "[imoScene] 房间加载失败（只显示角色）: %s\n", error.c_str());
        }
    }
    std::printf("[imoScene] 操作: 左/右键拖动旋转, 滚轮缩放, 中键平移, R 重置视角, Esc 退出\n");
    return true;
}

} // namespace

extern "C" {

GN_CALL void AppInit() {
    gLoaded = loadScene();
    initLights();
    if (gLoaded) {
        const bool albedoDebug = std::getenv("IMO_SCENE_ALBEDO") != 0;
        if (!albedoDebug) {
            if (gRoomLoaded) {
                // 室内环境光：户外蓝天直接当 IBL 会把房间洗成淡蓝、没有明暗；
                // 换成均匀暖灰 SH9（IMO_SCENE_AMBIENT 可调，默认 0.35）。
                const char *ambientEnv = std::getenv("IMO_SCENE_AMBIENT");
                const float ambient =
                    (ambientEnv != 0) ? static_cast<float>(std::atof(ambientEnv)) : 0.35f;
                imoScene::applyRoomAmbient(ambient);
            } else {
                // 角色模式：保留原先的 GI/IBL 取景。
                GNg::GN_VkEnableGI(true);
            }
        } else {
            GNg::GN_VkEnableGI(false);
        }
        // 曝光：真机发白说明合成阶段整体偏亮，默认收 -0.5 EV；室内再收一点避免死白。
        const char *evEnv = std::getenv("IMO_SCENE_EV");
        const float ev = (evEnv != 0) ? static_cast<float>(std::atof(evEnv))
                                      : (gRoomLoaded ? -0.40f : -0.5f);
        GNg::GN_VkSetExposure(ev);
    }
}

GN_CALL void AppUnInit() {
    gRoom.release();
    gScene.release();
    delete gLight;
    gLight = 0;
    gLoaded = false;
    gRoomLoaded = false;
}

GN_CALL void AppDisplay2D(unsigned int nTime) {
    (void)nTime;
    if (!gLoaded || gLight == 0 || ::GT_MainWindow == 0) {
        return;
    }
    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(::GT_MainWindow, &width, &height);
    if (width <= 0 || height <= 0) {
        return;
    }
    static bool firstFrame = true;
    if (firstFrame) {
        std::fprintf(stderr, "[imoScene] 首帧: 开始 3D pass（%dx%d）\n", width, height);
    }
    GNg::sMAT44 viewProjection;
    if (!imoScene::cameraBuildViewProj(static_cast<float>(width) / static_cast<float>(height),
                                       viewProjection)) {
        return;
    }
    const GNg::sVEC3 eye = imoScene::cameraPosition();
    GNg::GN_VkBegin3DPass(&viewProjection, &eye, gLight->GetUBO(), sizeof(GN3d::sLIGHT_UBO));
    GNg::GN_VkDrawSkybox(&viewProjection);
    // 房间自带地板与家具：再画一块 60×60 的虚拟地面会与木地板 z-fighting 并盖住房间。
    if (!gRoomLoaded) {
        drawGround();
    }
    if (gRoomLoaded) {
        gRoom.aModel.DrawPBRSorted(eye); // 房间先画（静态背景），角色再叠上去
    }
    gScene.aModel.DrawPBRSorted(eye); // OPAQUE/MASK 先画，BLEND 按网格距离由远到近
    GNg::GN_VkEnd3DPass();
    GNg::GN_VkBegin2DPass(); // 交还给 2D 叠层通道（保持 LOAD，不清屏）
    if (firstFrame) {
        std::fprintf(stderr, "[imoScene] 首帧: 录制完成（此后卡住 = present/swapchain）\n");
        firstFrame = false;
    }
}

GN_CALL void AppDisplay3D(unsigned int nTime) {
    (void)nTime;
}

GN_CALL void AppRun(unsigned int nTime) {
    float delta = static_cast<float>(nTime - gLastTick) / 1000.0f;
    gLastTick = nTime;
    if (!(delta > 0.0f) || delta > MAX_FRAME_SECONDS) {
        delta = 1.0f / 60.0f;
    }
    imoScene::cameraUpdate(delta);
}

} // extern "C"

GN_MAIN;
std::setvbuf(stdout, 0, _IONBF, 0); // 卡死/崩溃时保住已打印的进度行
const int inspectCode = runInspectIfRequested();
if (inspectCode >= 0) {
    std::exit(inspectCode);
}
GN_InitLoadCallBackFunc(AppInit, AppUnInit);
GN_InitLoopCallBackFunc(AppDisplay2D, AppDisplay3D, AppRun);
GN_StartRun("imo_chan Scene (glTF)", 1280, 800, 0, false, true, true, 0, 0, 0, 0);
GN_EXIT;
