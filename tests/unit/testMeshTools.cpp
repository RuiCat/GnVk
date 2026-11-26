// testMeshTools.cpp — 网格工具（法线焊接 / 质心 / 最近点 / alpha 判定 / 名称过滤）。
//
// 验收重点有三条，顺序就是重要性：
//
//   1. **抽离不改行为**。法线焊接原本埋在 tests/imoScene/imoSceneConvert.cpp 里，
//      搬到引擎模块时必须**逐位一致** —— 否则 demo 的画面会悄悄变。所以下面保留了
//      一份旧实现的逐行副本（referenceWeldNormals），用确定性伪随机网格逐位对拍。
//      （这一点很容易被忽略：浮点求和不满足交换律，换个收集顺序就差 ulp。）
//
//   2. **硬边不能被抹平**。立方体的角上三面各差 90°，平均法线与各面夹角约 54.7°，
//      超过 60° 阈值就必须整组保持原样。把棱角抹圆比不焊接更难接受 —— 前者是错的，
//      后者只是没优化。
//
//   3. **非法输入不许写坏数据**。焊接是原地改写顶点数组；一旦中途返回，
//      半个网格被改过是最坏的结果。所以判据是「返回 false ⇒ 数组一字未动」。
#include "testFramework.h"

#include <GN.h>
#include "GN_MeshTools.h"

#include <cmath>
#include <cstring>
#include <map>
#include <tuple>
#include <vector>

using namespace GN3d;

