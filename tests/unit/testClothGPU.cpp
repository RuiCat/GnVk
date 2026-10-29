// testClothGPU.cpp — P9-a GPU 布料：SSBO 布局、参数校验、CPU 回退（Verlet + Jacobi）与守卫。
#include "testFramework.h"

#include <GN.h>
#include "GN_3DclothGPU.h"

#include <cmath>
#include <cstddef>
#include <cstring>
#include <vector>

using namespace GN3d;

namespace {

sCLOTH_GPU_DESC makeDesc(uint32_t width, uint32_t height) {
    sCLOTH_GPU_DESC desc;
    desc.aWidth = width;
    desc.aHeight = height;
    desc.aSpacing = 0.2f;
    desc.aGravity[0] = 0.0f;
    desc.aGravity[1] = -10.0f;
    desc.aGravity[2] = 0.0f;
    desc.aDamping = 0.1f;
    desc.aIterations = 4;
    return desc;
}

std::vector<sCLOTH_GPU_VERTEX> makeGrid(uint32_t width, uint32_t height, float spacing,
                                        bool pinCorners) {
    std::vector<sCLOTH_GPU_VERTEX> vertices((size_t)width * height);
    for(uint32_t row = 0; row < height; ++row) {
        for(uint32_t column = 0; column < width; ++column) {
            sCLOTH_GPU_VERTEX &vertex = vertices[(size_t)row * width + column];
            std::memset(&vertex, 0, sizeof(vertex));
            vertex.aPos[0] = (float)column * spacing;
            vertex.aPos[1] = (float)row * spacing;
            vertex.aPos[2] = 0.0f;
            vertex.aPrev[0] = vertex.aPos[0];
            vertex.aPrev[1] = vertex.aPos[1];
            vertex.aPrev[2] = vertex.aPos[2];
        }
    }
    if(pinCorners) {
        vertices[0].aPinned = 1.0f;
        vertices[width - 1].aPinned = 1.0f;
        vertices[(size_t)(height - 1) * width].aPinned = 1.0f;
        vertices[(size_t)(height - 1) * width + width - 1].aPinned = 1.0f;
    }
    return vertices;
}

bool allFinite(const std::vector<sCLOTH_GPU_VERTEX> &vertices) {
    for(size_t i = 0; i < vertices.size(); ++i) {
        for(int axis = 0; axis < 3; ++axis) {
            if(!std::isfinite(vertices[i].aPos[axis])) return false;
            if(!std::isfinite(vertices[i].aPrev[axis])) return false;
        }
    }
    return true;
}

} // namespace

// SSBO 布局：32 字节、两个 vec4（pos+pinned / prev+pad），上限与迭代数与契约一致。
TEST_CASE(clothGpuLayout) {
    CHECK_EQ(sizeof(sCLOTH_GPU_VERTEX), (size_t)32);
    CHECK_EQ(offsetof(sCLOTH_GPU_VERTEX, aPos), (size_t)0);
    CHECK_EQ(offsetof(sCLOTH_GPU_VERTEX, aPinned), (size_t)12);
    CHECK_EQ(offsetof(sCLOTH_GPU_VERTEX, aPrev), (size_t)16);
    CHECK_EQ(offsetof(sCLOTH_GPU_VERTEX, aPad), (size_t)28);
    CHECK_EQ(CLOTH_GPU_MAX_DIM, 1024u);
    CHECK_EQ(CLOTH_GPU_MAX_ITERATIONS, 8u);
}

