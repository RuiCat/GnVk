// GN_MeshTools.cpp — 网格工具（纯 CPU）实现。
//
// 这里只有几何与字符串，不 include Vulkan、不读文件、不碰全局状态：
// 同一份输入永远得到同一份输出，所有统计走参数返回（便于并发与单测）。

#include "GN_MeshTools.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

namespace GN3d {

namespace {

// 位置量化精度：0.1 mm。与 FBX/Blender 导出的顶点重合判定一致。
const float kPositionQuantum = 10000.0f;
// 可量化位置上限：×kPositionQuantum = 2e9 < INT32_MAX，防 lround→int32 回绕把无关顶点焊在一起。
const float kMaxQuantizablePosition = 200000.0f;
// 长度小于它就当零（float 下 1e-12 已是"完全抵消"的量级）。
const float kEpsilon = 1.0e-12f;

// 量化的位置键：三个 int 的字典序。
struct sPOS_KEY {
    int32_t aX;
    int32_t aY;
    int32_t aZ;
    uint32_t aVertex;
};

bool posKeyLess(const sPOS_KEY &lhs, const sPOS_KEY &rhs) {
    if (lhs.aX != rhs.aX) return lhs.aX < rhs.aX;
    if (lhs.aY != rhs.aY) return lhs.aY < rhs.aY;
    if (lhs.aZ != rhs.aZ) return lhs.aZ < rhs.aZ;
    return lhs.aVertex < rhs.aVertex;
}

bool isFinite3(const GNg::sVEC3 &v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

char lowerAscii(char c) {
    return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
}

bool equalsIgnoreCase(const char *haystack, const char *needle, size_t needleLength) {
    if (needleLength == 0) {
        return true;
    }
    const size_t hayLength = std::strlen(haystack);
    if (needleLength > hayLength) {
        return false;
    }
    for (size_t start = 0; start + needleLength <= hayLength; ++start) {
        size_t offset = 0;
        while (offset < needleLength &&
               lowerAscii(haystack[start + offset]) == lowerAscii(needle[offset])) {
            ++offset;
        }
        if (offset == needleLength) {
            return true;
        }
    }
    return false;
}

} // namespace

// ═══════════════════════════════════════════════════════════
//  sVERTEX_FLOATS
// ═══════════════════════════════════════════════════════════

sVERTEX_FLOATS::sVERTEX_FLOATS() : aStride(11), aPositionOffset(0), aNormalOffset(3) {}

sVERTEX_FLOATS sVERTEX_FLOATS::Standard() {
    return sVERTEX_FLOATS();
}

sVERTEX_FLOATS sVERTEX_FLOATS::PositionOnly() {
    sVERTEX_FLOATS layout;
    layout.aStride = 3;
    layout.aPositionOffset = 0;
    layout.aNormalOffset = 3; // 越界 ⇒ HasNormal() 为 false
    return layout;
}

bool sVERTEX_FLOATS::IsValid() const {
    if (aStride == 0) {
        return false;
    }
    if (aPositionOffset + 3u > aStride) {
        return false;
    }
    if (aNormalOffset != 0xffffffffu && aNormalOffset + 3u > aStride) {
        return false;
    }
    return true;
}

bool sVERTEX_FLOATS::HasNormal() const {
    // 法线必须完整落在 stride 内，且不能和位置重叠（重叠说明这张表填错了）。
    return aNormalOffset != 0xffffffffu && aNormalOffset + 3u <= aStride &&
           aNormalOffset != aPositionOffset;
}

// ═══════════════════════════════════════════════════════════
//  sNORMAL_WELD_STATS
// ═══════════════════════════════════════════════════════════

sNORMAL_WELD_STATS::sNORMAL_WELD_STATS()
    : aWeldedGroups(0), aSmoothedGroups(0), aHardEdgeGroups(0), aDegenerateGroups(0),
      aAffectedVertices(0) {}

// ═══════════════════════════════════════════════════════════
//  一、按位置焊接法线
// ═══════════════════════════════════════════════════════════

bool weldNormalsByPosition(float *vertices, uint32_t vertexCount, const uint32_t *indices,
                           uint32_t indexCount, float thresholdCos, const sVERTEX_FLOATS &layout,
                           sNORMAL_WELD_STATS *stats) {
    if (stats != 0) {
        *stats = sNORMAL_WELD_STATS();
    }
    // ── 输入校验：任何一项不满足都**不修改数据** ──
    if (vertices == 0 || indices == 0) {
        return false;
    }
    if (vertexCount == 0 || indexCount < 3 || (indexCount % 3u) != 0) {
        return false;
    }
    if (!layout.IsValid() || !layout.HasNormal()) {
        return false;
    }
    const uint32_t triangleCount = indexCount / 3u;
    for (uint32_t i = 0; i < indexCount; ++i) {
        if (indices[i] >= vertexCount) {
            return false;
        }
    }
    // 位置必须有限：量化 lround 遇到 NaN 是未定义行为。
    for (uint32_t v = 0; v < vertexCount; ++v) {
        const float *p = &vertices[v * layout.aStride + layout.aPositionOffset];
        if (!std::isfinite(p[0]) || !std::isfinite(p[1]) || !std::isfinite(p[2])) {
            return false;
        }
        if (std::fabs(p[0]) > kMaxQuantizablePosition || std::fabs(p[1]) > kMaxQuantizablePosition ||
            std::fabs(p[2]) > kMaxQuantizablePosition) {
            return false; // 量化键会溢出 int32：不修改数据，直接拒绝
        }
    }

    // ── 1. 面法线 + 面积（叉积长度 = 2×面积，作权重足够）──
    std::vector<GNg::sVEC3> faceNormal(triangleCount);
    std::vector<float> faceArea(triangleCount);
    for (uint32_t t = 0; t < triangleCount; ++t) {
        const float *p0 = &vertices[indices[t * 3u + 0] * layout.aStride + layout.aPositionOffset];
        const float *p1 = &vertices[indices[t * 3u + 1] * layout.aStride + layout.aPositionOffset];
        const float *p2 = &vertices[indices[t * 3u + 2] * layout.aStride + layout.aPositionOffset];
        const float ax = p1[0] - p0[0], ay = p1[1] - p0[1], az = p1[2] - p0[2];
        const float bx = p2[0] - p0[0], by = p2[1] - p0[1], bz = p2[2] - p0[2];
        const float nx = ay * bz - az * by;
        const float ny = az * bx - ax * bz;
        const float nz = ax * by - ay * bx;
        const float length = std::sqrt(nx * nx + ny * ny + nz * nz);
        faceArea[t] = length;
        if (length > kEpsilon) {
            faceNormal[t] = GNg::sVEC3{nx / length, ny / length, nz / length};
        } else {
            faceNormal[t] = GNg::sVEC3{0.0f, 0.0f, 0.0f}; // 退化三角形：不提供方向
        }
    }

    // ── 2. 顶点 → 相邻面（CSR，避免 vector<vector> 的每顶点一次堆分配）──
    std::vector<uint32_t> incidentBegin(vertexCount + 1u, 0);
    for (uint32_t i = 0; i < indexCount; ++i) {
        incidentBegin[indices[i] + 1u] += 1u;
    }
    for (uint32_t v = 0; v < vertexCount; ++v) {
        incidentBegin[v + 1u] += incidentBegin[v];
    }
    std::vector<uint32_t> incidentFaces(incidentBegin[vertexCount]);
    std::vector<uint32_t> cursor(incidentBegin.begin(), incidentBegin.end() - 1);
    for (uint32_t t = 0; t < triangleCount; ++t) {
        for (uint32_t corner = 0; corner < 3u; ++corner) {
            const uint32_t vertex = indices[t * 3u + corner];
            incidentFaces[cursor[vertex]++] = t;
        }
    }

    // ── 3. 按量化位置分组（排序后取连续段：无哈希表、无每组建 vector）──
    std::vector<sPOS_KEY> keyed(vertexCount);
    for (uint32_t v = 0; v < vertexCount; ++v) {
        const float *p = &vertices[v * layout.aStride + layout.aPositionOffset];
        keyed[v].aX = static_cast<int32_t>(std::lround(p[0] * kPositionQuantum));
        keyed[v].aY = static_cast<int32_t>(std::lround(p[1] * kPositionQuantum));
        keyed[v].aZ = static_cast<int32_t>(std::lround(p[2] * kPositionQuantum));
        keyed[v].aVertex = v;
    }
    std::sort(keyed.begin(), keyed.end(), posKeyLess);

    // ── 4. 逐组求平均法线 ──
    // 面的收集顺序必须**按下标升序**：浮点求和不可交换，顺序变了结果就有 ulp 级差异。
    // 排序 + 去重同时解决"同一面被同组的多个顶点引用"的重复计入。
    std::vector<uint32_t> faces; // 跨组复用，避免每组一次分配
    sNORMAL_WELD_STATS local;
    uint32_t groupBegin = 0;
    while (groupBegin < vertexCount) {
        uint32_t groupEnd = groupBegin + 1u;
        while (groupEnd < vertexCount && keyed[groupEnd].aX == keyed[groupBegin].aX &&
               keyed[groupEnd].aY == keyed[groupBegin].aY &&
               keyed[groupEnd].aZ == keyed[groupBegin].aZ) {
            ++groupEnd;
        }
        const uint32_t members = groupEnd - groupBegin;
        if (members >= 2u) {
            ++local.aWeldedGroups;
            faces.clear();
            for (uint32_t m = groupBegin; m < groupEnd; ++m) {
                const uint32_t vertex = keyed[m].aVertex;
                for (uint32_t slot = incidentBegin[vertex]; slot < incidentBegin[vertex + 1u]; ++slot) {
                    faces.push_back(incidentFaces[slot]);
                }
            }
            std::sort(faces.begin(), faces.end());
            faces.erase(std::unique(faces.begin(), faces.end()), faces.end());

            GNg::sVEC3 sum{0.0f, 0.0f, 0.0f};
            for (size_t f = 0; f < faces.size(); ++f) {
                const uint32_t triangle = faces[f];
                const float weight = faceArea[triangle];
                sum.x += faceNormal[triangle].x * weight;
                sum.y += faceNormal[triangle].y * weight;
                sum.z += faceNormal[triangle].z * weight;
            }
            const float length = std::sqrt(sum.x * sum.x + sum.y * sum.y + sum.z * sum.z);
            if (length <= kEpsilon) {
                ++local.aDegenerateGroups; // 法线互相抵消（薄片双面）⇒ 没有可写的东西
            } else {
                const GNg::sVEC3 average{sum.x / length, sum.y / length, sum.z / length};
                bool hardEdge = false;
                for (size_t f = 0; f < faces.size(); ++f) {
                    const GNg::sVEC3 &normal = faceNormal[faces[f]];
                    const float dot =
                        normal.x * average.x + normal.y * average.y + normal.z * average.z;
                    if (dot < thresholdCos) {
                        hardEdge = true;
                        break;
                    }
                }
                if (hardEdge) {
                    ++local.aHardEdgeGroups;
                } else {
                    for (uint32_t m = groupBegin; m < groupEnd; ++m) {
                        const uint32_t vertex = keyed[m].aVertex;
                        float *normal = &vertices[vertex * layout.aStride + layout.aNormalOffset];
                        normal[0] = average.x;
                        normal[1] = average.y;
                        normal[2] = average.z;
                    }
                    ++local.aSmoothedGroups;
                    local.aAffectedVertices += members;
                }
            }
        }
        groupBegin = groupEnd;
    }

    if (stats != 0) {
        *stats = local;
    }
    return local.aSmoothedGroups > 0;
}

// ═══════════════════════════════════════════════════════════
//  二、顶点质心
// ═══════════════════════════════════════════════════════════

bool centroidOfVertices(const float *vertices, uint32_t vertexCount, const sVERTEX_FLOATS &layout,
                        GNg::sVEC3 *out) {
    if (vertices == 0 || out == 0 || vertexCount == 0 || !layout.IsValid()) {
        return false;
    }
    GNg::sVEC3 sum{0.0f, 0.0f, 0.0f};
    for (uint32_t v = 0; v < vertexCount; ++v) {
        const float *p = &vertices[v * layout.aStride + layout.aPositionOffset];
        sum.x += p[0];
        sum.y += p[1];
        sum.z += p[2];
    }
    const float inverseCount = 1.0f / static_cast<float>(vertexCount);
    const GNg::sVEC3 centroid{sum.x * inverseCount, sum.y * inverseCount, sum.z * inverseCount};
    if (!isFinite3(centroid)) {
        return false;
    }
    *out = centroid;
    return true;
}

// ═══════════════════════════════════════════════════════════
//  三、最近点
// ═══════════════════════════════════════════════════════════

int32_t nearestPointIndex(const GNg::sVEC3 &point, const GNg::sVEC3 *points, uint32_t count) {
    if (points == 0 || count == 0) {
        return -1;
    }
    int32_t best = -1;
    float bestDistance = 0.0f;
    for (uint32_t i = 0; i < count; ++i) {
        const float dx = points[i].x - point.x;
        const float dy = points[i].y - point.y;
        const float dz = points[i].z - point.z;
        const float distance = dx * dx + dy * dy + dz * dz;
        if (!std::isfinite(distance)) {
            continue; // 关节位置坏了就跳过，别让它靠"无穷远"赢下所有比较
        }
        // 严格小于：并列时保留下标较小者。
        if (best < 0 || distance < bestDistance) {
            best = static_cast<int32_t>(i);
            bestDistance = distance;
        }
    }
    return best;
}

// ═══════════════════════════════════════════════════════════
//  四、逆绑定矩阵 → 关节位置
// ═══════════════════════════════════════════════════════════

bool jointPositionFromInverseBind(const GNg::sMAT44 &inverseBind, GNg::sVEC3 *out) {
    if (out == 0) {
        return false;
    }
    const float *m = inverseBind.m;
    const GNg::sVEC3 translation{m[12], m[13], m[14]};
    const GNg::sVEC3 result{
        -(m[0] * translation.x + m[4] * translation.y + m[8] * translation.z),
        -(m[1] * translation.x + m[5] * translation.y + m[9] * translation.z),
        -(m[2] * translation.x + m[6] * translation.y + m[10] * translation.z)};
    if (!isFinite3(result)) {
        return false;
    }
    *out = result;
    return true;
}

// ═══════════════════════════════════════════════════════════
//  五、alpha 覆盖率 → 模式
// ═══════════════════════════════════════════════════════════

eALPHA_MODE resolveAlphaModeByCoverage(bool hasAlpha, float partialAlphaRatio, eALPHA_MODE source,
                                       float blendThreshold) {
    if (source == eALPHA_OPAQUE) {
        return eALPHA_OPAQUE; // 资产说它不透明，就不擅自升级
    }
    if (!hasAlpha) {
        return eALPHA_OPAQUE; // 贴图全不透明：混合管线只会让远侧盖住近侧
    }
    if (!std::isfinite(partialAlphaRatio) || partialAlphaRatio <= 0.0f) {
        // 有 alpha 通道但没有任何半透明像素 ⇒ 纯二值镂空。
        return eALPHA_MASK;
    }
    if (std::isfinite(blendThreshold) && partialAlphaRatio > blendThreshold) {
        return eALPHA_BLEND;
    }
    return eALPHA_MASK;
}

// ═══════════════════════════════════════════════════════════
//  六、网格名过滤
// ═══════════════════════════════════════════════════════════

bool matchNameFilter(const char *name, const char *filter) {
    if (filter == 0 || filter[0] == '\0') {
        return true; // 空过滤 = 全通过
    }
    if (name == 0) {
        return false;
    }
    const size_t filterLength = std::strlen(filter);
    size_t cursor = 0;
    while (cursor < filterLength) {
        // 跳过逗号与空白
        while (cursor < filterLength && (filter[cursor] == ',' || filter[cursor] == ' ' ||
                                         filter[cursor] == '\t')) {
            ++cursor;
        }
        const size_t begin = cursor;
        while (cursor < filterLength && filter[cursor] != ',') {
            ++cursor;
        }
        size_t end = cursor;
        while (end > begin && (filter[end - 1] == ' ' || filter[end - 1] == '\t')) {
            --end;
        }
        if (end > begin) {
            if (equalsIgnoreCase(name, &filter[begin], end - begin)) {
                return true;
            }
        }
    }
    // 只有逗号/空白的 filter 是笔误：不命中任何网格，暴露问题
    return false;
}

} // namespace GN3d