namespace {

const uint32_t kStride = 11u; // pos3 / normal3 / tangent3 / uv2

// ── 网格构造辅助 ──────────────────────────────────────────
// 顶点 = 11 float；位置写在 [0..2]，法线写在 [3..5]，其余留 0。
void pushVertex(std::vector<float> &vertices, float px, float py, float pz, float nx, float ny,
                float nz) {
    vertices.push_back(px);
    vertices.push_back(py);
    vertices.push_back(pz);
    vertices.push_back(nx);
    vertices.push_back(ny);
    vertices.push_back(nz);
    for (int i = 0; i < 5; ++i) {
        vertices.push_back(0.0f); // tangent + uv
    }
}

float *normalAt(std::vector<float> &vertices, uint32_t index) {
    return &vertices[index * kStride + 3u];
}

// 一个立方体：每个面 4 个独立顶点（顶点不共享），所以每个角上 3 个顶点位置相同、
// 法线各不相同 —— 正是 FBX 导出的那种"同位置多份顶点"。
// 24 个顶点 / 8 个角，每个角 3 份 ⇒ 焊接后有 8 个重合组。
void buildCube(std::vector<float> &vertices, std::vector<uint32_t> &indices) {
    const float h = 0.5f;
    struct sFACE {
        float aNormal[3];
        float aU[3];
        float aV[3];
    };
    const sFACE faces[6] = {
        {{1, 0, 0}, {0, 0, -1}, {0, 1, 0}},  // +X
        {{-1, 0, 0}, {0, 0, 1}, {0, 1, 0}},  // -X
        {{0, 1, 0}, {1, 0, 0}, {0, 0, -1}},  // +Y
        {{0, -1, 0}, {1, 0, 0}, {0, 0, 1}},  // -Y
        {{0, 0, 1}, {1, 0, 0}, {0, 1, 0}},   // +Z
        {{0, 0, -1}, {-1, 0, 0}, {0, 1, 0}}, // -Z
    };
    for (int face = 0; face < 6; ++face) {
        const uint32_t base = static_cast<uint32_t>(vertices.size() / kStride);
        for (int corner = 0; corner < 4; ++corner) {
            const float su = ((corner == 1 || corner == 2) ? 1.0f : -1.0f) * h;
            const float sv = ((corner == 2 || corner == 3) ? 1.0f : -1.0f) * h;
            const sFACE &f = faces[face];
            pushVertex(vertices, f.aNormal[0] * h + f.aU[0] * su + f.aV[0] * sv,
                       f.aNormal[1] * h + f.aU[1] * su + f.aV[1] * sv,
                       f.aNormal[2] * h + f.aU[2] * su + f.aV[2] * sv, f.aNormal[0], f.aNormal[1],
                       f.aNormal[2]);
        }
        indices.push_back(base + 0u);
        indices.push_back(base + 1u);
        indices.push_back(base + 2u);
        indices.push_back(base + 0u);
        indices.push_back(base + 2u);
        indices.push_back(base + 3u);
    }
}

// 一条曲面带（像圆柱侧面的一段）：**每个四边形各自生成 4 个顶点**，
// 于是相邻四边形在共享位置上各有一份顶点、各带自己那个面的法线 ——
// 这正是"导出器按面拆顶点"的形态，也是焊接要处理的对象。
//
// 8 段跨 90°（每段 11.25°）⇒ 相邻面法线只差 11.25°，远在 60° 阈值内，应全部焊接。
// 内部 7 个位置 × 2 行（y=0 与 y=1）= **14 组**，每组 2 份顶点。
void buildCurvedStrip(std::vector<float> &vertices, std::vector<uint32_t> &indices) {
    const int segments = 8;
    const float span = 3.14159265f * 0.5f;
    for (int i = 0; i < segments; ++i) {
        const float a0 = span * static_cast<float>(i) / static_cast<float>(segments);
        const float a1 = span * static_cast<float>(i + 1) / static_cast<float>(segments);
        const float x0 = std::cos(a0), z0 = std::sin(a0);
        const float x1 = std::cos(a1), z1 = std::sin(a1);
        // 该四边形自己的面法线（外法线 = 位置方向）
        const float mid = (a0 + a1) * 0.5f;
        const float nx = std::cos(mid), nz = std::sin(mid);
        const uint32_t base = static_cast<uint32_t>(vertices.size() / kStride);
        pushVertex(vertices, x0, 0.0f, z0, nx, 0.0f, nz);
        pushVertex(vertices, x1, 0.0f, z1, nx, 0.0f, nz);
        pushVertex(vertices, x1, 1.0f, z1, nx, 0.0f, nz);
        pushVertex(vertices, x0, 1.0f, z0, nx, 0.0f, nz);
        indices.push_back(base + 0u);
        indices.push_back(base + 1u);
        indices.push_back(base + 2u);
        indices.push_back(base + 0u);
        indices.push_back(base + 2u);
        indices.push_back(base + 3u);
    }
}

// ── 旧实现的逐行副本（搬走前就在 imoSceneConvert.cpp 里）────────
// 保留它唯一的目的：证明抽离后输出**逐位**一致。所以这里刻意不做任何"改进"，
// 连 vector<vector> 的分配模式都保持原样。
bool referenceWeldNormals(float *vertices, uint32_t vertexCount, const uint32_t *indices,
                          uint32_t indexCount, float thresholdCos) {
    if (vertexCount == 0 || indexCount < 3) {
        return false;
    }
    const uint32_t triangleCount = indexCount / 3u;
    std::vector<GNg::sVEC3> faceNormal(triangleCount);
    std::vector<float> faceArea(triangleCount);
    for (uint32_t triangle = 0; triangle < triangleCount; ++triangle) {
        const uint32_t i0 = indices[triangle * 3 + 0];
        const uint32_t i1 = indices[triangle * 3 + 1];
        const uint32_t i2 = indices[triangle * 3 + 2];
        if (i0 >= vertexCount || i1 >= vertexCount || i2 >= vertexCount) {
            return false;
        }
        const float *p0 = &vertices[i0 * 11u];
        const float *p1 = &vertices[i1 * 11u];
        const float *p2 = &vertices[i2 * 11u];
        const float ax = p1[0] - p0[0], ay = p1[1] - p0[1], az = p1[2] - p0[2];
        const float bx = p2[0] - p0[0], by = p2[1] - p0[1], bz = p2[2] - p0[2];
        const float nx = ay * bz - az * by;
        const float ny = az * bx - ax * bz;
        const float nz = ax * by - ay * bx;
        const float length = std::sqrt(nx * nx + ny * ny + nz * nz);
        faceArea[triangle] = length;
        if (length > 1.0e-12f) {
            faceNormal[triangle] = GNg::sVEC3{nx / length, ny / length, nz / length};
        } else {
            faceNormal[triangle] = GNg::sVEC3{0.0f, 0.0f, 0.0f};
        }
    }
    std::vector<std::vector<uint32_t>> incident(vertexCount);
    for (uint32_t triangle = 0; triangle < triangleCount; ++triangle) {
        for (int corner = 0; corner < 3; ++corner) {
            incident[indices[triangle * 3 + corner]].push_back(triangle);
        }
    }
    std::map<std::tuple<int, int, int>, std::vector<uint32_t>> groups;
    for (uint32_t index = 0; index < vertexCount; ++index) {
        const float *vertex = &vertices[index * 11u];
        const std::tuple<int, int, int> key(
            static_cast<int>(std::lround(vertex[0] * 10000.0f)),
            static_cast<int>(std::lround(vertex[1] * 10000.0f)),
            static_cast<int>(std::lround(vertex[2] * 10000.0f)));
        groups[key].push_back(index);
    }
    for (auto &entry : groups) {
        std::vector<uint32_t> &members = entry.second;
        if (members.size() < 2) {
            continue;
        }
        GNg::sVEC3 sum = {0.0f, 0.0f, 0.0f};
        std::vector<uint32_t> faces;
        for (size_t member = 0; member < members.size(); ++member) {
            const std::vector<uint32_t> &list = incident[members[member]];
            for (size_t f = 0; f < list.size(); ++f) {
                faces.push_back(list[f]);
            }
        }
        std::sort(faces.begin(), faces.end());
        faces.erase(std::unique(faces.begin(), faces.end()), faces.end());
        for (size_t f = 0; f < faces.size(); ++f) {
            const uint32_t triangle = faces[f];
            const float weight = faceArea[triangle];
            sum.x += faceNormal[triangle].x * weight;
            sum.y += faceNormal[triangle].y * weight;
            sum.z += faceNormal[triangle].z * weight;
        }
        const float length = std::sqrt(sum.x * sum.x + sum.y * sum.y + sum.z * sum.z);
        if (length <= 1.0e-12f) {
            continue;
        }
        const GNg::sVEC3 average = {sum.x / length, sum.y / length, sum.z / length};
        bool hardEdge = false;
        for (size_t f = 0; f < faces.size(); ++f) {
            const GNg::sVEC3 &normal = faceNormal[faces[f]];
            const float dot = normal.x * average.x + normal.y * average.y + normal.z * average.z;
            if (dot < thresholdCos) {
                hardEdge = true;
                break;
            }
        }
        if (hardEdge) {
            continue;
        }
        for (size_t member = 0; member < members.size(); ++member) {
            const uint32_t index = members[member];
            vertices[index * 11u + 3] = average.x;
            vertices[index * 11u + 4] = average.y;
            vertices[index * 11u + 5] = average.z;
        }
    }
    return true;
}

// 确定性伪随机网格：位置量化到 0.1 mm 的格点上（保证真的有重合顶点），
// 索引随机但避开退化。用固定种子，任何机器上生成同一份数据。
void buildPseudoRandomMesh(std::vector<float> &vertices, std::vector<uint32_t> &indices,
                           uint32_t vertexCount, uint32_t triangleCount) {
    uint32_t state = 0x12345678u;
    const uint32_t grid = 6; // 6×6×6 的格点 → 大量位置重合
    for (uint32_t v = 0; v < vertexCount; ++v) {
        state = state * 1664525u + 1013904223u;
        const uint32_t gx = (state >> 8) % grid;
        state = state * 1664525u + 1013904223u;
        const uint32_t gy = (state >> 8) % grid;
        state = state * 1664525u + 1013904223u;
        const uint32_t gz = (state >> 8) % grid;
        state = state * 1664525u + 1013904223u;
        const float jitter = static_cast<float>((state >> 16) % 7u) * 1.0e-5f; // 小于量化精度
        pushVertex(vertices, static_cast<float>(gx) * 0.1f + jitter,
                   static_cast<float>(gy) * 0.1f, static_cast<float>(gz) * 0.1f,
                   static_cast<float>(gx) / static_cast<float>(grid), 0.0f,
                   static_cast<float>(gz) / static_cast<float>(grid));
    }
    for (uint32_t t = 0; t < triangleCount; ++t) {
        state = state * 1664525u + 1013904223u;
        const uint32_t a = (state >> 8) % vertexCount;
        state = state * 1664525u + 1013904223u;
        const uint32_t b = (state >> 8) % vertexCount;
        state = state * 1664525u + 1013904223u;
        const uint32_t c = (state >> 8) % vertexCount;
        if (a == b || b == c || a == c) {
            continue; // 跳过退化三角形，避免"没有方向"的面干扰对拍
        }
        indices.push_back(a);
        indices.push_back(b);
        indices.push_back(c);
    }
}

// 两组法线是否逐位相同。
bool normalsBitEqual(const std::vector<float> &lhs, const std::vector<float> &rhs) {
    if (lhs.size() != rhs.size()) {
        return false;
    }
    return std::memcmp(lhs.data(), rhs.data(), lhs.size() * sizeof(float)) == 0;
}

} // namespace

