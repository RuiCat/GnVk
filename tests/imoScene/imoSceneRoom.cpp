// tests/imoScene/imoSceneRoom.cpp — 房间背景的加载：环境变量 → glTF 转换 → GPU 上传。
//
// 职责：把游戏场景的静态网格（room.glb）装进一个独立的 sSceneModel。
// 坐标约定：房间顶点已平移到“角色根为原点”的游戏世界坐标，因此不重定心
//           （aRecenter=false），否则会被按自身 AABB 挪走、与角色错位。
// alpha 约定：Blender 导出的场景材质全是 OPAQUE，玻璃/软垫的透明只能按贴图 alpha
//           统计重判（aClassifyAlpha=true），沿用角色那套 OPAQUE/MASK/BLEND 规则。

#include "imoSceneRoom.h"

#include <cmath>
#include <cstdlib>

#include "giParams.h"

namespace imoScene {

namespace {

// 默认房间资产：make imoRoomAsset 从游戏数据导出到本机状态目录。
const char *DEFAULT_ROOM_PATH = ".local/imoScene/room.glb";

} // namespace

sRoomOptions::sRoomOptions() : aEnabled(true), aPath(DEFAULT_ROOM_PATH) {}

sRoomOptions roomOptionsFromEnv() {
    sRoomOptions options;
    const char *noRoom = std::getenv("IMO_SCENE_NO_ROOM");
    if (noRoom != 0 && noRoom[0] == '1') {
        options.aEnabled = false;
    }
    const char *fromEnv = std::getenv("IMO_SCENE_ROOM");
    if (fromEnv != 0 && fromEnv[0] != '\0') {
        options.aPath = fromEnv;
    }
    return options;
}

sConvertOptions roomConvertOptions() {
    sConvertOptions options;
    options.aRecenter = false;
    options.aClassifyAlpha = true;
    options.aForceOpaque = std::getenv("IMO_SCENE_FORCE_OPAQUE") != 0;
    return options;
}

bool loadRoomModel(const sRoomOptions &options, sSceneModel &out, std::string &error) {
    if (!options.aEnabled) {
        error = "房间已关闭（IMO_SCENE_NO_ROOM=1）";
        return false;
    }
    sModelData data;
    if (!buildModelFromGltf(options.aPath.c_str(), data, error, roomConvertOptions())) {
        return false;
    }
    sUploadOptions uploadOptions;
    const char *noTextures = std::getenv("IMO_SCENE_NO_TEXTURES");
    uploadOptions.aLoadTextures = !(noTextures != 0 && noTextures[0] == '1');
    if (!uploadSceneModel(data, out, error, uploadOptions)) {
        return false;
    }
    return true;
}

void applyRoomAmbient(float aAmbient) {
    if (!(aAmbient > 0.0f)) {
        aAmbient = 0.0f;
    }
    GN3d::sGI_UBO ubo = GN3d::makeDefaultGiUbo();
    // 均匀环境辐亮度 L 的 SH00 = L·√(4π)；其余系数清零，避免默认 UBO 的
    // sh[1]/sh[2] 把方向性（略带俯仰的偏色）带进来。
    const float constant = aAmbient * std::sqrt(4.0f * 3.14159265358979f);
    ubo.sh[0] = constant * 1.00f; // 暖灰：R 稍高、B 稍低
    ubo.sh[1] = constant * 0.97f;
    ubo.sh[2] = constant * 0.92f;
    ubo.sh[3] = 0.0f;
    for (int index = 4; index < GN3d::SH9_FLOAT_COUNT; ++index) {
        ubo.sh[index] = 0.0f;
    }
    ubo.indirect[0] = 1.0f;  // 漫反射：用上面的 SH9（室内均匀环境）
    ubo.indirect[1] = 0.25f; // 镜面：天空压到 1/4，避免蓝色反光/高光洗白
    ubo.indirect[2] = 1.0f;  // AO 强度
    ubo.indirect[3] = 0.0f;  // SSAO 关
    GNg::GN_VkSetGI(ubo);
}

} // namespace imoScene
