#pragma once
#ifndef GN_IMOSCENE_ROOM_H
#define GN_IMOSCENE_ROOM_H
// tests/imoScene/imoSceneRoom.h — 房间背景（游戏场景静态网格）的配置与加载。
//
// 边界：只负责房间模型的环境变量解析、glTF 转换与 GPU 上传；
//       不碰光照、相机、角色模型，也不改引擎绘制顺序（主循环里房间先画、角色后画）。
#include "imoSceneUpload.h"

#include <string>

namespace imoScene {

// 房间配置：路径来自 IMO_SCENE_ROOM，默认 .local/imoScene/room.glb（make imoRoomAsset 产物）。
struct sRoomOptions {
    bool aEnabled; // IMO_SCENE_NO_ROOM=1 时关闭（只显示角色）
    std::string aPath;

    sRoomOptions();
};

// 从环境变量读房间配置。
sRoomOptions roomOptionsFromEnv();

// 房间模型转换选项：不重定心（世界坐标）+ 按贴图 alpha 重判（glTF 材质全是 OPAQUE）。
sConvertOptions roomConvertOptions();

// 加载房间：坐标不重定心（与角色共用游戏世界原点），alpha 按贴图统计重判。
// 失败时 out 清空并写 error；房间缺失属于可选依赖，由调用方决定是否降级。
bool loadRoomModel(const sRoomOptions &options, sSceneModel &out, std::string &error);

} // namespace imoScene
#endif // GN_IMOSCENE_ROOM_H
