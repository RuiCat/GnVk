#ifndef _GN_3D_PROBE_RELOCATION_H_
#define _GN_3D_PROBE_RELOCATION_H_
// GN_3DprobeRelocation.h — CPU 侧 probe relocation 偏移计算（无 Vulkan）。
// 输入是 GPV4 的逐 texel depth moments（mean/mean²），输出每探针 4 float
// （xyz=世界偏移，w=有效标记）。算法是启发式：短命中方向的反方向即离开几何的方向；
// 用于 Cornell DDGI 的 B3b，不替代 RTXGI 的完整 relocation。

#include <cstdint>

namespace GN3d {

struct sPROBE_RELOCATION_DESC {
    const float *aDepthMoments; // probes * 6 * res * res * 2（mean, mean²）
    uint32_t aProbeCount;
    uint32_t aDepthRes;
    float aSpacing;
    float aThreshold;  // mean < threshold*spacing 视为“探针贴住/进入几何”
    float aMaxOffset;  // 逐轴偏移上限（世界单位）
};

// 成功写出 outOffsets（4*probeCount float）返回 true；非法输入返回 false 且不写 out。
bool computeProbeRelocationOffsets(const sPROBE_RELOCATION_DESC &desc, float *outOffsets);

} // namespace GN3d
#endif // _GN_3D_PROBE_RELOCATION_H_
