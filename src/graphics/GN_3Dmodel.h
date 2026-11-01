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
//  sMESH_LOD — P10 烘焙级（CPU 数据；PBR 路径每次绘制动态上传，不需要独立 VkBuffer）
// ═══════════════════════════════════════════════════════════
struct sMESH_LOD {
    float *aVertices; // VkVertex3D 格式（11 floats/顶点）
    uint32_t *aIndices;
    uint32_t aVertexCount;
    uint32_t aIndexCount;
    float aMaxError; // 相对 LOD0 的世界空间最大几何误差（米）；LOD0 必须为 0

    sMESH_LOD();
    ~sMESH_LOD();
};

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
    // P10：可选 LOD 级；aLodCount == 0 表示单级（旧行为，既有调用不受影响）。
    sMESH_LOD *aLods;
    uint32_t aLodCount;
    float aBoundRadius; // 局部包围球半径（0 = 未计算）

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
    // P10：按指定级绘制（level 越界/该级无数据时退回 LOD0，即 aVertices/aIndices）。
    // forceBlend：true 时即使材质不是 eALPHA_BLEND 也走混合管线（供排序绘制第二趟使用）。
    void DrawPBRLevel(const GNg::sMAT44 *modelMatrix, uint32_t level, bool forceBlend = false) const;
    // L3：阴影 pass 绘制（深度-only；alpha 非 OPAQUE 的网格由 cMODEL::DrawShadow 跳过）。
    void DrawShadow(const GNg::sMAT44 *modelMatrix) const;
};

// ═══════════════════════════════════════════════════════════
//  cMODEL — 3D 模型 (包含多个网格)
// ═══════════════════════════════════════════════════════════
class cMODEL {
  public:
    char aName[128];
    sMESH *aMeshes;
    uint32_t aMeshCount;
    sMATERIAL *aMaterials; // 材质池（cMODEL 拥有；网格 aMaterial 指向其中，可为空）
    uint32_t aMaterialCount;
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
    // L3：阴影投影绘制（只画 OPAQUE 网格；透明/MASK 网格 v1 不投影，避免整片硬影）。
    void DrawShadow() const;
    // 透明排序绘制：先 OPAQUE/MASK（原顺序），再 BLEND 按网格世界 AABB 中心到相机
    // 由远到近逐个绘制（仅网格级排序，非逐三角形）。
    void DrawPBRSorted(const GNg::sVEC3 &cameraPos) const;
    // P10：按每个网格的选级结果绘制；levels 可空（空 = 全部 LOD0，等价 DrawPBR）。
    void DrawPBRWithLod(const uint32_t *levels, uint32_t levelCount) const;

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

// 剔除绘制：复用 cullModels 的可见列表，只对可见模型调用 DrawPBR，返回实际绘制数。
// models 里允许出现空指针；outVisible 可空，非空时写入可见模型数。
// 无 GPU（未初始化/不在 3D pass 内）时 DrawPBR 为 no-op，但返回值仍是可见计数。
uint32_t cullDrawModelsPBR(const sFRUSTUM &frustum, cMODEL *const *models, uint32_t count,
                           uint32_t *outVisible);

} // namespace GN3d
#endif // _GN_3DMODEL_H_
