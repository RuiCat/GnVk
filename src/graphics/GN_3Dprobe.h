// GN_3Dprobe.h — SH9 探针网格（CPU）：网格状态、世界→格索引映射、三线性插值、
// Recenter 整数格搬移与时间滞后混合。纯 CPU（不依赖 Vulkan），SH9 布局与 giParams
// 的 36 float（9×vec4）一致。GPU 3D 纹理上传/着色器采样属 G5b，本文件仅预留注释。
#pragma once
#ifndef GN_3D_PROBE_H
#define GN_3D_PROBE_H

#include <GN.h>
#include <cstdint>

namespace GN3d {

// 探针网格描述：aOrigin 为索引 (0,0,0) 探针的世界坐标。
struct sPROBE_GRID_DESC {
    GNg::sVEC3 aOrigin = {0.0f, 0.0f, 0.0f};
    float aSpacing = 2.0f;
    uint32_t aCountX = 8;
    uint32_t aCountY = 4;
    uint32_t aCountZ = 8;
};

// 探针总上限（与 docs/GI_PLAN.md §7 的轻量方案一致）与 SH9 浮点数（9×vec4）。
constexpr uint32_t PROBE_GRID_MAX_PROBES = 1024;
constexpr int PROBE_SH9_FLOAT_COUNT = 36;

// G5a：SH9 探针网格 CPU 基座。接口约定：
//  - 所有 SH9 入参/出参为 float[9][4]，内部按 36 float 连续存储（与 giParams 一致）；
//  - SampleSh 对边界外坐标做夹紧（不越界、不返回脏数据）；
//  - Recenter 只在相机位移达到 2×spacing 时搬移，搬移按整数格进行并保留重叠区 SH9，
//    越界的新格夹取最近的旧值（不丢已有数据）。
class cPROBE_GRID {
  public:
    cPROBE_GRID();

    // 校验并重置网格：counts>0、spacing>0、总探针 <= PROBE_GRID_MAX_PROBES。
    bool Init(const sPROBE_GRID_DESC &desc);

    // 清空为默认无效状态（不动 desc 的默认值）。
    void Reset();

    // 相机移动后重定位网格。位移 < 2×spacing 返回 false（不搬移）；否则按整数格
    // 搬移 SH9 并返回 true。Init 后以描述原点作为初始参考点。
    bool Recenter(const GNg::sVEC3 &cameraPos);

    // 写入/混合单个探针（index 越界或空指针返回 false）。
    bool SetProbeSh(uint32_t index, const float sh9[9][4]);
    bool BlendProbeSh(uint32_t index, const float target[9][4], float alpha);

    // 三线性采样 SH9；世界坐标先夹到网格范围，再取 8 邻域加权。
    bool SampleSh(const GNg::sVEC3 &worldPos, float outSh9[9][4]) const;

    // 读取单探针的 36 float（越界/空指针返回 false）。
    bool GetProbeSh(uint32_t index, float out36[PROBE_SH9_FLOAT_COUNT]) const;

    uint32_t GetProbeCount() const;
    uint32_t GetCountX() const { return aDesc.aCountX; }
    uint32_t GetCountY() const { return aDesc.aCountY; }
    uint32_t GetCountZ() const { return aDesc.aCountZ; }
    float GetSpacing() const { return aDesc.aSpacing; }
    GNg::sVEC3 GetOrigin() const { return aDesc.aOrigin; }
    bool IsValid() const { return aValid; }

  private:
    uint32_t probeIndex(uint32_t x, uint32_t y, uint32_t z) const;
    void clearData();

    bool aValid;
    sPROBE_GRID_DESC aDesc;
    GNg::sVEC3 aLastCamera; // 上次重定位的参考点（Init 时取描述原点）
    float aSh[PROBE_GRID_MAX_PROBES][PROBE_SH9_FLOAT_COUNT];
};

// G5b-1（裁决 A）：把网格打包成 9 张 8×4×8 RGBA16F 纹理（float 层，无损 36 float/探针）。
// 输出布局：[k=0..8][z=0..countZ-1][y=0..countY-1][x=0..countX-1][c=0..3]，
// 索引公式：out[((((k * countZ + z) * countY + y) * countX + x) * 4) + c]。
// 默认网格总 float 数 = 9 × 8 × 4 × 8 × 4 = 9216（约 72 KB staging）。
// out 为 null 或 grid 未 Init 返回 false（不写 out）。
bool packProbeTextures(const cPROBE_GRID &grid, float *out);

} // namespace GN3d

#endif // GN_3D_PROBE_H
