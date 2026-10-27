#pragma once
#ifndef _GN_3DMODEL_H_
#define _GN_3DMODEL_H_
// ╔══════════════════════════════════════════════════════════╗
// ║  GN_mini — 3D 模型系统                                  ║
// ║  OBJ 加载 + GPU 上传 + 绘制                             ║
// ╚══════════════════════════════════════════════════════════╝

#include "GN_3Dmath.h"
#include "GN_Spatial.h" // sAABB / sFRUSTUM（视锥剔除用）
#ifdef GN_USING_3D
#include "_Vulkan/GN_Vk.h"
#endif

namespace GN3d {
struct sMATERIAL; // 前向声明 (定义在 GN_3Dmaterial.h)

// ═══════════════════════════════════════════════════════════
//  sMESH — 单一网格 (一个材质对应一个网格)
// ═══════════════════════════════════════════════════════════
struct sMESH {
    char aName[128];
    float *aVertices; // VkVertex3D 格式: 11 floats per vertex
    uint32_t *aIndices;
    uint32_t aVertexCount;
    uint32_t aIndexCount;
    float aDiffuse[3];
    float aSpecular[3]; // 镜面反射颜色 (默认 1,1,1)
    float aShininess; // 光泽度 (默认 32)
    sMATERIAL *aMaterial; // 材质引用 (可选, 为0则用内置值)
    GNg::sIMAGE *aAlbedoTex; // 漫反射纹理 (可选, 为0则纯色)
    GNg::sIMAGE *aNormalTex; // 法线贴图 (可选, 为0则无凹凸)

#ifdef GN_USING_3D
    VkBuffer aVertexBuffer;
    VkDeviceMemory aVertexMemory;
    VkBuffer aIndexBuffer;
    VkDeviceMemory aIndexMemory;
#endif

    sMESH();
    ~sMESH();
    void Release();
    void ReleaseGPU();
    void UploadToGPU();
    void Draw(const GNg::sMAT44 *modelMatrix) const;
    void DrawPBR(const GNg::sMAT44 *modelMatrix) const; // PBR 模式绘制
};

// ═══════════════════════════════════════════════════════════
//  cMODEL — 3D 模型 (包含多个网格)
// ═══════════════════════════════════════════════════════════
class cMODEL {
  public:
    char aName[128];
    sMESH *aMeshes;
    uint32_t aMeshCount;
    GNg::sMAT44 aTransform;
    GNg::sVEC3 aPosition;
    GNg::sQUAT aRotation;
    GNg::sVEC3 aScale; // 默认 1,1,1
    bool aVisible;
    sAABB aWorldAABB; // 由 computeWorldAABB() 刷新（世界坐标包围盒）

    cMODEL();
    ~cMODEL();

    // 用网格顶点 × aTransform 计算世界 AABB；无网格/无顶点返回 false（aWorldAABB 保持为空盒）。
    bool computeWorldAABB();
    // 保守视锥剔除：aVisible && 世界 AABB 与视锥相交（空盒/未计算时为 false）。
    bool isVisibleIn(const sFRUSTUM &frustum) const;

    bool LoadOBJ(const char *filename);
    void UploadAllToGPU();
    void Release();
    void ReleaseGPU();
    void UpdateTransform();
    void Draw() const;
    void DrawPBR() const; // PBR 模式绘制所有网格

    void SetPosition(float x, float y, float z) {
        aPosition.x = x;
        aPosition.y = y;
        aPosition.z = z;
        UpdateTransform();
    }
    void SetRotation(const GNg::sQUAT &q) {
        aRotation = q;
        UpdateTransform();
    }
    void SetScale(float sx, float sy, float sz) {
        aScale.x = sx;
        aScale.y = sy;
        aScale.z = sz;
        UpdateTransform();
    }
};

// ═══════════════════════════════════════════════════════════
//  几何体生成器
// ═══════════════════════════════════════════════════════════
cMODEL *CreateCubeModel(float size = 1.0f, float r = 0.8f, float g = 0.8f, float b = 0.8f);
cMODEL *CreateSphereModel(float radius = 1.0f, int segments = 16,
                          float r = 0.8f, float g = 0.8f, float b = 0.8f);
cMODEL *CreatePlaneModel(float width = 1.0f, float depth = 1.0f,
                         float r = 0.8f, float g = 0.8f, float b = 0.8f);

// 视锥剔除：把 models 中可见的下标写入 outIndices（最多 maxOut 个），返回写入数量。
// models 里允许出现空指针（跳过）；不修改模型状态。
uint32_t cullModels(const sFRUSTUM &frustum, const cMODEL *const *models, uint32_t count,
                    uint32_t *outIndices, uint32_t maxOut);

} // namespace GN3d
#endif // _GN_3DMODEL_H_
