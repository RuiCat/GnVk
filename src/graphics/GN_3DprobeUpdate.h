#ifndef _GN_3D_PROBE_UPDATE_H_
#define _GN_3D_PROBE_UPDATE_H_
// GN_3DprobeUpdate.h — DDGI 异步更新的 CPU 调度/混合基座（无 Vulkan）。
// 只负责“每帧更新哪一段探针、temporal alpha 多大、SH9 怎么混合”；
// GPU compute 与双缓冲在后续批次接线。

#include <cstdint>

namespace GN3d {

struct sPROBE_UPDATE_SCHEDULER {
    uint32_t aCursor;     // 下一次从哪个探针开始
    uint32_t aBudget;     // 每帧更新多少个
    uint32_t aProbeCount; // 网格总探针数
};

struct sPROBE_UPDATE_RANGE {
    uint32_t aStart;
    uint32_t aCount;
};

// 初始化；aBudget=0 时按 1 处理，aProbeCount=0 时范围为空。
void initProbeUpdateScheduler(sPROBE_UPDATE_SCHEDULER *scheduler, uint32_t probeCount,
                              uint32_t budget);
// 取本帧范围并推进 cursor（环绕）；非法输入返回 {0,0}。
sPROBE_UPDATE_RANGE nextProbeUpdateRange(sPROBE_UPDATE_SCHEDULER *scheduler);

// 96 B probe UBO（std140：6×vec4）。前 48 B 与静态渲染路径逐字段一致，
// 后三个 vec4 只被 probe update compute 读取（见 docs/PROBE_BAKE_PLAN.md §10.2.1）。
struct sPROBE_UPDATE_UBO {
    float aOrigin[4];        // xyz = 网格原点，w = 保留
    float aSpacingCounts[4]; // x = spacing，yzw = counts
    float aWeight[4];        // x = 强度，y = depthRes，z = normalBias，w = viewBias
    float aRelocation[4];    // x = maxOffset，y = insideThreshold，z = iterations，w = enable
    float aUpdate[4];        // x = rangeStart，y = rangeCount，z = hysteresis，w = flags
    float aState[4];         // x = spp，y = maxAge，z = epsilon，w = 保留
};

// 每探针状态，对应 SSBO 的 uvec4[]：age / class / score(×1000) / flags。
struct sPROBE_STATE {
    uint32_t aAge;
    uint32_t aClass;
    uint32_t aScoreMilli;
    uint32_t aFlags;
};

enum ePROBE_STATE_CLASS {
    ePROBE_CLASS_ACTIVE = 0,
    ePROBE_CLASS_RELOCATED = 1,
    ePROBE_CLASS_INACTIVE = 2,
};

// 全零初始化；depthRes=4、spp=4、maxAge=65535。
void initProbeUpdateUbo(sPROBE_UPDATE_UBO *ubo);
// 按 std140 顺序把 6×vec4 写成 24 个 float；空指针返回 false 且不改 out。
bool packProbeUpdateUbo(const sPROBE_UPDATE_UBO *ubo, float out[24]);
// age = min(age+1, maxAge)；maxAge=0 钳到 1。
uint32_t advanceProbeAge(uint32_t age, uint32_t maxAge);
// 把 [0, probeCount) 的状态重置为 active/age=0/score=0/flags=0。
void resetProbeStates(sPROBE_STATE *states, uint32_t probeCount);

// temporal alpha = max(1/(age+1), 1-hysteresis)，钳制 [0,1]。
float probeUpdateAlpha(uint32_t age, float hysteresis);
// out = mix(old, sample, alpha)；alpha 钳制 [0,1]；36 float（9×vec4）。
void blendProbeSh(const float oldSh[36], const float sampleSh[36], float alpha, float outSh[36]);

} // namespace GN3d
#endif // _GN_3D_PROBE_UPDATE_H_