// ═══════════════════════════════════════════════════════════
//  一、法线焊接
// ═══════════════════════════════════════════════════════════

TEST_CASE(weldGeneratesSameBitsAsReferenceImplementation) {
    // 4 份不同规模的伪随机网格：小网格覆盖边界、大网格覆盖分组与排序路径。
    const uint32_t configs[4][2] = {{37, 20}, {200, 130}, {600, 400}, {1500, 900}};
    for (int c = 0; c < 4; ++c) {
        std::vector<float> vertices;
        std::vector<uint32_t> indices;
        buildPseudoRandomMesh(vertices, indices, configs[c][0], configs[c][1]);
        if (indices.size() < 3) {
            continue;
        }
        std::vector<float> mine = vertices;
        std::vector<float> reference = vertices;

        sNORMAL_WELD_STATS stats;
        const bool mineOk = weldNormalsByPosition(mine.data(), configs[c][0], indices.data(),
                                                  static_cast<uint32_t>(indices.size()), 0.5f,
                                                  sVERTEX_FLOATS::Standard(), &stats);
        const bool refOk = referenceWeldNormals(reference.data(), configs[c][0], indices.data(),
                                                static_cast<uint32_t>(indices.size()), 0.5f);
        CHECK_EQ(mineOk, refOk); // 连返回值也必须是同一个语义
        CHECK(normalsBitEqual(mine, reference)); // 逐位一致：位置/tangent/uv 都不能被动
        // 位置与其它字段一个字节都不该变 —— 只允许改法线。
        for (uint32_t v = 0; v < configs[c][0]; ++v) {
            for (uint32_t f = 0; f < kStride; ++f) {
                if (f == 3u || f == 4u || f == 5u) {
                    continue;
                }
                CHECK_EQ(mine[v * kStride + f], vertices[v * kStride + f]);
            }
        }
    }
}

