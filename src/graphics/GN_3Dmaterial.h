#pragma once
#ifndef _GN_3DMATERIAL_H_
#define _GN_3DMATERIAL_H_
// ╔══════════════════════════════════════════════════════════╗
// ║  GN_mini — 3D 材质系统                                  ║
// ║  漫反射/镜面反射/光泽度 + PBR 预留                        ║
// ╚══════════════════════════════════════════════════════════╝

#include "GN_3Dmath.h"

// 前向声明
namespace GNg {
struct sIMAGE;
}

namespace GN3d {

// ═══════════════════════════════════════════════════════════
//  材质属性 (Blinn-Phong + PBR 共用)
// ═══════════════════════════════════════════════════════════
struct sMATERIAL {
    GNg::sVEC3 aDiffuse; // 漫反射颜色 (默认 0.8, 0.8, 0.8)
    GNg::sVEC3 aSpecular; // 镜面反射颜色 (默认 1.0, 1.0, 1.0)
    float aShininess; // 光泽度 (1-256, 默认 32)
    GNg::sVEC3 aEmissive; // 自发光颜色 (默认 0, 0, 0)
    float aRoughness; // 粗糙度 PBR (0-1, 默认 0.5) — 阶段三使用
    float aMetallic; // 金属度 PBR (0-1, 默认 0.0) — 阶段三使用
    GNg::sIMAGE *aAlbedoTex; // Albedo 纹理 (可选)
    GNg::sIMAGE *aNormalTex; // 法线贴图 (可选)

    sMATERIAL();
    void SetDefault(); // 重置为默认灰色
    void SetWhite(float shininess = 32.0f);
    void SetRed(float shininess = 32.0f);
    void SetGreen(float shininess = 32.0f);
    void SetBlue(float shininess = 32.0f);
    void SetGold(); // 金色金属
    void SetSilver(); // 银色金属
    void SetPlastic(float r, float g, float b, float shininess = 64.0f);
};

// ═══════════════════════════════════════════════════════════
//  GPU Push Constant 材质布局 (与着色器 ModelPC 对齐)
// ═══════════════════════════════════════════════════════════
struct alignas(16) sMATERIAL_PC {
    float model[16]; // 模型矩阵
    float diffuse[4]; // 漫反射 RGB + _pad
    float specular[4]; // 镜面反射 RGB + shininess
};

} // namespace GN3d
#endif // _GN_3DMATERIAL_H_
