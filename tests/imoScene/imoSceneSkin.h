#pragma once
#ifndef GN_IMOSCENE_SKIN_H
#define GN_IMOSCENE_SKIN_H
// tests/imoScene/imoSceneSkin.h — GNAC 剪辑加载 + 每帧调色板 + CPU 重蒙皮（不碰 Vulkan）。
//
// 边界：
//   * 只读 .gnac（由 .local/anim/exportClips.py 导出的 30fps 重定向结果）与 sSkinRig；
//   * 输出仍是 VkVertex3D 布局（11 floats/顶点），交给既有 DrawPBR 逐帧上传，不改材质/索引；
//   * 每帧全量重算（约 10 万顶点），不做增量缓存。
#include "imoSceneConvert.h"

#include <string>
#include <vector>

namespace imoScene {

// 一段动画：按关节存每帧局部旋转；没有通道的关节保持绑定旋转。
struct sSkinClip {
    uint32_t aFrameCount;
    float aFps;
    std::vector<std::vector<GNg::sQUAT>> aRot; // [关节][帧]

    sSkinClip();
};

// 读 GNAC 文件并把节点通道映射到关节下标；失败时 out 清空、error 写明原因。
bool loadSkinClip(const char *path, const sSkinRig &rig, sSkinClip &out, std::string &error);

// 采样 time（秒）处的局部旋转并算调色板（palette = 关节世界矩阵 × 逆绑定矩阵）。
bool buildSkinPalette(const sSkinRig &rig, const sSkinClip &clip, float time,
                      std::vector<GNg::sMAT44> &outPalette);

// 用调色板把绑定顶点重算成 VkVertex3D（pos/normal/tangent/uv）；offset = 重定心平移。
void skinBindVertices(const std::vector<sBindVertex> &bind, const std::vector<GNg::sMAT44> &palette,
                      const GNg::sVEC3 &offset, float *outVertices);

} // namespace imoScene
#endif // GN_IMOSCENE_SKIN_H