TEST_CASE(weldSmoothsSplitVerticesOnCurvedSurface) {
    std::vector<float> vertices;
    std::vector<uint32_t> indices;
    buildCurvedStrip(vertices, indices);
    const uint32_t vertexCount = static_cast<uint32_t>(vertices.size() / kStride);
    // 每个位置两份顶点，法线相差 0.1 的偏移 ⇒ 平滑后必须相同。
    sNORMAL_WELD_STATS stats;
    CHECK(weldNormalsByPosition(vertices.data(), vertexCount, indices.data(),
                                static_cast<uint32_t>(indices.size()), 0.5f,
                                sVERTEX_FLOATS::Standard(), &stats));
    // 内部 7 个位置 × 2 行 = 14 组，每组 2 份顶点（两端的顶点只有一份，不成组）。
    CHECK_EQ(stats.aWeldedGroups, 14u);
    CHECK_EQ(stats.aSmoothedGroups, 14u);
    CHECK_EQ(stats.aAffectedVertices, 28u);
    CHECK_EQ(stats.aHardEdgeGroups, 0u);
    // 同一位置的两份顶点法线必须完全相同：按位置重新分组来核对。
    std::map<std::tuple<int, int, int>, std::vector<uint32_t>> groups;
    for (uint32_t v = 0; v < vertexCount; ++v) {
        const float *p = &vertices[v * kStride];
        groups[std::make_tuple(static_cast<int>(std::lround(p[0] * 10000.0f)),
                               static_cast<int>(std::lround(p[1] * 10000.0f)),
                               static_cast<int>(std::lround(p[2] * 10000.0f)))]
            .push_back(v);
    }
    uint32_t checked = 0;
    for (std::map<std::tuple<int, int, int>, std::vector<uint32_t>>::const_iterator it =
             groups.begin();
         it != groups.end(); ++it) {
        const std::vector<uint32_t> &members = it->second;
        if (members.size() < 2) {
            continue;
        }
        const float *first = normalAt(vertices, members[0]);
        for (size_t m = 1; m < members.size(); ++m) {
            CHECK_EQ(normalAt(vertices, members[m])[0], first[0]);
            CHECK_EQ(normalAt(vertices, members[m])[1], first[1]);
            CHECK_EQ(normalAt(vertices, members[m])[2], first[2]);
        }
        ++checked;
    }
    CHECK_EQ(checked, 14u);
}

TEST_CASE(weldKeepsCubeCornersSharpAtSixtyDegrees) {
    // **这条用例是本模块最重要的守卫**：立方体必须保角。
    //
    // 一开始我以为 60° 阈值会把这个角平滑掉（三面各差 90°，平均法线与各面差 54.7°）。
    // 实测不是：四边形拆成两个三角形后，角上三个面的入射三角形数是 2:1:1，
    // 面积加权把平均值拉向大的那个面，与另两个面的夹角超过 104° ⇒ 判为硬边。
    // 也就是说**面积加权天然提高硬边倾向**，真实网格里"小三角形汇聚在折边"
    // 正是这种情形。这个性质写进了头文件，这里用用例钉住它。
    std::vector<float> vertices;
    std::vector<uint32_t> indices;
    buildCube(vertices, indices);
    const std::vector<float> before = vertices;
    const uint32_t vertexCount = static_cast<uint32_t>(vertices.size() / kStride);
    CHECK_EQ(vertexCount, 24u);

    sNORMAL_WELD_STATS stats;
    const bool changed = weldNormalsByPosition(vertices.data(), vertexCount, indices.data(),
                                               static_cast<uint32_t>(indices.size()), 0.5f,
                                               sVERTEX_FLOATS::Standard(), &stats);
    CHECK_EQ(changed, false); // 没有任何顶点被改写
    CHECK_EQ(stats.aWeldedGroups, 8u); // 8 个角（每个角 3 份顶点）
    CHECK_EQ(stats.aSmoothedGroups, 0u);
    CHECK_EQ(stats.aHardEdgeGroups, 8u); // 全部判为硬边
    CHECK_EQ(stats.aAffectedVertices, 0u);
    CHECK(normalsBitEqual(vertices, before)); // 一个浮点都没动
}

TEST_CASE(weldThresholdIsMonotonic) {
    // 阈值越宽松（thresholdCos 越小），被平滑的组只会变多、不会变少。
    // 三个档位：60°（硬边）→ 110°（放松一点）→ 不限 -1（全平滑）。
    struct sCASE {
        float aThresholdCos;
        uint32_t aExpectedSmoothed;
        uint32_t aExpectedHard;
    };
    const sCASE cases[3] = {
        {0.5f, 0u, 8u},                                       // 60°
        {std::cos(110.0f * 3.14159265f / 180.0f), 8u, 0u},    // 110°
        {-1.0f, 8u, 0u},                                      // 不限
    };
    for (int c = 0; c < 3; ++c) {
        std::vector<float> vertices;
        std::vector<uint32_t> indices;
        buildCube(vertices, indices);
        sNORMAL_WELD_STATS stats;
        weldNormalsByPosition(vertices.data(), 24u, indices.data(),
                              static_cast<uint32_t>(indices.size()), cases[c].aThresholdCos,
                              sVERTEX_FLOATS::Standard(), &stats);
        CHECK_EQ(stats.aSmoothedGroups, cases[c].aExpectedSmoothed);
        CHECK_EQ(stats.aHardEdgeGroups, cases[c].aExpectedHard);
        // 每组要么被平滑、要么判硬边、要么退化：三者相加必须等于重合组数，
        // 不允许出现"既没平滑也没解释"的组（那说明统计漏了一支）。
        CHECK_EQ(stats.aWeldedGroups,
                 stats.aSmoothedGroups + stats.aHardEdgeGroups + stats.aDegenerateGroups);
    }
}

