// GN_3DSkin.cpp — CPU 线性混合蒙皮（LBS）与关节调色板打包（GPU 绑定留 P2 装配）。
//
// 约定：
//   * 权重先钳负数再归一化；总和 ≤ 1e-6 或调色板为空 ⇒ 原顶点（位置原样、法线归一化）。
//   * 关节索引按 paletteCount 钳制，坏数据不会越界读；只累加正权重关节，避免 NaN 调色板污染。
//   * 位置用完整 4x4（仿射，w 不做透视除法），法线只取左上 3x3 后归一化；零长度法线回退原法线。
#include <GN.h> // 先引入引擎基础类型（GN_2Dmath/GN_3Dmath 依赖 uint8/GN_API）
#include "GN_3DSkin.h"

#include <cmath>
#include <cstring>

namespace GN3d {
namespace {

// 权重和低于该值视为“无有效权重”，退化为原顶点。
const float WEIGHT_EPSILON = 1e-6f;

// 列主序 mat4 × 点（w = 1）：m[列*4 + 行]。
GNg::sVEC3 transformPoint(const GNg::sMAT44 &matrix, const GNg::sVEC3 &point) {
    return GNg::sVEC3{
        matrix.m[0] * point.x + matrix.m[4] * point.y + matrix.m[8] * point.z + matrix.m[12],
        matrix.m[1] * point.x + matrix.m[5] * point.y + matrix.m[9] * point.z + matrix.m[13],
        matrix.m[2] * point.x + matrix.m[6] * point.y + matrix.m[10] * point.z + matrix.m[14]};
}

// 列主序 mat4 的左上 3x3 × 方向（不带平移）。
GNg::sVEC3 transformDirection(const GNg::sMAT44 &matrix, const GNg::sVEC3 &direction) {
    return GNg::sVEC3{
        matrix.m[0] * direction.x + matrix.m[4] * direction.y + matrix.m[8] * direction.z,
        matrix.m[1] * direction.x + matrix.m[5] * direction.y + matrix.m[9] * direction.z,
        matrix.m[2] * direction.x + matrix.m[6] * direction.y + matrix.m[10] * direction.z};
}

// 归一化；长度退化（0/NaN/Inf）时返回 fallback，保证输出有限。
GNg::sVEC3 normalizeOr(const GNg::sVEC3 &value, const GNg::sVEC3 &fallback) {
    const float lengthSq = value.x * value.x + value.y * value.y + value.z * value.z;
    if(!(lengthSq > 1e-20f)) return fallback;
    const float invLength = 1.0f / std::sqrt(lengthSq);
    return GNg::sVEC3{value.x * invLength, value.y * invLength, value.z * invLength};
}

// 原顶点法线的安全回退：零长度时给 +Z，避免写出 NaN。
GNg::sVEC3 safeOriginalNormal(const GNg::sVEC3 &normal) {
    return normalizeOr(normal, GNg::sVEC3{0.0f, 0.0f, 1.0f});
}

} // namespace

bool skinVertices(const sSKINNED_VERTEX *in, uint32_t count, const GNg::sMAT44 *palette,
                  uint32_t paletteCount, GNg::sVEC3 *outPositions, GNg::sVEC3 *outNormals) {
    if(in == nullptr || outPositions == nullptr || outNormals == nullptr) return false;
    if(count == 0) return true;

    const bool hasPalette = palette != nullptr && paletteCount > 0;
    const uint32_t lastJoint = hasPalette ? paletteCount - 1 : 0;

    for(uint32_t index = 0; index < count; ++index) {
        const sSKINNED_VERTEX &vertex = in[index];
        float weights[4];
        float weightSum = 0.0f;
        for(uint32_t slot = 0; slot < 4; ++slot) {
            weights[slot] = vertex.aWeights[slot] > 0.0f ? vertex.aWeights[slot] : 0.0f;
            weightSum += weights[slot];
        }

        if(!hasPalette || !(weightSum > WEIGHT_EPSILON)) {
            outPositions[index] = vertex.aPosition;
            outNormals[index] = safeOriginalNormal(vertex.aNormal);
            continue;
        }

        const float invWeightSum = 1.0f / weightSum;
        float posX = 0.0f;
        float posY = 0.0f;
        float posZ = 0.0f;
        float nrmX = 0.0f;
        float nrmY = 0.0f;
        float nrmZ = 0.0f;
        for(uint32_t slot = 0; slot < 4; ++slot) {
            if(!(weights[slot] > 0.0f)) continue; // 跳过零权重槽位（其关节索引可能是脏值）
            uint32_t joint = vertex.aJoints[slot];
            if(joint > lastJoint) joint = lastJoint; // 越界钳制，坏数据不崩溃
            const float weight = weights[slot] * invWeightSum;
            const GNg::sVEC3 position = transformPoint(palette[joint], vertex.aPosition);
            const GNg::sVEC3 normal = transformDirection(palette[joint], vertex.aNormal);
            posX += position.x * weight;
            posY += position.y * weight;
            posZ += position.z * weight;
            nrmX += normal.x * weight;
            nrmY += normal.y * weight;
            nrmZ += normal.z * weight;
        }

        outPositions[index] = GNg::sVEC3{posX, posY, posZ};
        outNormals[index] = normalizeOr(GNg::sVEC3{nrmX, nrmY, nrmZ}, safeOriginalNormal(vertex.aNormal));
    }
    return true;
}

uint32_t packPalette(const GNg::sMAT44 *palette, uint32_t paletteCount, sSKIN_UBO &out) {
    std::memset(&out, 0, sizeof(out));
    if(palette == nullptr || paletteCount == 0) return 0;
    const uint32_t copyCount = paletteCount < SKIN_MAX_JOINTS ? paletteCount : SKIN_MAX_JOINTS;
    for(uint32_t joint = 0; joint < copyCount; ++joint) {
        for(uint32_t element = 0; element < 16; ++element) {
            out.aJointMatrices[joint][element] = palette[joint].m[element];
        }
    }
    return copyCount;
}

} // namespace GN3d
