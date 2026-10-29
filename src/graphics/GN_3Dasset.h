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
//   * 材质/纹理只导入 CPU 描述与压缩图片字节（因子、贴图索引、uri/mime/bufferView、
//     sampler 的 wrap/filter；bufferView/外部文件的图片字节存进 aImageBytes）；
//     不解码、不接渲染侧的 sMATERIAL（解码见 GN_3Dtexture.h）。
//   * 相机/灯光不导入。
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
    int32_t aMaterial; // 材质索引，-1 = 无
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

// 纹理来源描述：image 的 uri/mime/bufferView + 其 sampler 的采样参数。
// 导入时已把可取的压缩字节存进 aImageBytes（bufferView / 相对 .gltf 的外部文件）；
// data URI 不进 aImageBytes，由 GN_3Dtexture 的解码器按 aUri 解出。
// wrap/filter 用 glTF 原始枚举值，0 表示未指定（filter 无规范默认），wrap 缺省 10497(REPEAT)。
struct sASSET_TEXTURE {
    std::string aName;
    std::string aUri; // image.uri，空 = 无外部 uri
    std::string aMime; // image.mimeType，缺失为空
    int32_t aBufferView; // image.bufferView 索引，-1 = 无
    std::vector<uint8_t> aImageBytes; // 压缩图片字节（PNG/JPEG）；空 = 需从 aUri 解
    int32_t aWrapS;
    int32_t aWrapT;
    int32_t aMagFilter; // 9728/9729/9984..9987，0 = 未指定
    int32_t aMinFilter;
};

// 材质 CPU 描述，默认值取 glTF 规范（baseColor=1,1,1,1；metallic=1；roughness=1；emissive=0,0,0）。
struct sASSET_MATERIAL {
    std::string aName;
    float aBaseColorFactor[4];
    float aMetallic;
    float aRoughness;
    float aEmissive[3];
    int32_t aAlbedoTexture; // baseColorTexture 索引，-1 = 无
    int32_t aNormalTexture; // normalTexture 索引，-1 = 无
};

struct sASSET {
    std::vector<sASSET_MESH> aMeshes;
    std::vector<sASSET_NODE> aNodes;
    std::vector<sASSET_SKIN> aSkins;
    std::vector<sASSET_ANIMATION> aAnimations;
    std::vector<sASSET_MATERIAL> aMaterials;
    std::vector<sASSET_TEXTURE> aTextures;

    bool loadFromFile(const char *path);
    bool loadFromMemory(const void *data, size_t size);
    void clear();
    bool isLoaded() const { return !aNodes.empty() || !aMeshes.empty(); }
};

} // namespace GN3d
#endif // _GN_3DASSET_H_