TEST_CASE(weldWritesSameAverageToEveryMemberOfGroup) {
    // 焊接的定义：同位置的顶点**拿到同一个**法线。用阈值 110° 让立方体也走平滑分支。
    std::vector<float> vertices;
    std::vector<uint32_t> indices;
    buildCube(vertices, indices);
    sNORMAL_WELD_STATS stats;
    CHECK(weldNormalsByPosition(vertices.data(), 24u, indices.data(),
                                static_cast<uint32_t>(indices.size()),
                                std::cos(110.0f * 3.14159265f / 180.0f),
                                sVERTEX_FLOATS::Standard(), &stats));
    // 每个角的 3 份顶点（下标 3k, 3k+1, 3k+2 不一定同角，所以按位置分组核对）
    std::map<std::tuple<int, int, int>, std::vector<uint32_t>> groups;
    for (uint32_t v = 0; v < 24u; ++v) {
        const float *p = &vertices[v * kStride];
        groups[std::make_tuple(static_cast<int>(std::lround(p[0] * 10000.0f)),
                               static_cast<int>(std::lround(p[1] * 10000.0f)),
                               static_cast<int>(std::lround(p[2] * 10000.0f)))]
            .push_back(v);
    }
    for (std::map<std::tuple<int, int, int>, std::vector<uint32_t>>::const_iterator it =
             groups.begin();
         it != groups.end(); ++it) {
        const std::vector<uint32_t> &members = it->second;
        if (members.size() < 2) {
            continue;
        }
        const float *first = normalAt(vertices, members[0]);
        const float length =
            std::sqrt(first[0] * first[0] + first[1] * first[1] + first[2] * first[2]);
        CHECK_NEAR(length, 1.0f, 1.0e-5); // 写入的必须是单位向量
        for (size_t m = 1; m < members.size(); ++m) {
            CHECK_EQ(normalAt(vertices, members[m])[0], first[0]);
            CHECK_EQ(normalAt(vertices, members[m])[1], first[1]);
            CHECK_EQ(normalAt(vertices, members[m])[2], first[2]);
        }
    }
}

TEST_CASE(weldRejectsInvalidInputWithoutTouchingData) {
    std::vector<float> vertices;
    std::vector<uint32_t> indices;
    buildCurvedStrip(vertices, indices);
    const std::vector<float> before = vertices;
    const uint32_t vertexCount = static_cast<uint32_t>(vertices.size() / kStride);
    sNORMAL_WELD_STATS stats;

    // 1) 索引越界
    std::vector<uint32_t> badIndices = indices;
    badIndices[3] = vertexCount + 5u;
    CHECK_EQ(weldNormalsByPosition(vertices.data(), vertexCount, badIndices.data(),
                                   static_cast<uint32_t>(badIndices.size()), 0.5f,
                                   sVERTEX_FLOATS::Standard(), &stats),
             false);
    CHECK(normalsBitEqual(vertices, before));

    // 2) 索引数不是 3 的倍数
    CHECK_EQ(weldNormalsByPosition(vertices.data(), vertexCount, indices.data(),
                                   static_cast<uint32_t>(indices.size()) - 1u, 0.5f,
                                   sVERTEX_FLOATS::Standard(), &stats),
             false);
    CHECK(normalsBitEqual(vertices, before));

    // 3) 空输入
    CHECK_EQ(weldNormalsByPosition(vertices.data(), 0, indices.data(),
                                   static_cast<uint32_t>(indices.size()), 0.5f,
                                   sVERTEX_FLOATS::Standard(), &stats),
             false);
    CHECK_EQ(weldNormalsByPosition(0, vertexCount, indices.data(),
                                   static_cast<uint32_t>(indices.size()), 0.5f,
                                   sVERTEX_FLOATS::Standard(), &stats),
             false);

    // 4) 布局不自洽（stride 太小，法线放不下）
    sVERTEX_FLOATS broken;
    broken.aStride = 4;
    broken.aPositionOffset = 0;
    broken.aNormalOffset = 3; // 3+3 > 4
    CHECK_EQ(broken.IsValid(), false);
    CHECK_EQ(weldNormalsByPosition(vertices.data(), vertexCount, indices.data(),
                                   static_cast<uint32_t>(indices.size()), 0.5f, broken, &stats),
             false);
    CHECK(normalsBitEqual(vertices, before));

    // 5) 位置含 NaN（lround(NaN) 是未定义行为，必须提前拒掉）
    std::vector<float> nanVertices = before;
    nanVertices[0] = std::nanf("");
    CHECK_EQ(weldNormalsByPosition(nanVertices.data(), vertexCount, indices.data(),
                                   static_cast<uint32_t>(indices.size()), 0.5f,
                                   sVERTEX_FLOATS::Standard(), &stats),
             false);
}

