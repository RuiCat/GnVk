// P2 GPU 蒙皮验收：蒙皮管线/调色板 UBO 检查 + 单/双关节 LBS 像素边界 + 129 关节截断 + 参数校验。
//
// 像素判定方法：用 identity viewProj/model 绘制一张 x 边界已知的四边形，蒙皮只做 x 平移；
// 回读中心行、找"首个被绘制像素"换算成 NDC x，与 CPU skinVertices 的参考左边界比较（±0.03）。
#include <GN.h>
#include "GN_Vk.h"
#include "GN_3DSkin.h"

#include "gpuCheckSkin.h"
#include "gpuCheckReport.h"
#include "gpuPixelRead.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

namespace GNg {
// 测试钩子（实现在 vkSkinDraw.cpp）：读蒙皮管线句柄、经同一缓存键探测是否命中。
VkPipeline GN_VkSkinPipeline();
bool GN_VkSkinPipelineProbe(VkPipeline *outPipeline);
} // namespace GNg

namespace GpuCheck {
namespace {

const float kEdgeTolerance = 0.03f; // 契约：单关节位移/CPU 参考一致 ±0.03（NDC）
const float kDrawnGreen = 0.20f; // 蒙皮色 g≈0.64，背景 g≈0.05，取中间值判定覆盖
const uint32_t kQuadIndices[6] = {0, 2, 1, 0, 3, 2};

struct sSKIN_QUAD {
    GN3d::sSKINNED_VERTEX aVerts[4];
};

// 构造 x ∈ [leftX, rightX]、y ∈ [-0.5, 0.5]、z = 0.5 的正面四边形；关节/权重由调用方设置。
void makeSkinQuad(sSKIN_QUAD &quad, float leftX, float rightX) {
    std::memset(&quad, 0, sizeof(quad));
    const float xs[4] = {leftX, rightX, rightX, leftX};
    const float ys[4] = {-0.5f, -0.5f, 0.5f, 0.5f};
    for(int index = 0; index < 4; ++index) {
        GN3d::sSKINNED_VERTEX &vertex = quad.aVerts[index];
        vertex.aPosition = GNg::sVEC3{xs[index], ys[index], 0.5f};
        vertex.aNormal = GNg::sVEC3{0.0f, 0.0f, 1.0f};
        vertex.aUv[0] = (index == 1 || index == 2) ? 1.0f : 0.0f;
        vertex.aUv[1] = (index >= 2) ? 1.0f : 0.0f;
        vertex.aTangent = GNg::sVEC3{1.0f, 0.0f, 0.0f};
        vertex.aJoints[0] = 0;
        vertex.aJoints[1] = 0;
        vertex.aJoints[2] = 0;
        vertex.aJoints[3] = 0;
        vertex.aWeights[0] = 1.0f;
    }
}

// 设置 4 个槽位的关节索引与前两个槽位的权重（其余槽位权重保持 0）。
void setSkinWeights(sSKIN_QUAD &quad, uint16_t joint0, uint16_t joint1, float weight0,
                    float weight1) {
    for(int index = 0; index < 4; ++index) {
        GN3d::sSKINNED_VERTEX &vertex = quad.aVerts[index];
        vertex.aJoints[0] = joint0;
        vertex.aJoints[1] = joint1;
        vertex.aWeights[0] = weight0;
        vertex.aWeights[1] = weight1;
    }
}

// 单位矩阵的最小包装（sMAT44 默认构造不初始化内存）。
GNg::sMAT44 makeIdentity() {
    GNg::sMAT44 matrix;
    GNg::Mat44Identity(&matrix);
    return matrix;
}

// 纯平移调色板项。
GNg::sMAT44 makeTranslation(float x, float y, float z) {
    GNg::sMAT44 matrix;
    GNg::Mat44Translate(&matrix, x, y, z);
    return matrix;
}

// 回读当前 swapchain 的中心行，返回首个"被蒙皮颜色覆盖"像素的 NDC x；没找到返回 false。
bool findDrawnLeftEdge(float *outNdcX) {
    const uint32_t width = GNg::GN_Vk.swapchainExtent.width;
    const uint32_t height = GNg::GN_Vk.swapchainExtent.height;
    std::vector<uint8_t> pixels;
    if(!ReadSwapchain(0, 0, (int)width, (int)height, pixels)) return false;

    const int y = (int)(height / 2);
    for(uint32_t x = 0; x < width; ++x) {
        const sFLOAT_PIXEL pixel = SampleSwapchain(pixels, (int)width, (int)x, y);
        if(pixel.g > kDrawnGreen && pixel.g > pixel.b + 0.05f) {
            *outNdcX = ((float)x + 0.5f) / (float)width * 2.0f - 1.0f;
            return true;
        }
    }
    return false;
}

// 绘制一次并回读左边界；返回是否成功提交 + 读到像素。
bool renderAndMeasure(const sSKIN_QUAD &quad, const GNg::sMAT44 *palette, uint32_t paletteCount,
                      float *outNdcX) {
    GNg::sMAT44 identity = makeIdentity();
    GNg::sMAT44 model = makeIdentity();
    GNg::sVEC3 eye = {0.0f, 0.0f, 1.0f};

    if(!GNg::GN_VkBeginFrame()) return false;
    GNg::GN_VkBegin3DPass(&identity, &eye, nullptr, 0);
    const bool submitted = GNg::GN_VkDraw3DSkinned(quad.aVerts, 4, kQuadIndices, 6, palette,
                                                   paletteCount, &model, 0.0f, 1.0f, 0.0f);
    GNg::GN_VkEnd3DPass();
    GNg::GN_VkEndFrame();
    vkQueueWaitIdle(GNg::GN_Vk.graphicsQueue);
    if(!submitted) return false;
    return findDrawnLeftEdge(outNdcX);
}

// 通用边界检查：CPU 参考用 cpuPalette（截断场景传前 128 项），GPU 用 palette。
void checkSkinnedLeftEdge(const char *name, const sSKIN_QUAD &quad, const GNg::sMAT44 *palette,
                          uint32_t paletteCount, const GNg::sMAT44 *cpuPalette,
                          uint32_t cpuPaletteCount, const char *extra) {
    GNg::sVEC3 cpuPositions[4];
    GNg::sVEC3 cpuNormals[4];
    if(!GN3d::skinVertices(quad.aVerts, 4, cpuPalette, cpuPaletteCount, cpuPositions, cpuNormals)) {
        ReportFail(name, "CPU skinVertices returned false");
        return;
    }

    float gpuLeftX = 0.0f;
    if(!renderAndMeasure(quad, palette, paletteCount, &gpuLeftX)) {
        ReportFail(name, "GN_VkDraw3DSkinned failed or swapchain readback failed");
        return;
    }

    const float delta = std::fabs(gpuLeftX - cpuPositions[0].x);
    char detail[192];
    std::snprintf(detail, sizeof(detail), "cpuLeftX=%.4f gpuLeftX=%.4f delta=%.4f tol=%.3f %s",
                  cpuPositions[0].x, gpuLeftX, delta, kEdgeTolerance, extra ? extra : "");
    if(delta <= kEdgeTolerance) ReportPass(name, detail);
    else ReportFail(name, detail);
}

// 检查 1：蒙皮管线非空且经 aPipelineCache 命中；调色板 UBO = 128×16×4 字节。
void checkPipelineAndPalette() {
    const size_t paletteBytes = sizeof(GN3d::sSKIN_UBO);
    VkPipeline pipeline = GNg::GN_VkSkinPipeline();
    VkPipeline probed = VK_NULL_HANDLE;
    const bool cacheHit = GNg::GN_VkSkinPipelineProbe(&probed);

    char detail[192];
    std::snprintf(detail, sizeof(detail),
                  "pipeline=%p cacheHit=%d probePipe=%p paletteUBO=%zu expected=%u",
                  (void *)pipeline, cacheHit ? 1 : 0, (void *)probed, paletteBytes,
                  (unsigned)(GN3d::SKIN_MAX_JOINTS * 16u * 4u));
    const bool ok = pipeline != VK_NULL_HANDLE && probed == pipeline && cacheHit &&
                    paletteBytes == (size_t)(GN3d::SKIN_MAX_JOINTS * 16u * 4u);
    if(ok) ReportPass("skin.pipeline", detail);
    else ReportFail("skin.pipeline", detail);
}

// 检查 5：空参数/未在 3D pass 内一律返回 false（不崩溃）。
void checkInvalidArguments() {
    sSKIN_QUAD quad;
    makeSkinQuad(quad, -0.5f, 0.5f);
    GNg::sMAT44 palette = makeTranslation(0.5f, 0.0f, 0.0f);
    GNg::sMAT44 model = makeIdentity();
    uint32_t accepted = 0;
    if(GNg::GN_VkDraw3DSkinned(nullptr, 4, kQuadIndices, 6, &palette, 1, &model, 0.0f, 1.0f, 0.0f)) {
        accepted |= 1u;
    }
    if(GNg::GN_VkDraw3DSkinned(quad.aVerts, 0, kQuadIndices, 6, &palette, 1, &model, 0.0f, 1.0f, 0.0f)) {
        accepted |= 2u;
    }
    if(GNg::GN_VkDraw3DSkinned(quad.aVerts, 4, kQuadIndices, 6, nullptr, 0, &model, 0.0f, 1.0f, 0.0f)) {
        accepted |= 4u;
    }
    if(GNg::GN_VkDraw3DSkinned(quad.aVerts, 4, kQuadIndices, 6, &palette, 0, &model, 0.0f, 1.0f, 0.0f)) {
        accepted |= 8u;
    }
    if(GNg::GN_VkDraw3DSkinned(quad.aVerts, 4, kQuadIndices, 6, &palette, 1, nullptr, 0.0f, 1.0f, 0.0f)) {
        accepted |= 16u;
    }
    // 未在 GN_VkBegin3DPass 之内：即便参数齐全也必须拒绝。
    if(GNg::GN_VkDraw3DSkinned(quad.aVerts, 4, kQuadIndices, 6, &palette, 1, &model, 0.0f, 1.0f, 0.0f)) {
        accepted |= 32u;
    }

    char detail[192];
    std::snprintf(detail, sizeof(detail),
                  "nullVertices/emptyCount/nullPalette/zeroPalette/nullModel/outsidePass accepted=0x%x",
                  accepted);
    if(accepted == 0) ReportPass("skin.invalidArgs", detail);
    else ReportFail("skin.invalidArgs", detail);
}

// 检查 2：单关节平移 +0.5x，左边界 = -0.5 + 0.5 = 0.0（GPU 与 CPU skinVertices 一致）。
void checkSingleJoint() {
    sSKIN_QUAD quad;
    makeSkinQuad(quad, -0.5f, 0.5f);
    setSkinWeights(quad, 0, 0, 1.0f, 0.0f);
    GNg::sMAT44 palette[1] = {makeTranslation(0.5f, 0.0f, 0.0f)};
    checkSkinnedLeftEdge("skin.singleJoint", quad, palette, 1, palette, 1,
                         "single joint +0.5x");
}

// 检查 3：双关节 0.5/0.5（平移 +0.5x 与单位矩阵），左边界 = 0.5*(-0.5+0.5)+0.5*(-0.5) = -0.25。
void checkDualJoint() {
    sSKIN_QUAD quad;
    makeSkinQuad(quad, -0.5f, 0.5f);
    setSkinWeights(quad, 0, 1, 0.5f, 0.5f);
    GNg::sMAT44 palette[2] = {makeTranslation(0.5f, 0.0f, 0.0f), makeIdentity()};
    checkSkinnedLeftEdge("skin.dualJoint", quad, palette, 2, palette, 2,
                         "dual joint weights 0.5/0.5");
}

// 检查 4：paletteCount=129 时只上传前 128 项；关节索引 128 被钳到 127，
// 参考用"截断后的前 128 项 + CPU skinVertices 的越界钳制"计算。
void checkJointLimit() {
    sSKIN_QUAD quad;
    makeSkinQuad(quad, -0.5f, 0.5f);
    setSkinWeights(quad, 128, 0, 1.0f, 0.0f);

    std::vector<GNg::sMAT44> palette(129, makeIdentity());
    palette[127] = makeTranslation(0.5f, 0.0f, 0.0f);
    palette[128] = makeTranslation(0.9f, 0.0f, 0.0f); // 第 129 项必须被截断丢弃
    std::vector<GNg::sMAT44> truncated(palette.begin(), palette.begin() + 128);

    GN3d::sSKIN_UBO packed;
    const uint32_t packedCount = GN3d::packPalette(palette.data(), 129, packed);
    char extra[96];
    std::snprintf(extra, sizeof(extra), "packed=%u/129 joint128->clamp127", packedCount);
    if(packedCount != 128) {
        ReportFail("skin.jointLimit", "packPalette(129) did not truncate to 128");
        return;
    }
    checkSkinnedLeftEdge("skin.jointLimit", quad, palette.data(), 129, truncated.data(), 128, extra);
}

} // namespace

void RunSkinChecks() {
    checkPipelineAndPalette();
    checkInvalidArguments();
    checkSingleJoint();
    checkDualJoint();
    checkJointLimit();
}

} // namespace GpuCheck
