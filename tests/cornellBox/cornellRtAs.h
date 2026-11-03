// tests/cornellBox/cornellRtAs.h — 硬件 ray-query 的场景数据与加速结构
//
// 职责：把 sCornellGeometry（四边形 + 球）三角化，上传为「位置/法线/三角形元数据」
// 三个 SSBO，并建 BLAS/TLAS；着色器用 ray query 遍历 TLAS，按 primitive 索引取材质。
// 与 cornellGpuRt 的 compute 路径共用同一份几何（buildCornellGeometry），避免漂移。
#ifndef CORNELL_RT_AS_H
#define CORNELL_RT_AS_H

#include "cornellGeometry.h"
#include "cornellVkUtil.h"

#include <vulkan/vulkan.h>

#include <cstdint>

namespace Cornell {

// 硬件光追场景：着色器可见的 3 个 SSBO + TLAS；其余 AS 资源由本模块持有/销毁。
struct sRtAsScene {
    VkAccelerationStructureKHR aTlas;
    VkAccelerationStructureKHR aBlas;
    sVkBuffer aVertexBuffer;  // vec4：xyz = 位置
    sVkBuffer aNormalBuffer;  // vec4：xyz = 法线
    sVkBuffer aTriMetaBuffer; // uvec4：i0,i1,i2,material
    uint32_t aTriangleCount;
    uint32_t aVertexCount;
    // 内部 AS 资源
    sVkBuffer aBlasBuffer;
    sVkBuffer aTlasBuffer;
    sVkBuffer aInstanceBuffer;
    sVkBuffer aIndexBuffer;
};

// 三角化 + 建 BLAS/TLAS。失败返回 false（调用方回退 compute 路径）。
bool buildRayQueryScene(VkDevice device, VkPhysicalDevice physical, VkQueue queue,
                        uint32_t queueFamily, const sCornellGeometry &geometry,
                        sRtAsScene *out);

// 释放 buildRayQueryScene 创建的全部资源；重复调用安全。
void destroyRayQueryScene(VkDevice device, sRtAsScene *scene);

} // namespace Cornell
#endif // CORNELL_RT_AS_H