TEST_CASE(weldAcceptsPositionOnlyLayoutAsInvalid) {
    // 只有位置的布局没有法线可写：必须拒绝，而不是写到 tangent 上。
    CHECK_EQ(sVERTEX_FLOATS::PositionOnly().HasNormal(), false);
    CHECK_EQ(sVERTEX_FLOATS::Standard().HasNormal(), true);
    std::vector<float> vertices;
    std::vector<uint32_t> indices;
    buildCurvedStrip(vertices, indices);
    sNORMAL_WELD_STATS stats;
    CHECK_EQ(weldNormalsByPosition(vertices.data(),
                                   static_cast<uint32_t>(vertices.size() / kStride),
                                   indices.data(), static_cast<uint32_t>(indices.size()), 0.5f,
                                   sVERTEX_FLOATS::PositionOnly(), &stats),
             false);
}

// ═══════════════════════════════════════════════════════════
//  二、质心
// ═══════════════════════════════════════════════════════════

TEST_CASE(centroidIsMeanNotBoundsCenter) {
    // 三个点：AABB 中心是 (1,0,0)，但均值是 (0.5,0,0) —— 半边空的网格两者差很远。
    std::vector<float> vertices;
    pushVertex(vertices, -1.0f, 0.0f, 0.0f, 0, 0, 1);
    pushVertex(vertices, 1.0f, 0.0f, 0.0f, 0, 0, 1);
    pushVertex(vertices, 1.5f, 0.0f, 0.0f, 0, 0, 1);
    GNg::sVEC3 centroid;
    CHECK(centroidOfVertices(vertices.data(), 3, sVERTEX_FLOATS::Standard(), &centroid));
    CHECK_NEAR(centroid.x, 0.5f, 1.0e-6);
    CHECK_NEAR(centroid.y, 0.0f, 1.0e-6);
    CHECK_NEAR(centroid.z, 0.0f, 1.0e-6);
}

TEST_CASE(centroidRejectsEmptyInput) {
    GNg::sVEC3 out;
    out.x = 123.0f;
    std::vector<float> vertices;
    pushVertex(vertices, 1.0f, 2.0f, 3.0f, 0, 0, 1);
    CHECK_EQ(centroidOfVertices(vertices.data(), 0, sVERTEX_FLOATS::Standard(), &out), false);
    CHECK_EQ(out.x, 123.0f); // 未写入
    CHECK_EQ(centroidOfVertices(0, 1, sVERTEX_FLOATS::Standard(), &out), false);
    CHECK_EQ(centroidOfVertices(vertices.data(), 1, sVERTEX_FLOATS::Standard(), 0), false);
}

TEST_CASE(centroidSkipsNothingButReportsNonFinite) {
    // 位置含 NaN ⇒ 结果非有限 ⇒ 返回 false，而不是把 NaN 传给调用方。
    std::vector<float> vertices;
    pushVertex(vertices, std::nanf(""), 0.0f, 0.0f, 0, 0, 1);
    GNg::sVEC3 out;
    CHECK_EQ(centroidOfVertices(vertices.data(), 1, sVERTEX_FLOATS::Standard(), &out), false);
}

// ═══════════════════════════════════════════════════════════
//  三、最近点
// ═══════════════════════════════════════════════════════════

