#pragma once
#ifndef GN_IMOSCENE_CONVERT_H
#define GN_IMOSCENE_CONVERT_H
// tests/imoScene/imoSceneConvert.h — glTF → 场景网格数据（CPU 烘焙蒙皮，不碰 Vulkan）。
//
// 边界：
//   * 只读 .gltf/.glb（GN3d::sASSET），输出 VkVertex3D 布局的顶点数组与材质/贴图描述；
//   * 蒙皮在导入姿势（无动画）下烘焙进顶点，关节数不受 GPU 调色板上限约束；
//   * 不创建 sIMAGE、不传 GPU、不做剔除（上传见 imoSceneUpload，渲染见 imoSceneMain）；
//   * 网格统一重定心：脚底 y=0、水平中心 xz=0，供场景摆位与相机取景。
#include "GN.h"
#include "GN_3Dmaterial.h"

#include <cstdint>
#include <string>
#include <vector>

namespace imoScene {

// 一个 glTF primitive 烘焙成的网格（顶点布局 = VkVertex3D：pos3 / normal3 / tangent3 / uv2）。
struct sMeshData {
    std::string aName;
    std::vector<float> aVertices;
    std::vector<uint32_t> aIndices;
    int32_t aMaterial; // 材质下标，-1 = 无
    GNg::sVEC3 aMin;   // 重定心后的 AABB
    GNg::sVEC3 aMax;

    sMeshData();
};

// 材质：alpha 模式按贴图 alpha 分布重判（FBX 导出的 glTF 常把全部材质标成 BLEND）。
struct sMaterialData {
    std::string aName;
    float aDiffuse[3];
    float aMetallic;
    float aRoughness;
    GN3d::eALPHA_MODE aAlphaMode;
    float aAlphaCutoff;
    int32_t aAlbedoTexture; // 贴图下标，-1 = 无
    int32_t aNormalTexture;

    sMaterialData();
};

// 贴图：保留原始压缩字节（PNG/JPEG），alpha 统计在 CPU 解码阶段得出。
struct sTextureData {
    std::string aName;
    std::vector<uint8_t> aBytes;
    bool aHasAlpha;          // 存在 alpha < 250 的像素
    float aPartialAlphaRatio; // 0 < alpha < 250 的像素占比

    sTextureData();
};

// 整份场景模型的 CPU 数据；顶点已重定心，aMin/aMax 同步更新。
struct sModelData {
    std::vector<sMeshData> aMeshes;
    std::vector<sMaterialData> aMaterials;
    std::vector<sTextureData> aTextures;
    GNg::sVEC3 aMin;
    GNg::sVEC3 aMax;
    uint32_t aVertexCount;
    uint32_t aTriangleCount;
    uint32_t aZeroWeightVertices; // 四个权重全为 0 的顶点数（烘焙时保留原位置）
    uint32_t aSkippedPrimitives;  // 缺 POSITION 等不可用 primitive 数

    sModelData();
    void clear();
};

// 读取 glTF/GLB 并烘焙成 sModelData；失败时 out 清空、error 写明原因。
bool buildModelFromGltf(const char *path, sModelData &out, std::string &error);

// alpha 模式的可读名（日志/诊断用）。
const char *alphaModeName(GN3d::eALPHA_MODE mode);

} // namespace imoScene
#endif // GN_IMOSCENE_CONVERT_H
