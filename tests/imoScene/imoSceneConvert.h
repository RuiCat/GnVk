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
#include "GN_3Danim.h"
#include "GN_3Dmaterial.h"

#include <cstdint>
#include <string>
#include <vector>

namespace imoScene {

// 绑定姿势顶点（动画重蒙皮用）：关节下标指向 sSkinRig::aJointNodes，权重未归一化。
struct sBindVertex {
    GNg::sVEC3 aPosition;
    GNg::sVEC3 aNormal;
    float aUv[2];
    uint16_t aJoints[4];
    float aWeights[4];

    sBindVertex();
};

// 蒙皮骨架：关节顺序 = glTF skin.joints 顺序，也是 sBindVertex::aJoints 的下标空间。
struct sSkinRig {
    std::vector<uint32_t> aJointNodes;             // 关节 → sASSET 节点下标
    std::vector<GNg::sMAT44> aInverseBinds;        // 绑定姿势世界矩阵的逆
    std::vector<int32_t> aParents;                 // 关节内父下标（拓扑序，-1 = 根）
    std::vector<GN3d::sANIM_TRANSFORM> aBindLocal; // 每关节绑定局部 TRS

    sSkinRig();
    bool empty() const { return aJointNodes.empty(); }
};

// 一个 glTF primitive 烘焙成的网格（顶点布局 = VkVertex3D：pos3 / normal3 / tangent3 / uv2）。
struct sMeshData {
    std::string aName;
    std::vector<float> aVertices;
    std::vector<uint32_t> aIndices;
    int32_t aMaterial; // 材质下标，-1 = 无
    GNg::sVEC3 aMin;   // 重定心后的 AABB
    GNg::sVEC3 aMax;
    std::vector<sBindVertex> aBindVertices; // 非空 = 该网格可重蒙皮

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
    int32_t aWrapS;          // glTF sampler wrap_s 原始枚举（10497=REPEAT 缺省）
    int32_t aWrapT;          // glTF sampler wrap_t 原始枚举

    sTextureData();
};

// 整份场景模型的 CPU 数据；顶点已重定心，aMin/aMax 同步更新。
struct sModelData {
    std::vector<sMeshData> aMeshes;
    std::vector<sMaterialData> aMaterials;
    std::vector<sTextureData> aTextures;
    GNg::sVEC3 aMin;
    GNg::sVEC3 aMax;
    GNg::sVEC3 aRecenterOffset; // 重定心平移（动画重蒙皮后要补同一偏移）
    sSkinRig aSkin;
    uint32_t aVertexCount;
    uint32_t aTriangleCount;
    uint32_t aZeroWeightVertices; // 四个权重全为 0 的顶点数（烘焙时保留原位置）
    uint32_t aSkippedPrimitives;  // 缺 POSITION 等不可用 primitive 数
    uint32_t aFilteredMeshes;     // 被 show/hide 过滤掉的网格数

    sModelData();
    void clear();
};

// 转换选项：网格名过滤（逗号分隔的**子串**，大小写不敏感），用于隐藏重复服装/特效网格。
struct sConvertOptions {
    std::string aShowFilter; // 非空时只保留命中该列表的网格
    std::string aHideFilter; // 命中该列表的网格一律跳过
    bool aForceOpaque;       // 调试：忽略 alpha，全部按 OPAQUE 画（区分"纹理黑"与"镂空"）
    bool aRecenter;          // true = 脚底 y=0、水平 xz 居中（单角色）；房间等世界坐标组装要关
    bool aClassifyAlpha;     // true = 不信 glTF alphaMode，按贴图 alpha 统计重判（静态场景）

    sConvertOptions();
    // 说明见字段注释。
};

// 读取 glTF/GLB 并烘焙成 sModelData；失败时 out 清空、error 写明原因。
bool buildModelFromGltf(const char *path, sModelData &out, std::string &error,
                        const sConvertOptions &options = sConvertOptions());

// alpha 模式的可读名（日志/诊断用）。
const char *alphaModeName(GN3d::eALPHA_MODE mode);

} // namespace imoScene
#endif // GN_IMOSCENE_CONVERT_H