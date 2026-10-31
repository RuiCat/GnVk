// GN_3Dprobe.cpp — SH9 探针网格 CPU 实现：Init 校验、三线性采样、整数格 Recenter、
// 时间滞后混合。纯 CPU，不依赖 Vulkan；G5b 的 GPU 上传/采样在此不实现。
#include "GN_3Dprobe.h"

#include <cmath>
#include <cstring>
#include <vector>

namespace GN3d {
namespace {

// 把坐标夹到 [0, count-1] 并返回格坐标与邻格索引。
void gridCoord(float value, float origin, float spacing, uint32_t count, uint32_t *base,
               uint32_t *next, float *frac) {
    float g = (value - origin) / spacing;
    const float maxIndex = static_cast<float>(count - 1);
    if(!(g > 0.0f)) g = 0.0f; // NaN 也走 0
    if(g > maxIndex) g = maxIndex;
    const uint32_t i0 = static_cast<uint32_t>(g);
    const uint32_t i1 = (i0 + 1u < count) ? i0 + 1u : i0;
    *base = i0;
    *next = i1;
    *frac = (i1 == i0) ? 0.0f : (g - static_cast<float>(i0));
}

} // namespace

cPROBE_GRID::cPROBE_GRID() : aValid(false) {
    aDesc = sPROBE_GRID_DESC();
    aLastCamera = aDesc.aOrigin;
    clearData();
}

void cPROBE_GRID::Reset() {
    aValid = false;
    aDesc = sPROBE_GRID_DESC();
    aLastCamera = aDesc.aOrigin;
    clearData();
}

void cPROBE_GRID::clearData() {
    std::memset(aSh, 0, sizeof(aSh));
}

bool cPROBE_GRID::Init(const sPROBE_GRID_DESC &desc) {
    if(desc.aCountX == 0u || desc.aCountY == 0u || desc.aCountZ == 0u) return false;
    if(!(desc.aSpacing > 0.0f) || !std::isfinite(desc.aSpacing)) return false;
    const uint64_t total = static_cast<uint64_t>(desc.aCountX) * desc.aCountY * desc.aCountZ;
    if(total == 0u || total > PROBE_GRID_MAX_PROBES) return false;
    aDesc = desc;
    aLastCamera = desc.aOrigin;
    aValid = true;
    clearData();
    return true;
}

uint32_t cPROBE_GRID::probeIndex(uint32_t x, uint32_t y, uint32_t z) const {
    return (z * aDesc.aCountY + y) * aDesc.aCountX + x;
}

uint32_t cPROBE_GRID::GetProbeCount() const {
    if(!aValid) return 0u;
    return aDesc.aCountX * aDesc.aCountY * aDesc.aCountZ;
}

bool cPROBE_GRID::SetProbeSh(uint32_t index, const float sh9[9][4]) {
    if(!aValid || !sh9 || index >= GetProbeCount()) return false;
    std::memcpy(aSh[index], sh9, sizeof(aSh[index]));
    return true;
}

bool cPROBE_GRID::GetProbeSh(uint32_t index, float out36[PROBE_SH9_FLOAT_COUNT]) const {
    if(!aValid || !out36 || index >= GetProbeCount()) return false;
    std::memcpy(out36, aSh[index], sizeof(aSh[index]));
    return true;
}

bool cPROBE_GRID::BlendProbeSh(uint32_t index, const float target[9][4], float alpha) {
    if(!aValid || !target || index >= GetProbeCount()) return false;
    if(!(alpha >= 0.0f)) return false; // NaN / 负数：不改
    if(alpha > 1.0f) alpha = 1.0f;
    for(int i = 0; i < PROBE_SH9_FLOAT_COUNT; ++i) {
        aSh[index][i] = aSh[index][i] * (1.0f - alpha) + target[i / 4][i % 4] * alpha;
    }
    return true;
}

bool cPROBE_GRID::SampleSh(const GNg::sVEC3 &worldPos, float outSh9[9][4]) const {
    if(!aValid || !outSh9) return false;
    uint32_t x0 = 0, x1 = 0, y0 = 0, y1 = 0, z0 = 0, z1 = 0;
    float tx = 0.0f, ty = 0.0f, tz = 0.0f;
    gridCoord(worldPos.x, aDesc.aOrigin.x, aDesc.aSpacing, aDesc.aCountX, &x0, &x1, &tx);
    gridCoord(worldPos.y, aDesc.aOrigin.y, aDesc.aSpacing, aDesc.aCountY, &y0, &y1, &ty);
    gridCoord(worldPos.z, aDesc.aOrigin.z, aDesc.aSpacing, aDesc.aCountZ, &z0, &z1, &tz);
    const uint32_t ix[2] = {x0, x1};
    const uint32_t iy[2] = {y0, y1};
    const uint32_t iz[2] = {z0, z1};
    const float wx[2] = {1.0f - tx, tx};
    const float wy[2] = {1.0f - ty, ty};
    const float wz[2] = {1.0f - tz, tz};
    float result[PROBE_SH9_FLOAT_COUNT];
    for(int i = 0; i < PROBE_SH9_FLOAT_COUNT; ++i) result[i] = 0.0f;
    for(int k = 0; k < 2; ++k) {
        for(int j = 0; j < 2; ++j) {
            for(int i = 0; i < 2; ++i) {
                const float weight = wx[i] * wy[j] * wz[k];
                if(weight <= 0.0f) continue;
                const float *probe = aSh[probeIndex(ix[i], iy[j], iz[k])];
                for(int c = 0; c < PROBE_SH9_FLOAT_COUNT; ++c) result[c] += probe[c] * weight;
            }
        }
    }
    std::memcpy(outSh9, result, sizeof(result));
    return true;
}

bool cPROBE_GRID::Recenter(const GNg::sVEC3 &cameraPos) {
    if(!aValid) return false;
    const float spacing = aDesc.aSpacing;
    // 阈值 2×spacing：避免相机每走一格就重建（3.9m 不搬、4.1m 搬 的验收语义）。
    const float threshold = 2.0f * spacing;
    const float dx = cameraPos.x - aLastCamera.x;
    const float dy = cameraPos.y - aLastCamera.y;
    const float dz = cameraPos.z - aLastCamera.z;
    if(std::fabs(dx) < threshold && std::fabs(dy) < threshold && std::fabs(dz) < threshold) {
        return false;
    }
    // 新原点：让相机所在格中心落在网格中心索引处（保持整数格偏移）。
    const float halfX = static_cast<float>(aDesc.aCountX - 1u) * 0.5f * spacing;
    const float halfY = static_cast<float>(aDesc.aCountY - 1u) * 0.5f * spacing;
    const float halfZ = static_cast<float>(aDesc.aCountZ - 1u) * 0.5f * spacing;
    const float cellX = std::floor(cameraPos.x / spacing) * spacing + 0.5f * spacing;
    const float cellY = std::floor(cameraPos.y / spacing) * spacing + 0.5f * spacing;
    const float cellZ = std::floor(cameraPos.z / spacing) * spacing + 0.5f * spacing;
    const GNg::sVEC3 newOrigin = {cellX - halfX, cellY - halfY, cellZ - halfZ};
    // 整数格偏移（格数）与 SH9 搬移：newSh[i] = oldSh[i + shift]，越界夹到最近旧值。
    const long shiftX = std::lround((newOrigin.x - aDesc.aOrigin.x) / spacing);
    const long shiftY = std::lround((newOrigin.y - aDesc.aOrigin.y) / spacing);
    const long shiftZ = std::lround((newOrigin.z - aDesc.aOrigin.z) / spacing);
    std::vector<float> moved(static_cast<size_t>(PROBE_GRID_MAX_PROBES) * PROBE_SH9_FLOAT_COUNT);
    const long countX = static_cast<long>(aDesc.aCountX);
    const long countY = static_cast<long>(aDesc.aCountY);
    const long countZ = static_cast<long>(aDesc.aCountZ);
    for(long z = 0; z < countZ; ++z) {
        for(long y = 0; y < countY; ++y) {
            for(long x = 0; x < countX; ++x) {
                long sx = x + shiftX;
                long sy = y + shiftY;
                long sz = z + shiftZ;
                if(sx < 0) sx = 0;
                if(sx >= countX) sx = countX - 1;
                if(sy < 0) sy = 0;
                if(sy >= countY) sy = countY - 1;
                if(sz < 0) sz = 0;
                if(sz >= countZ) sz = countZ - 1;
                const uint32_t dst = probeIndex(static_cast<uint32_t>(x), static_cast<uint32_t>(y),
                                                static_cast<uint32_t>(z));
                const uint32_t src = probeIndex(static_cast<uint32_t>(sx), static_cast<uint32_t>(sy),
                                                static_cast<uint32_t>(sz));
                std::memcpy(&moved[static_cast<size_t>(dst) * PROBE_SH9_FLOAT_COUNT], aSh[src],
                            sizeof(aSh[src]));
            }
        }
    }
    std::memcpy(aSh, moved.data(), sizeof(aSh));
    aDesc.aOrigin = newOrigin;
    aLastCamera = cameraPos;
    return true;
}

// G5b-1（裁决 A）：9 张 8×4×8 RGBA16F 纹理布局（float 层）：
// out[((((k * countZ + z) * countY + y) * countX + x) * 4) + c]
// k = SH 系数索引 0..8，c = 该系数的 vec4 分量（RGB + padding）。
bool packProbeTextures(const cPROBE_GRID &grid, float *out) {
    if(!grid.IsValid() || !out) return false;
    const uint32_t countX = grid.GetCountX();
    const uint32_t countY = grid.GetCountY();
    const uint32_t countZ = grid.GetCountZ();
    for(uint32_t k = 0; k < 9u; ++k) {
        for(uint32_t z = 0; z < countZ; ++z) {
            for(uint32_t y = 0; y < countY; ++y) {
                for(uint32_t x = 0; x < countX; ++x) {
                    const uint32_t probe = (z * countY + y) * countX + x;
                    float sh36[PROBE_SH9_FLOAT_COUNT];
                    if(!grid.GetProbeSh(probe, sh36)) return false;
                    float *dst = out + ((((size_t)k * countZ + z) * countY + y) * countX + x) * 4u;
                    for(uint32_t c = 0; c < 4u; ++c) dst[c] = sh36[k * 4u + c];
                }
            }
        }
    }
    return true;
}

} // namespace GN3d