// 参数校验：尺寸/间距/阻尼/迭代轮数/重力任何一项非法都拒绝。
TEST_CASE(clothGpuDescValidation) {
    CHECK(validateClothGpuDesc(makeDesc(4, 4)));

    sCLOTH_GPU_DESC tooSmall = makeDesc(1, 4);
    CHECK(!validateClothGpuDesc(tooSmall));

    sCLOTH_GPU_DESC tooBig = makeDesc(1025, 4);
    CHECK(!validateClothGpuDesc(tooBig));

    sCLOTH_GPU_DESC badSpacing = makeDesc(4, 4);
    badSpacing.aSpacing = 0.0f;
    CHECK(!validateClothGpuDesc(badSpacing));

    sCLOTH_GPU_DESC badDamping = makeDesc(4, 4);
    badDamping.aDamping = 1.0f;
    CHECK(!validateClothGpuDesc(badDamping));

    sCLOTH_GPU_DESC badIterations = makeDesc(4, 4);
    badIterations.aIterations = 0;
    CHECK(!validateClothGpuDesc(badIterations));
    badIterations.aIterations = CLOTH_GPU_MAX_ITERATIONS + 1;
    CHECK(!validateClothGpuDesc(badIterations));

    sCLOTH_GPU_DESC nanGravity = makeDesc(4, 4);
    nanGravity.aGravity[1] = std::nanf("");
    CHECK(!validateClothGpuDesc(nanGravity));
}

// CPU 步进：pinned 顶点完全不动，其余顶点在重力下 y 下降，结果全有限。
TEST_CASE(clothCpuStepGravityPinned) {
    const uint32_t width = 4;
    const uint32_t height = 4;
    const sCLOTH_GPU_DESC desc = makeDesc(width, height);
    std::vector<sCLOTH_GPU_VERTEX> vertices = makeGrid(width, height, desc.aSpacing, true);
    const std::vector<sCLOTH_GPU_VERTEX> before = vertices;

    stepClothCpu(&vertices[0], (uint32_t)vertices.size(), desc, 0.1f);
    CHECK(allFinite(vertices));
    CHECK_EQ(vertices[0].aPos[1], before[0].aPos[1]); // 角点固定
    CHECK_EQ(vertices[width - 1].aPos[1], before[width - 1].aPos[1]);

    const uint32_t center = (height / 2) * width + width / 2;
    CHECK(vertices[center].aPos[1] < before[center].aPos[1]); // 重力下垂
    CHECK(std::fabs(vertices[center].aPos[1] - before[center].aPos[1]) > 1e-5f);
}

// 决定性：同参数 + 同 dt 的两次步进逐位相等。
TEST_CASE(clothCpuStepDeterministic) {
    const sCLOTH_GPU_DESC desc = makeDesc(5, 5);
    std::vector<sCLOTH_GPU_VERTEX> a = makeGrid(5, 5, desc.aSpacing, true);
    std::vector<sCLOTH_GPU_VERTEX> b = a;
    for(int step = 0; step < 12; ++step) {
        stepClothCpu(&a[0], (uint32_t)a.size(), desc, 1.0f / 60.0f);
        stepClothCpu(&b[0], (uint32_t)b.size(), desc, 1.0f / 60.0f);
    }
    CHECK(std::memcmp(&a[0], &b[0], a.size() * sizeof(sCLOTH_GPU_VERTEX)) == 0);
    CHECK(allFinite(a));
}

// 守卫：空指针、count 与网格不符、dt <= 0、非法 desc 都不改动数据。
TEST_CASE(clothCpuStepGuards) {
    const sCLOTH_GPU_DESC desc = makeDesc(4, 4);
    std::vector<sCLOTH_GPU_VERTEX> vertices = makeGrid(4, 4, desc.aSpacing, false);
    const std::vector<sCLOTH_GPU_VERTEX> before = vertices;

    stepClothCpu(0, (uint32_t)vertices.size(), desc, 0.1f);
    stepClothCpu(&vertices[0], 15, desc, 0.1f); // count 与 4*4 不符
    stepClothCpu(&vertices[0], (uint32_t)vertices.size(), desc, 0.0f);
    stepClothCpu(&vertices[0], (uint32_t)vertices.size(), desc, -0.1f);

    sCLOTH_GPU_DESC bad = desc;
    bad.aIterations = 0;
    stepClothCpu(&vertices[0], (uint32_t)vertices.size(), bad, 0.1f);

    CHECK(std::memcmp(&vertices[0], &before[0], vertices.size() * sizeof(sCLOTH_GPU_VERTEX)) == 0);
}
