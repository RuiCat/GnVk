// tests/fixtures/fixtures.cpp — 程序化骨架/蒙皮资产实现（纯 CPU，无 GPU 依赖）。

#include "fixtures.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

#include <sys/stat.h>

namespace fixture {

namespace {

// 单位四元数（绕 Z 轴 radians 的旋转）。四元数顺序与 GN 一致 (x, y, z, w)。
GNg::sQUAT quatFromAxisAngleZ(float radians) {
    GNg::sQUAT q;
    const float half = radians * 0.5f;
    q.x = 0.0f;
    q.y = 0.0f;
    q.z = std::sin(half);
    q.w = std::cos(half);
    return q;
}

} // namespace

GN3d::sSKELETON buildBoneChain(uint32_t jointCount) {
    if (jointCount < 1) {
        jointCount = 1;
    }
    GN3d::sSKELETON skeleton;
    skeleton.aJoints.resize(jointCount);
    for (uint32_t i = 0; i < jointCount; ++i) {
        GN3d::sJOINT &joint = skeleton.aJoints[i];
        joint.aParent = (i == 0) ? -1 : static_cast<int32_t>(i - 1);
        joint.aBindTranslation = GNg::sVEC3{0.0f, BONE_LENGTH, 0.0f};
        joint.aBindRotation = GNg::sQUAT{0.0f, 0.0f, 0.0f, 1.0f};
        joint.aBindScale = GNg::sVEC3{1.0f, 1.0f, 1.0f};
        // inverseBind = 绑定姿势世界矩阵的逆。绑定姿势是纯平移链：
        // world(i) = T(0, (i+1)*L, 0)，所以逆就是 T(0, -(i+1)*L, 0)。
        GNg::sMAT44 inverseBind(true);
        inverseBind.m[13] = -static_cast<float>(i + 1) * BONE_LENGTH;
        joint.aInverseBind = inverseBind;
    }
    return skeleton;
}

std::vector<GN3d::sANIM_TRANSFORM> bindPoseLocal(const GN3d::sSKELETON &skeleton) {
    std::vector<GN3d::sANIM_TRANSFORM> local(skeleton.aJoints.size());
    for (size_t i = 0; i < skeleton.aJoints.size(); ++i) {
        local[i].aTranslation = skeleton.aJoints[i].aBindTranslation;
        local[i].aRotation = skeleton.aJoints[i].aBindRotation;
        local[i].aScale = skeleton.aJoints[i].aBindScale;
    }
    return local;
}

std::vector<GN3d::sANIM_TRANSFORM> poseWithJointRotation(const GN3d::sSKELETON &skeleton,
                                                        uint32_t joint, float radians) {
    std::vector<GN3d::sANIM_TRANSFORM> local = bindPoseLocal(skeleton);
    if (joint >= local.size()) {
        return local;
    }
    local[joint].aRotation = quatFromAxisAngleZ(radians);
    return local;
}

std::vector<GN3d::sSKINNED_VERTEX> buildSkinnedCube(float size) {
    const float half = (size > 0.0f ? size : 1.0f) * 0.5f;
    // 顶点顺序：先 -Y 面 4 个（0..3），再 +Y 面 4 个（4..7），与 cubeIndices 对应。
    const float positions[8][3] = {
        {-half, -half, -half}, {half, -half, -half}, {half, -half, half}, {-half, -half, half},
        {-half, half, -half},  {half, half, -half},  {half, half, half},  {-half, half, half}};

    std::vector<GN3d::sSKINNED_VERTEX> vertices(8);
    for (uint32_t i = 0; i < 8; ++i) {
        GN3d::sSKINNED_VERTEX &v = vertices[i];
        std::memset(&v, 0, sizeof(v));
        v.aPosition = GNg::sVEC3{positions[i][0], positions[i][1], positions[i][2]};
        // 法线取顶点方向（球面近似）：只为让 LBS 有可断言的非零法线，不追求面法线正确。
        GNg::sVEC3 normal = {positions[i][0], positions[i][1], positions[i][2]};
        const float length = std::sqrt(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
        if (length > 0.0f) {
            normal.x /= length;
            normal.y /= length;
            normal.z /= length;
        }
        v.aNormal = normal;
        v.aUv[0] = (positions[i][0] > 0.0f) ? 1.0f : 0.0f;
        v.aUv[1] = (positions[i][1] > 0.0f) ? 1.0f : 0.0f;
        v.aTangent = GNg::sVEC3{1.0f, 0.0f, 0.0f};

        // 权重分配：下表面 100% 绑 root，上表面 100% 绑 mid（model 至少 2 节时才绑 mid）。
        const bool upper = (positions[i][1] > 0.0f);
        v.aJoints[0] = 0;
        v.aJoints[1] = 1;
        v.aJoints[2] = 0;
        v.aJoints[3] = 0;
        v.aWeights[0] = upper ? 0.0f : 1.0f;
        v.aWeights[1] = upper ? 1.0f : 0.0f;
        v.aWeights[2] = 0.0f;
        v.aWeights[3] = 0.0f;
    }
    return vertices;
}

std::vector<uint32_t> cubeIndices() {
    // 下表面（0..3，绕序与引擎正面口径一致：从外侧看连续）、上表面（4..7）、四个侧面。
    static const uint32_t indices[36] = {0, 2, 1, 0, 3, 2, 4, 5, 6, 4, 6, 7,
                                         0, 1, 5, 0, 5, 4, 1, 2, 6, 1, 6, 5,
                                         2, 3, 7, 2, 7, 6, 3, 0, 4, 3, 4, 7};
    return std::vector<uint32_t>(indices, indices + 36);
}

bool writeFixtures(const char *dir) {
    if (dir == nullptr || dir[0] == '\0') {
        return false;
    }
    ::mkdir(dir, 0755); // 已存在返回 EEXIST，忽略

    // 1) 骨链（默认 3 节 + 129 节的 UBO/SSBO 分流样本）
    const uint32_t counts[2] = {3u, 129u};
    for (int c = 0; c < 2; ++c) {
        const GN3d::sSKELETON skeleton = buildBoneChain(counts[c]);
        char path[512];
        std::snprintf(path, sizeof(path), "%s/bone%u.txt", dir, counts[c]);
        std::FILE *file = std::fopen(path, "wb");
        if (file == nullptr) {
            return false;
        }
        for (size_t i = 0; i < skeleton.aJoints.size(); ++i) {
            std::fprintf(file, "%u %d %.6f\n", (unsigned)i, skeleton.aJoints[i].aParent,
                         skeleton.aJoints[i].aBindTranslation.y);
        }
        std::fclose(file);
    }

    // 2) 蒙皮立方体
    const std::vector<GN3d::sSKINNED_VERTEX> vertices = buildSkinnedCube(1.0f);
    char path[512];
    std::snprintf(path, sizeof(path), "%s/cube_skin.txt", dir);
    std::FILE *file = std::fopen(path, "wb");
    if (file == nullptr) {
        return false;
    }
    for (size_t i = 0; i < vertices.size(); ++i) {
        const GN3d::sSKINNED_VERTEX &v = vertices[i];
        std::fprintf(file, "%.6f %.6f %.6f %.6f %.6f %.6f %.3f %.3f %u %.3f %u %.3f\n",
                     v.aPosition.x, v.aPosition.y, v.aPosition.z, v.aNormal.x, v.aNormal.y,
                     v.aNormal.z, v.aUv[0], v.aUv[1], (unsigned)v.aJoints[0], v.aWeights[0],
                     (unsigned)v.aJoints[1], v.aWeights[1]);
    }
    std::fclose(file);
    return true;
}

} // namespace fixture
