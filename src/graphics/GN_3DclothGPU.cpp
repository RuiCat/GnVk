// GN_3DclothGPU.cpp — P9-a GPU 布料的 CPU 侧：参数校验与 CPU 回退/对拍实现。
//
// 契约：
//   * 与 assets/shaders/gn3d_cloth.comp 同一套公式（Verlet 积分 + Jacobi 约束迭代），
//     两份实现的结果差异是 GPU 验收判据（cloth.cpuMatch，容差 1e-3）。
//   * 本文件不触碰 Vulkan：无 GPU / 未初始化时 CPU 回退仍可用。
#include "GN_3DclothGPU.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace GN3d {

namespace {

bool isFinite3(const float v[3]) {
    return std::isfinite(v[0]) && std::isfinite(v[1]) && std::isfinite(v[2]);
}

} // namespace

bool validateClothGpuDesc(const sCLOTH_GPU_DESC &desc) {
    if(desc.aWidth < 2 || desc.aHeight < 2) return false;
    if(desc.aWidth > CLOTH_GPU_MAX_DIM || desc.aHeight > CLOTH_GPU_MAX_DIM) return false;
    if(!(desc.aSpacing > 0.0f) || !std::isfinite(desc.aSpacing)) return false;
    if(!isFinite3(desc.aGravity)) return false;
    if(!std::isfinite(desc.aDamping) || desc.aDamping < 0.0f || desc.aDamping >= 1.0f) return false;
    if(desc.aIterations == 0 || desc.aIterations > CLOTH_GPU_MAX_ITERATIONS) return false;
    return true;
}

void stepClothCpu(sCLOTH_GPU_VERTEX *vertices, uint32_t count, const sCLOTH_GPU_DESC &desc,
                  float dt) {
    if(!vertices || count == 0 || !(dt > 0.0f) || !std::isfinite(dt)) return;
    if(!validateClothGpuDesc(desc)) return;
    const uint32_t width = desc.aWidth;
    const uint32_t height = desc.aHeight;
    if((uint64_t)width * (uint64_t)height != (uint64_t)count) return;

    // ── 1) Verlet 积分（pinned 完全跳过）────────────────────
    const float dampingScale = 1.0f - desc.aDamping;
    const float gravityStep = desc.aGravity[0] * dt * dt;
    (void)gravityStep;
    for(uint32_t index = 0; index < count; ++index) {
        sCLOTH_GPU_VERTEX &vertex = vertices[index];
        if(vertex.aPinned >= 0.5f) continue;
        for(int axis = 0; axis < 3; ++axis) {
            const float velocity = (vertex.aPos[axis] - vertex.aPrev[axis]) * dampingScale;
            vertex.aPrev[axis] = vertex.aPos[axis];
            vertex.aPos[axis] = vertex.aPos[axis] + velocity + desc.aGravity[axis] * dt * dt;
        }
    }

    // ── 2) Jacobi 约束迭代（整轮读旧值，轮末整体替换）────────
    std::vector<sCLOTH_GPU_VERTEX> snapshot(vertices, vertices + count);
    static const int kOffsetX[4] = {-1, 1, 0, 0};
    static const int kOffsetY[4] = {0, 0, -1, 1};
    for(uint32_t iteration = 0; iteration < desc.aIterations; ++iteration) {
        snapshot.assign(vertices, vertices + count);
        for(uint32_t index = 0; index < count; ++index) {
            if(vertices[index].aPinned >= 0.5f) continue;
            const uint32_t column = index % width;
            const uint32_t row = index / width;
            float correction[3] = {0.0f, 0.0f, 0.0f};
            uint32_t neighbourCount = 0;
            for(int k = 0; k < 4; ++k) {
                const int neighbourColumn = (int)column + kOffsetX[k];
                const int neighbourRow = (int)row + kOffsetY[k];
                if(neighbourColumn < 0 || neighbourColumn >= (int)width) continue;
                if(neighbourRow < 0 || neighbourRow >= (int)height) continue;
                const uint32_t neighbour = (uint32_t)neighbourRow * width + (uint32_t)neighbourColumn;
                float delta[3];
                float lengthSquared = 0.0f;
                for(int axis = 0; axis < 3; ++axis) {
                    delta[axis] = snapshot[neighbour].aPos[axis] - snapshot[index].aPos[axis];
                    lengthSquared += delta[axis] * delta[axis];
                }
                const float length = std::sqrt(lengthSquared);
                if(length <= 1e-6f) continue;
                const float scale = (length - desc.aSpacing) / length * 0.5f;
                for(int axis = 0; axis < 3; ++axis) correction[axis] += delta[axis] * scale;
                ++neighbourCount;
            }
            if(neighbourCount == 0) continue;
            for(int axis = 0; axis < 3; ++axis) {
                vertices[index].aPos[axis] =
                    snapshot[index].aPos[axis] + correction[axis] / (float)neighbourCount;
            }
        }
    }
}

} // namespace GN3d