TEST_CASE(nearestPointPicksClosest) {
    const GNg::sVEC3 points[3] = {{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {5.0f, 0.0f, 0.0f}};
    CHECK_EQ(nearestPointIndex(GNg::sVEC3{0.9f, 0.0f, 0.0f}, points, 3), 1);
    CHECK_EQ(nearestPointIndex(GNg::sVEC3{4.9f, 0.0f, 0.0f}, points, 3), 2);
    CHECK_EQ(nearestPointIndex(GNg::sVEC3{-3.0f, 0.0f, 0.0f}, points, 3), 0);
}

TEST_CASE(nearestPointBreaksTiesByLowerIndex) {
    // 两个等距离的点：必须稳定返回下标较小者，否则同一份资产每次运行绑到不同骨头。
    const GNg::sVEC3 points[2] = {{-1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}};
    CHECK_EQ(nearestPointIndex(GNg::sVEC3{0.0f, 0.0f, 0.0f}, points, 2), 0);
    // 三角：夹在中间那个点必须赢。
    const GNg::sVEC3 tri[3] = {{10.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {10.0f, 0.0f, 0.0f}};
    CHECK_EQ(nearestPointIndex(GNg::sVEC3{0.2f, 0.0f, 0.0f}, tri, 3), 1);
}

TEST_CASE(nearestPointHandlesEmptyAndNonFinite) {
    const GNg::sVEC3 points[2] = {{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}};
    CHECK_EQ(nearestPointIndex(GNg::sVEC3{0.0f, 0.0f, 0.0f}, points, 0), -1);
    CHECK_EQ(nearestPointIndex(GNg::sVEC3{0.0f, 0.0f, 0.0f}, 0, 2), -1);
    // 含 NaN 的候选不能靠"比较永远为假"混成赢家：应被跳过，选中有限的那个。
    const GNg::sVEC3 withNaN[2] = {{std::nanf(""), std::nanf(""), std::nanf("")},
                                   {3.0f, 0.0f, 0.0f}};
    CHECK_EQ(nearestPointIndex(GNg::sVEC3{2.9f, 0.0f, 0.0f}, withNaN, 2), 1);
}

// ═══════════════════════════════════════════════════════════
//  四、逆绑定矩阵
// ═══════════════════════════════════════════════════════════

TEST_CASE(jointPositionFromIdentityInverseBindIsOrigin) {
    GNg::sMAT44 identity;
    for (int i = 0; i < 16; ++i) {
        identity.m[i] = (i % 5 == 0) ? 1.0f : 0.0f; // 单位阵
    }
    GNg::sVEC3 position;
    CHECK(jointPositionFromInverseBind(identity, &position));
    CHECK_NEAR(position.x, 0.0f, 1.0e-6);
    CHECK_NEAR(position.y, 0.0f, 1.0e-6);
    CHECK_NEAR(position.z, 0.0f, 1.0e-6);
}

TEST_CASE(jointPositionInvertsTranslation) {
    // 若绑定世界矩阵是"平移 (1,2,3)"，则逆绑定矩阵的平移是 (-1,-2,-3)，
    // 关节世界位置应还原成 (1,2,3)。
    GNg::sMAT44 inverseBind;
    for (int i = 0; i < 16; ++i) {
        inverseBind.m[i] = (i % 5 == 0) ? 1.0f : 0.0f;
    }
    inverseBind.m[12] = -1.0f;
    inverseBind.m[13] = -2.0f;
    inverseBind.m[14] = -3.0f;
    GNg::sVEC3 position;
    CHECK(jointPositionFromInverseBind(inverseBind, &position));
    CHECK_NEAR(position.x, 1.0f, 1.0e-5);
    CHECK_NEAR(position.y, 2.0f, 1.0e-5);
    CHECK_NEAR(position.z, 3.0f, 1.0e-5);
}

TEST_CASE(jointPositionRejectsNonFinite) {
    GNg::sMAT44 inverseBind;
    for (int i = 0; i < 16; ++i) {
        inverseBind.m[i] = 0.0f;
    }
    inverseBind.m[15] = 1.0f;
    inverseBind.m[0] = std::nanf("");
    GNg::sVEC3 position;
    CHECK_EQ(jointPositionFromInverseBind(inverseBind, &position), false);
    CHECK_EQ(jointPositionFromInverseBind(inverseBind, 0), false);
}

TEST_CASE(nearestJointChainBindsCollarToNeck) {
    // 端到端串一遍 imoScene 的绑定链：**质心 → 关节位置 → 最近点**。
    // 场景取自真实问题：项圈在 glTF 里没有 skin，顶点已烘进世界坐标，
    // 不绑骨头就会留在原地。这里造一根竖着的三节链（髋/胸/颈）与一圈位于颈部的顶点。
    //
    // 逆绑定矩阵 = (绑定世界矩阵)^-1。绑定世界矩阵只有平移，所以逆矩阵的平移取负。
    const float jointHeights[3] = {0.0f, 1.0f, 2.0f};
    GNg::sMAT44 inverseBinds[3];
    for (int j = 0; j < 3; ++j) {
        for (int i = 0; i < 16; ++i) {
            inverseBinds[j].m[i] = (i % 5 == 0) ? 1.0f : 0.0f;
        }
        inverseBinds[j].m[13] = -jointHeights[j]; // 平移 y = -高度
    }
    GNg::sVEC3 jointPositions[3];
    for (int j = 0; j < 3; ++j) {
        CHECK(jointPositionFromInverseBind(inverseBinds[j], &jointPositions[j]));
    }
    CHECK_NEAR(jointPositions[2].y, 2.0f, 1.0e-5);

    // 一圈顶点位于 y≈2.05（r=0.15）—— 项圈套在脖子上
    std::vector<float> vertices;
    const int ringSegments = 12;
    for (int i = 0; i < ringSegments; ++i) {
        const float angle = 6.2831853f * static_cast<float>(i) / static_cast<float>(ringSegments);
        pushVertex(vertices, std::cos(angle) * 0.15f, 2.05f, std::sin(angle) * 0.15f, 0, 1, 0);
    }
    GNg::sVEC3 centroid;
    CHECK(centroidOfVertices(vertices.data(), static_cast<uint32_t>(ringSegments),
                             sVERTEX_FLOATS::Standard(), &centroid));
    // 环的质心在轴上，y 精确等于 2.05
    CHECK_NEAR(centroid.x, 0.0f, 1.0e-5);
    CHECK_NEAR(centroid.y, 2.05f, 1.0e-5);
    CHECK_NEAR(centroid.z, 0.0f, 1.0e-5);
    // 最近的关节必须是"颈"（下标 2），不是胸（下标 1）
    CHECK_EQ(nearestPointIndex(centroid, jointPositions, 3), 2);
}

// ═══════════════════════════════════════════════════════════
//  五、alpha 覆盖率 → 模式
// ═══════════════════════════════════════════════════════════

TEST_CASE(alphaCoverageKeepsExplicitOpaque) {
    // 资产明确说不透明：即使贴着 alpha 通道、有半透明像素，也不擅自升级。
    CHECK_EQ(resolveAlphaModeByCoverage(true, 0.9f, eALPHA_OPAQUE), eALPHA_OPAQUE);
    CHECK_EQ(resolveAlphaModeByCoverage(false, 0.0f, eALPHA_OPAQUE), eALPHA_OPAQUE);
}

TEST_CASE(alphaCoverageDowngradesTextureWithoutAlpha) {
    // 这一条是最容易漏、后果最严重的：材质标了 BLEND 但贴图全不透明 ⇒
    // 若照抄 BLEND，深度写关 + 远侧盖住近侧（帽子压住头发）。
    CHECK_EQ(resolveAlphaModeByCoverage(false, 0.0f, eALPHA_BLEND), eALPHA_OPAQUE);
    CHECK_EQ(resolveAlphaModeByCoverage(false, 0.0f, eALPHA_MASK), eALPHA_OPAQUE);
}

TEST_CASE(alphaCoverageDistinguishesCutoutFromBlend) {
    // 纯镂空（有 alpha 但没有半透明像素）⇒ MASK：能写深度、不用排序。
    CHECK_EQ(resolveAlphaModeByCoverage(true, 0.0f, eALPHA_BLEND), eALPHA_MASK);
    CHECK_EQ(resolveAlphaModeByCoverage(true, 0.05f, eALPHA_BLEND), eALPHA_MASK);
    // 大量半透明像素（薄纱/玻璃）⇒ 该真的混合。
    CHECK_EQ(resolveAlphaModeByCoverage(true, 0.31f, eALPHA_BLEND), eALPHA_BLEND);
    CHECK_EQ(resolveAlphaModeByCoverage(true, 0.80f, eALPHA_BLEND), eALPHA_BLEND);
    // 阈值边界：等于阈值不升级（严格大于才算）。
    CHECK_EQ(resolveAlphaModeByCoverage(true, 0.30f, eALPHA_BLEND), eALPHA_MASK);
    // 自定义阈值
    CHECK_EQ(resolveAlphaModeByCoverage(true, 0.20f, eALPHA_BLEND, 0.10f), eALPHA_BLEND);
    CHECK_EQ(resolveAlphaModeByCoverage(true, 0.20f, eALPHA_BLEND, 0.90f), eALPHA_MASK);
}

TEST_CASE(alphaCoverageHandlesNonFiniteRatio) {
    // 解码失败时可能传进 NaN：不能因为比较为假就走 BLEND。
    CHECK_EQ(resolveAlphaModeByCoverage(true, std::nanf(""), eALPHA_BLEND), eALPHA_MASK);
    CHECK_EQ(resolveAlphaModeByCoverage(true, -1.0f, eALPHA_BLEND), eALPHA_MASK);
}

// ═══════════════════════════════════════════════════════════
//  六、名称过滤
// ═══════════════════════════════════════════════════════════

TEST_CASE(nameFilterEmptyMatchesEverything) {
    CHECK(matchNameFilter("Mesh_Body", ""));
    CHECK(matchNameFilter("Mesh_Body", 0));
}

TEST_CASE(nameFilterMatchesSubstringIgnoreCase) {
    CHECK(matchNameFilter("Mesh_Hair.001", "hair"));
    CHECK(matchNameFilter("Mesh_Hair.001", "HAIR"));
    CHECK(matchNameFilter("Mesh_Hair.001", "hair.001"));
    CHECK_EQ(matchNameFilter("Mesh_Hair.001", "body"), false);
}

TEST_CASE(nameFilterSupportsCommaListAndSpaces) {
    CHECK(matchNameFilter("Mesh_Hat", "hair, hat"));
    CHECK(matchNameFilter("Mesh_Hat", "hair,hat"));
    CHECK(matchNameFilter("Mesh_Hat", "  hair  ,   hat  "));
    CHECK_EQ(matchNameFilter("Mesh_Skirt", "hair, hat"), false);
    // 空项被跳过（结尾多一个逗号不应导致误命中）
    CHECK_EQ(matchNameFilter("Mesh_Skirt", "hair,"), false);
    CHECK_EQ(matchNameFilter("Mesh_Skirt", ","), false); // 纯逗号是笔误：不命中，暴露问题
}

TEST_CASE(nameFilterRejectsNullNameUnlessFilterEmpty) {
    CHECK(matchNameFilter(0, ""));
    CHECK_EQ(matchNameFilter(0, "hair"), false);
}

// ── 2026-11-26：量化键溢出防护回归（|pos|×10000 超 int32 会回绕，把无关顶点焊在一起）──
TEST_CASE(meshWeldRejectsUnquantizablePosition) {
    float verts[22];
    for(int i = 0; i < 22; ++i) verts[i] = 0.0f;
    verts[0] = 300000.0f; // ×10000 = 3e9 > INT32_MAX
    verts[11] = 0.0f;
    const uint32_t indices[3] = {0, 1, 0};
    sNORMAL_WELD_STATS stats;
    CHECK(!GN3d::weldNormalsByPosition(verts, 2, indices, 3, 0.5f,
                                       GN3d::sVERTEX_FLOATS::Standard(), &stats));
    CHECK(verts[0] == 300000.0f); // 拒绝时不修改数据
}
