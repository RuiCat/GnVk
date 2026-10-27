#pragma once
#ifndef _GN_3DASSET_H_
#define _GN_3DASSET_H_
// ╔══════════════════════════════════════════════════════════╗
// ║  GN_mini — glTF 2.0 资产导入（节点/网格/皮肤/动画，CPU）  ║
// ╚══════════════════════════════════════════════════════════╝
//
// 契约（P2 v1）：
//   * 支持 .gltf/.glb；外部 .bin 仅在 loadFromFile 时按相对路径加载，loadFromMemory 只支持内嵌/data URI。
//   * 节点：名称、父索引（-1=根）、局部 TRS、可选的 mesh/skin 索引（-1=无）。
//   * 网格：每个 primitive 输出顶点（位置/法线/UV/4 关节+4 权重）与 uint32 索引；无索引时顺序生成。
//   * 皮肤：关节=节点索引数组，逆绑定矩阵与关节等长（缺失时为单位矩阵）。
//   * 动画：仅 translation/rotation/scale 通道，STEP/LINEAR 映射到 `sANIM_CLIP`；
//     CUBICSPLINE 通道与 morph 权重通道跳过（P2 v1 边界）。
//   * 材质/纹理/相机/灯光不导入（渲染侧仍用 sMATERIAL 默认值）。
#include "GN_3Dmath.h"
#include "GN_3Danim.h"

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

namespace GN3d {

struct sASSET_VERTEX {
    GNg::sVEC3 aPosition;
    GNg::sVEC3 aNormal;
    float aUv[2];
    uint16_t aJoints[4]; // 关节局部索引（skin.joints 数组下标）
    float aWeights[4]; // 调用方负责归一化
};

struct sASSET_PRIMITIVE {
    std::vector<sASSET_VERTEX> aVertices;
    std::vector<uint32_t> aIndices;
};

struct sASSET_MESH {
    std::vector<sASSET_PRIMITIVE> aPrimitives;
};

struct sASSET_NODE {
    std::string aName;
    int32_t aParent; // -1 = 根
    GNg::sVEC3 aTranslation;
    GNg::sQUAT aRotation;
    GNg::sVEC3 aScale;
    int32_t aMesh; // -1 = 无
    int32_t aSkin; // -1 = 无
};

struct sASSET_SKIN {
    std::vector<uint32_t> aJoints; // 节点索引
    std::vector<GNg::sMAT44> aInverseBinds; // 与 aJoints 等长
};

struct sASSET_ANIMATION {
    std::string aName;
    sANIM_CLIP aClip;
};

struct sASSET {
    std::vector<sASSET_MESH> aMeshes;
    std::vector<sASSET_NODE> aNodes;
    std::vector<sASSET_SKIN> aSkins;
    std::vector<sASSET_ANIMATION> aAnimations;

    bool loadFromFile(const char *path);
    bool loadFromMemory(const void *data, size_t size);
    void clear();
    bool isLoaded() const { return !aNodes.empty() || !aMeshes.empty(); }
};

} // namespace GN3d
#endif // _GN_3DASSET_H_
