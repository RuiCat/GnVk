#pragma once
#ifndef GN_MMD_MESH_BUILDER_H
#define GN_MMD_MESH_BUILDER_H
// ╔══════════════════════════════════════════════════════════╗
// ║  GN_mini — MMD 模型 → 引擎网格（CPU 数据）              ║
// ╚══════════════════════════════════════════════════════════╝
//
// 一个 MMD 材质对应一个 sMESH：PMD/PMX 的索引缓冲按材质的 indexCount 顺序切段，
// 每段压缩成独立顶点数组（去掉未引用顶点）。PMD/PMX 是左手系（+Y 上、+Z 前）；
// 本层镜像 X 转右手系后正面保持 +Z，并**反转每个三角形的绕序**——
// 镜像（行列式 -1）会反转绕序，而原始数据的绕序与顶点法线一致，不翻回来就会内外翻转
// （CPU 探针 mode.pmd：镜像前 agree=6189/disagree=40，镜像后 agree=40/disagree=6189）。
// 最终约定见 GN_Space.h：世界 CCW + 外向法线。
//
// 边界：
//   * 只生成 CPU 顶点/索引与材质描述，不读贴图字节、不碰 Vulkan；
//   * 蒙皮暂不参与：顶点为绑定姿势（PMD/PMX 文件内的原始位置）；
//   * 不实现 MMD 的卡通贴图/球面贴图/透明混合，只带漫反射贴图路径；
//   * 只使用漫反射 RGB、镜面 RGB 与光泽度（MMD power 夹到 1..256）；
//   * 每个网格生成一份 sMATERIAL（由 cMODEL::aMaterials 拥有）：alphaMode=MASK、
//     cutoff=0.5（MMD 头发/裙边靠贴图 alpha 做 cutout；BLEND 未实现，PMX 透明材质
//     暂按硬边裁剪处理）；纹理仍由调用方挂 mesh.aAlbedoTex。
#include <GN.h>
#include "pmdModel.h"
#include "pmxModel.h"

#include <cstdint>
#include <string>
#include <vector>

namespace mmd {

// 网格 → 贴图相对路径（尚未与模型所在目录拼接，供查看器解析）。
struct MeshTextureBinding {
    uint32_t meshIndex;
    std::string texturePath;
};

// 把解析后的 PMX 转成 cMODEL（每个材质一个网格）；成功返回 true。
bool buildPmxModel(const PmxModel &source, GN3d::cMODEL &outModel,
                   std::vector<MeshTextureBinding> &outTextures, std::string *error = nullptr);

// 把解析后的 PMD 转成 cMODEL（每个材质一个网格）；成功返回 true。
bool buildPmdModel(const PmdModel &source, GN3d::cMODEL &outModel,
                   std::vector<MeshTextureBinding> &outTextures, std::string *error = nullptr);

} // namespace mmd
#endif // GN_MMD_MESH_BUILDER_H
