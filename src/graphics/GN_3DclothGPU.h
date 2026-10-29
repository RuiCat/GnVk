#pragma once
#ifndef _GN_3DCLOTH_GPU_H_
#define _GN_3DCLOTH_GPU_H_
// ╔══════════════════════════════════════════════════════════╗
// ║  GN_mini — P9 GPU 布料：SSBO 数据契约 + CPU 回退/对拍     ║
// ╚══════════════════════════════════════════════════════════╝
//
// 契约（CPU 与 compute 必须逐位等价，浮点容差 1e-3）：
//   * 规则网格 W × H，顶点 v = j*W + i（行优先），初始位置 (i*spacing, j*spacing, 0)。
//   * 邻域固定顺序：左 (i-1,j)、右 (i+1,j)、上 (i,j-1)、下 (i,j+1)；越界跳过；静止长度均为 spacing。
//   * `aPinned = 1` 的顶点完全静止（不积分、不被约束推动），pos/prev 保持上传值。
//
// 一步（dt > 0 且 desc 合法）：
//   1) 积分（仅非 pinned）：
//        vx = (pos.x - prev.x) * (1 - damping)   // 三个轴同式
//        prev = pos
//        pos = pos + v + gravity * dt * dt
//   2) 约束迭代，共 aIterations 轮，**Jacobi 语义**（每轮读旧 pos、写 tmp，轮末整体替换，
//      不得就地写；CPU 与 GPU 都必须如此，否则结果不可比）：
//        对每个非 pinned 顶点 v：
//          corr = 0; cnt = 0
//          对每个存在的邻域 n（按上面的固定顺序）：
//            d = pos[n] - pos[v]; len = |d|
//            if (len > 1e-6) { corr += d * ((len - spacing) / len) * 0.5; cnt++ }
//          tmp[v] = (cnt > 0) ? pos[v] + corr / cnt : pos[v]
//        pinned 顶点 tmp = pos；轮末 pos = tmp。
//   3) 不改 prev（Verlet 中 prev 即速度载体）。
#include <cstdint>

namespace GN3d {

const uint32_t CLOTH_GPU_MAX_DIM = 1024; // 单边顶点上限
const uint32_t CLOTH_GPU_MAX_ITERATIONS = 8;

struct sCLOTH_GPU_DESC {
    uint32_t aWidth; // 列数（>= 2）
    uint32_t aHeight; // 行数（>= 2）
    float aSpacing; // 静止间距（> 0）
    float aGravity[3]; // 重力加速度
    float aDamping; // 速度阻尼 [0, 1)
    uint32_t aIterations; // 约束迭代轮数（1..8）
};

// SSBO 布局：32 字节/顶点（std430，两个 vec4：pos+pinned / prev+pad）。
// 改字段必须同步 gn3d_cloth.comp/vert 的 ClothVertex。
struct sCLOTH_GPU_VERTEX {
    float aPos[3]; // 世界位置
    float aPinned; // 1 = 固定（占 pos 的 w 分量，保证两个 vec4 对齐）
    float aPrev[3]; // 上一步位置（Verlet 速度）
    float aPad; // 对齐填充，必须为 0
};

// 参数合法：尺寸 2..CLOTH_GPU_MAX_DIM，spacing > 0，迭代 1..MAX，damping ∈ [0,1)，重力有限。
bool validateClothGpuDesc(const sCLOTH_GPU_DESC &desc);

// CPU 回退 + GPU 对拍参考：按上面的公式推进一步；非法输入（空指针/count 不符/dt<=0/非法 desc）
// 不改任何数据。count 必须等于 desc.aWidth * desc.aHeight。
void stepClothCpu(sCLOTH_GPU_VERTEX *vertices, uint32_t count, const sCLOTH_GPU_DESC &desc,
                  float dt);

} // namespace GN3d
#endif // _GN_3DCLOTH_GPU_H_
