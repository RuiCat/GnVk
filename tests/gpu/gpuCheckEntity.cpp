// gpuCheckEntity.cpp — P2：实体层「骨骼控制器 → 蒙皮绘制」的运行时接线验收。
//
// 与 gpuCheckSkin / gpuCheckSkinSsbo 的分工：
//   * 那两个验的是 Vulkan 蒙皮入口本身（单/双关节像素边界、129 截断、SSBO 容量）；
//   * 本文件验的是**上层接线**：fixtures 骨架 → cSKELETON_CONTROLLER::ComputePalette
//     → selectSkinPalettePath 分流 → GN_VkDrawSkinnedAuto → 像素。
//     这条链正是文档里说的"从未在运行时跑通过"的那条。
//
// 判定方法（轮廓一致性）：把 CPU 端 skinVertices 的结果按同一 model 变换投影到屏幕，
// 得到期望轮廓；再逐采样点比对 GPU 是否绘制。离任何三角形边 ≥3 px 的采样点必须完全一致。
//
// 为什么不用「顶点位置差 ≤1e-4」：这条路读不到顶点输出（没有 transform feedback /
// 计算回写），只能回读像素。1e-4 世界单位在 640 宽下远小于 1 个像素，物理上不可能从
// 像素里分辨；这里改成"轮廓逐采样点一致 + 死区 3 px"，能力边界写清楚而不是假装达到 1e-4。
#include <GN.h>
#include "gpuTestHooks.h"
#include "GN_Vk.h"
#include "GN_3DSkin.h"
#include "GN_Entity.h"
#include "GN_AnimStateMachine.h"
#include "vkSkinnedDispatch.h"

#include "gpuCheckEntity.h"
#include "gpuCheckReport.h"
#include "gpuPixelRead.h"
#include "fixtures.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

namespace GpuCheck {
namespace {

const int kSampleStepPx = 2; // 采样网格步长（2 px：一次判定约 7.6 万个点）
const float kEdgeDeadZonePx = 3.0f; // 靠近三角形边界的采样点不判定（抗锯齿 + 光栅化取整）
const int kMinSamples = 1200; // 有效采样点下限：防止"网格全被跳过"导致用例空过

// 屏幕坐标下的一个采样点：是否在期望轮廓内、到最近边的距离。
struct sSAMPLE {
    int aPixelX;
    int aPixelY;
    bool aInside;
    float aEdgeDistancePx;
};

struct sPROJECTED_TRI {
    float aX[3];
    float aY[3];
};

// 单位矩阵。
GNg::sMAT44 makeIdentity() {
    GNg::sMAT44 matrix;
    GNg::Mat44Identity(&matrix);
    return matrix;
}

// model = T(0,0,0.5) * S(scale)：把夹具立方体挪到正 z、缩小到屏幕内。
// （identity viewProj 下 NDC=世界坐标，z 必须在 [0,1]，否则被深度裁剪。）
GNg::sMAT44 makeFixtureModel(float scale) {
    GNg::sMAT44 model;
    std::memset(&model, 0, sizeof(model));
    model.m[0] = scale;
    model.m[5] = scale;
    model.m[10] = scale;
    model.m[14] = 0.5f;
    model.m[15] = 1.0f;
    return model;
}

GNg::sVEC3 transformPoint(const GNg::sMAT44 &matrix, const GNg::sVEC3 &point) {
    return GNg::sVEC3{
        matrix.m[0] * point.x + matrix.m[4] * point.y + matrix.m[8] * point.z + matrix.m[12],
        matrix.m[1] * point.x + matrix.m[5] * point.y + matrix.m[9] * point.z + matrix.m[13],
        matrix.m[2] * point.x + matrix.m[6] * point.y + matrix.m[10] * point.z + matrix.m[14]};
}

// 点到线段距离（像素空间）。
float distanceToSegment(float px, float py, float ax, float ay, float bx, float by) {
    const float dx = bx - ax;
    const float dy = by - ay;
    const float lengthSq = dx * dx + dy * dy;
    float t = 0.0f;
    if (lengthSq > 1e-12f) {
        t = ((px - ax) * dx + (py - ay) * dy) / lengthSq;
        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;
    }
    const float cx = ax + t * dx;
    const float cy = ay + t * dy;
    return std::sqrt((px - cx) * (px - cx) + (py - cy) * (py - cy));
}

// 重心坐标判定（含边界）。
bool insideTriangle(float px, float py, const sPROJECTED_TRI &tri) {
    const float x0 = tri.aX[0];
    const float y0 = tri.aY[0];
    const float d1 = (px - tri.aX[1]) * (y0 - tri.aY[1]) - (x0 - tri.aX[1]) * (py - tri.aY[1]);
    const float d2 = (px - tri.aX[2]) * (tri.aY[1] - tri.aY[2]) - (tri.aX[1] - tri.aX[2]) * (py - tri.aY[2]);
    const float d3 = (px - x0) * (tri.aY[2] - y0) - (tri.aX[2] - x0) * (py - y0);
    const bool hasNegative = (d1 < 0.0f) || (d2 < 0.0f) || (d3 < 0.0f);
    const bool hasPositive = (d1 > 0.0f) || (d2 > 0.0f) || (d3 > 0.0f);
    return !(hasNegative && hasPositive);
}

// 把 CPU 蒙皮结果投影成屏幕三角形列表（像素坐标，y 向下）。
std::vector<sPROJECTED_TRI> projectTriangles(const std::vector<GNg::sVEC3> &localPositions,
                                             const std::vector<uint32_t> &indices,
                                             const GNg::sMAT44 &model, int width, int height) {
    std::vector<sPROJECTED_TRI> triangles;
    for (size_t i = 0; i + 2 < indices.size(); i += 3) {
        sPROJECTED_TRI tri;
        bool valid = true;
        for (int corner = 0; corner < 3; ++corner) {
            const uint32_t vertex = indices[i + (size_t)corner];
            if (vertex >= localPositions.size()) {
                valid = false;
                break;
            }
            const GNg::sVEC3 world = transformPoint(model, localPositions[vertex]);
            // identity viewProj ⇒ NDC = 世界坐标。
            // 回读图像的 row 与 Vulkan 一致：y_fb = (y_ndc + 1) / 2 × H，**NDC +y 在下方**
            // （Vulkan 的帧缓冲原点在左上、y 向下；引擎的投影矩阵自带 Y 翻转，所以屏幕上看着是
            // 正常的"y 向上"，但这里传的是 identity，没有那层翻转，必须按规范换算）。
            // 这条踩过坑：先写成 0.5 - y*0.5，结果 CPU 轮廓与 GPU 覆盖正好上下镜像。
            tri.aX[corner] = (world.x * 0.5f + 0.5f) * (float)width;
            tri.aY[corner] = (world.y * 0.5f + 0.5f) * (float)height;
        }
        if (valid) triangles.push_back(tri);
    }
    return triangles;
}

// 在采样网格上算出期望轮廓与到边距离。
std::vector<sSAMPLE> buildSamples(const std::vector<sPROJECTED_TRI> &triangles, int width,
                                  int height) {
    std::vector<sSAMPLE> samples;
    for (int y = kSampleStepPx / 2; y < height; y += kSampleStepPx) {
        for (int x = kSampleStepPx / 2; x < width; x += kSampleStepPx) {
            const float px = (float)x + 0.5f;
            const float py = (float)y + 0.5f;
            bool inside = false;
            float nearest = 1.0e9f;
            for (size_t t = 0; t < triangles.size(); ++t) {
                if (insideTriangle(px, py, triangles[t])) inside = true;
                for (int edge = 0; edge < 3; ++edge) {
                    const int next = (edge + 1) % 3;
                    const float distance =
                        distanceToSegment(px, py, triangles[t].aX[edge], triangles[t].aY[edge],
                                          triangles[t].aX[next], triangles[t].aY[next]);
                    if (distance < nearest) nearest = distance;
                }
            }
            sSAMPLE sample;
            sample.aPixelX = x;
            sample.aPixelY = y;
            sample.aInside = inside;
            sample.aEdgeDistancePx = nearest;
            samples.push_back(sample);
        }
    }
    return samples;
}

// GPU 侧是否画到了这个像素（蒙皮 frag 只做 albedo × Lambert，绿色通道最亮）。
bool gpuCovered(const std::vector<uint8_t> &pixels, int width, int x, int y) {
    const sFLOAT_PIXEL pixel = SampleSwapchain(pixels, width, x, y);
    return pixel.g > 0.20f && pixel.g > pixel.b + 0.05f;
}

// 画一帧：identity viewProj + 固定 model，经 GN_VkDrawSkinnedAuto（自动分流）提交。
bool drawOnce(const std::vector<GN3d::sSKINNED_VERTEX> &vertices,
              const std::vector<uint32_t> &indices, const GN3d::cSKELETON_CONTROLLER &pose,
              const GNg::sMAT44 &model) {
    GNg::sMAT44 viewProj = makeIdentity();
    GNg::sVEC3 eye = {0.0f, 0.0f, 1.0f};
    if (!GNg::GN_VkBeginFrame()) return false;
    GNg::GN_VkBegin3DPass(&viewProj, &eye, nullptr, 0);
    GNg::sSKINNED_DRAW_DESC desc;
    desc.aVertices = vertices.data();
    desc.aVertexCount = (uint32_t)vertices.size();
    desc.aIndices = indices.data();
    desc.aIndexCount = (uint32_t)indices.size();
    desc.aPose = &pose;
    desc.aModelMatrix = &model;
    desc.aColor[0] = 0.0f;
    desc.aColor[1] = 1.0f;
    desc.aColor[2] = 0.0f;
    const bool submitted = GNg::GN_VkDrawSkinnedAuto(desc);
    GNg::GN_VkEnd3DPass();
    GNg::GN_VkEndFrame();
    vkQueueWaitIdle(Queue());
    return submitted;
}

// CPU 参考：fixtures 立方体按给定姿势做 LBS。
bool cpuSkin(const std::vector<GN3d::sSKINNED_VERTEX> &vertices,
             const GN3d::cSKELETON_CONTROLLER &pose, std::vector<GNg::sVEC3> &outPositions) {
    outPositions.assign(vertices.size(), GNg::sVEC3{0.0f, 0.0f, 0.0f});
    std::vector<GNg::sVEC3> normals(vertices.size(), GNg::sVEC3{0.0f, 0.0f, 0.0f});
    return GN3d::skinVertices(vertices.data(), (uint32_t)vertices.size(), pose.Palette().data(),
                              (uint32_t)pose.Palette().size(), outPositions.data(),
                              normals.data());
}

// 核心：GPU 轮廓 vs CPU 轮廓，逐采样点比对（死区外必须一致）。
void checkSilhouette(const char *name, const std::vector<GN3d::sSKINNED_VERTEX> &vertices,
                     const std::vector<uint32_t> &indices, const GN3d::cSKELETON_CONTROLLER &pose,
                     const GNg::sMAT44 &model, const char *extra) {
    const int width = (int)Extent().width;
    const int height = (int)Extent().height;

    std::vector<GNg::sVEC3> cpuPositions;
    if (!cpuSkin(vertices, pose, cpuPositions)) {
        ReportFail(name, "CPU skinVertices returned false");
        return;
    }

    if (!drawOnce(vertices, indices, pose, model)) {
        ReportFail(name, "GN_VkDrawSkinnedAuto rejected the draw");
        return;
    }
    std::vector<uint8_t> pixels;
    if (!ReadSwapchain(0, 0, width, height, pixels)) {
        ReportFail(name, "swapchain readback failed");
        return;
    }

    const std::vector<sPROJECTED_TRI> triangles =
        projectTriangles(cpuPositions, indices, model, width, height);
    const std::vector<sSAMPLE> samples = buildSamples(triangles, width, height);

    int checked = 0;
    int insideCpu = 0;
    int mismatched = 0;
    int firstMismatchX = -1;
    int firstMismatchY = -1;
    for (size_t i = 0; i < samples.size(); ++i) {
        const sSAMPLE &sample = samples[i];
        if (sample.aEdgeDistancePx < kEdgeDeadZonePx) continue; // 抗锯齿/取整死区
        ++checked;
        if (sample.aInside) ++insideCpu;
        const bool gpu = gpuCovered(pixels, width, sample.aPixelX, sample.aPixelY);
        if (gpu != sample.aInside) {
            ++mismatched;
            if (firstMismatchX < 0) {
                firstMismatchX = sample.aPixelX;
                firstMismatchY = sample.aPixelY;
            }
        }
    }

    // 诊断用包围盒：CPU 投影 vs GPU 实际覆盖（对不上时一眼看出是"偏移/翻转/画大了"）。
    float cpuMinX = 1e9f, cpuMaxX = -1e9f, cpuMinY = 1e9f, cpuMaxY = -1e9f;
    for (size_t t = 0; t < triangles.size(); ++t) {
        for (int c = 0; c < 3; ++c) {
            cpuMinX = std::fmin(cpuMinX, triangles[t].aX[c]);
            cpuMaxX = std::fmax(cpuMaxX, triangles[t].aX[c]);
            cpuMinY = std::fmin(cpuMinY, triangles[t].aY[c]);
            cpuMaxY = std::fmax(cpuMaxY, triangles[t].aY[c]);
        }
    }
    int gpuMinX = width, gpuMaxX = -1, gpuMinY = height, gpuMaxY = -1;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            if (gpuCovered(pixels, width, x, y)) {
                if (x < gpuMinX) gpuMinX = x;
                if (x > gpuMaxX) gpuMaxX = x;
                if (y < gpuMinY) gpuMinY = y;
                if (y > gpuMaxY) gpuMaxY = y;
            }
        }
    }

    char detail[384];
    std::snprintf(detail, sizeof(detail),
                  "samples=%d(inside=%d) mismatch=%d first=(%d,%d) deadZone=%.0fpx path=%d "
                  "cpuBox=[%.0f,%.0f]-[%.0f,%.0f] gpuBox=[%d,%d]-[%d,%d] %s",
                  checked, insideCpu, mismatched, firstMismatchX, firstMismatchY,
                  kEdgeDeadZonePx, (int)GNg::GN_VkLastSkinnedPath(), cpuMinX, cpuMaxX, cpuMinY,
                  cpuMaxY, gpuMinX, gpuMinY, gpuMaxX, gpuMaxY, extra ? extra : "");
    if (checked < kMinSamples) {
        ReportFail(name, "too few samples (轮廓几乎全在死区里，用例没有实际判定)");
        return;
    }
    if (mismatched == 0) ReportPass(name, detail);
    else ReportFail(name, detail);
}

// 检查 1：路径分流表（纯 CPU 逻辑，但在 GPU 站点里报，保证和上面的绘制用的是同一套判断）。
void checkPathSelection() {
    struct sCase {
        uint32_t aJoints;
        GN3d::eSKIN_PALETTE_PATH aExpect;
    };
    const sCase cases[] = {{0, GN3d::eSKIN_PATH_NONE},
                           {1, GN3d::eSKIN_PATH_UBO},
                           {3, GN3d::eSKIN_PATH_UBO},
                           {GN3d::SKIN_MAX_JOINTS, GN3d::eSKIN_PATH_UBO},
                           {GN3d::SKIN_MAX_JOINTS + 1, GN3d::eSKIN_PATH_SSBO},
                           {GN3d::SKIN_SSBO_MAX_JOINTS, GN3d::eSKIN_PATH_SSBO},
                           {GN3d::SKIN_SSBO_MAX_JOINTS + 1, GN3d::eSKIN_PATH_NONE}};

    uint32_t wrong = 0;
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        if (GN3d::selectSkinPalettePath(cases[i].aJoints) != cases[i].aExpect) ++wrong;
    }
    // 控制器侧也验一遍：3 节骨架 → UBO，129 节 → SSBO，5000 节 → NONE
    const GN3d::sSKELETON small = fixture::buildBoneChain(3);
    const GN3d::sSKELETON big = fixture::buildBoneChain(129);
    const GN3d::sSKELETON huge = fixture::buildBoneChain(5000);
    GN3d::cSKELETON_CONTROLLER smallPose;
    GN3d::cSKELETON_CONTROLLER bigPose;
    GN3d::cSKELETON_CONTROLLER hugePose;
    smallPose.SetSkeleton(&small);
    bigPose.SetSkeleton(&big);
    hugePose.SetSkeleton(&huge);
    if (smallPose.PalettePath() != GN3d::eSKIN_PATH_UBO) ++wrong;
    if (bigPose.PalettePath() != GN3d::eSKIN_PATH_SSBO) ++wrong;
    if (hugePose.PalettePath() != GN3d::eSKIN_PATH_NONE) ++wrong;
    if (!GN3d::skinPaletteTruncated(5000)) ++wrong;
    if (GN3d::skinPaletteTruncated(129)) ++wrong;

    char detail[224];
    std::snprintf(detail, sizeof(detail),
                  "0/1/3/128/129/4096/4097 + controller(3/129/5000) wrong=%u", wrong);
    if (wrong == 0) ReportPass("entity.skin.path", detail);
    else ReportFail("entity.skin.path", detail);
}

// 检查 2：绑定姿势下控制器算出的调色板必须是单位阵，且 CPU LBS 还原原始顶点。
void checkBindPosePalette() {
    const GN3d::sSKELETON skeleton = fixture::buildBoneChain(3);
    GN3d::cSKELETON_CONTROLLER pose;
    pose.SetSkeleton(&skeleton);
    if (!pose.ComputePalette(pose.BindPoseLocal())) {
        ReportFail("entity.skin.bindPose", "ComputePalette failed");
        return;
    }

    float worst = 0.0f;
    for (size_t joint = 0; joint < pose.Palette().size(); ++joint) {
        for (int element = 0; element < 16; ++element) {
            const float expected = (element % 5 == 0) ? 1.0f : 0.0f;
            const float delta = std::fabs(pose.Palette()[joint].m[element] - expected);
            if (delta > worst) worst = delta;
        }
    }

    const std::vector<GN3d::sSKINNED_VERTEX> vertices = fixture::buildSkinnedCube(1.0f);
    std::vector<GNg::sVEC3> positions;
    if (!cpuSkin(vertices, pose, positions)) {
        ReportFail("entity.skin.bindPose", "skinVertices failed");
        return;
    }
    float worstVertex = 0.0f;
    for (size_t i = 0; i < vertices.size(); ++i) {
        const float dx = positions[i].x - vertices[i].aPosition.x;
        const float dy = positions[i].y - vertices[i].aPosition.y;
        const float dz = positions[i].z - vertices[i].aPosition.z;
        const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (distance > worstVertex) worstVertex = distance;
    }

    char detail[224];
    std::snprintf(detail, sizeof(detail),
                  "palette maxDev=%.2e vertex maxDev=%.2e joints=%u path=%d", worst, worstVertex,
                  pose.JointCount(), (int)pose.PalettePath());
    if (worst < 1e-5f && worstVertex < 1e-5f) ReportPass("entity.skin.bindPose", detail);
    else ReportFail("entity.skin.bindPose", detail);
}

// 检查 3：UBO 路径（3 节骨架，mid 转 60°）——GPU 轮廓必须与 CPU LBS 轮廓一致。
void checkSilhouetteUbo() {
    const GN3d::sSKELETON skeleton = fixture::buildBoneChain(3);
    GN3d::cSKELETON_CONTROLLER pose;
    pose.SetSkeleton(&skeleton);
    if (!pose.ComputePalette(fixture::poseWithJointRotation(skeleton, 1, 1.0471976f))) {
        ReportFail("entity.skin.silhouette.ubo", "ComputePalette failed");
        return;
    }
    const std::vector<GN3d::sSKINNED_VERTEX> vertices = fixture::buildSkinnedCube(1.0f);
    const std::vector<uint32_t> indices = fixture::cubeIndices();
    const GNg::sMAT44 model = makeFixtureModel(0.35f);
    checkSilhouette("entity.skin.silhouette.ubo", vertices, indices, pose, model,
                    "3 joints, mid rotated 60deg");
}

// 检查 4：SSBO 路径（129 节骨架）——同一批权重只用前两节关节，轮廓必须与 CPU 一致。
void checkSilhouetteSsbo() {
    const GN3d::sSKELETON skeleton = fixture::buildBoneChain(129);
    GN3d::cSKELETON_CONTROLLER pose;
    pose.SetSkeleton(&skeleton);
    if (!pose.ComputePalette(fixture::poseWithJointRotation(skeleton, 1, 1.0471976f))) {
        ReportFail("entity.skin.silhouette.ssbo", "ComputePalette failed");
        return;
    }
    if (pose.PalettePath() != GN3d::eSKIN_PATH_SSBO) {
        ReportFail("entity.skin.silhouette.ssbo", "129 joints did not route to SSBO");
        return;
    }
    const std::vector<GN3d::sSKINNED_VERTEX> vertices = fixture::buildSkinnedCube(1.0f);
    const std::vector<uint32_t> indices = fixture::cubeIndices();
    const GNg::sMAT44 model = makeFixtureModel(0.35f);
    checkSilhouette("entity.skin.silhouette.ssbo", vertices, indices, pose, model,
                    "129 joints routed to SSBO");
}

// 检查 5：不是空过——绑定位姿 vs 转了 60° 的轮廓必须明显不同（否则上面两条一致性无意义）。
void checkSilhouetteDiffers() {
    const GN3d::sSKELETON skeleton = fixture::buildBoneChain(3);
    const std::vector<GN3d::sSKINNED_VERTEX> vertices = fixture::buildSkinnedCube(1.0f);
    const std::vector<uint32_t> indices = fixture::cubeIndices();
    const GNg::sMAT44 model = makeFixtureModel(0.35f);
    const int width = (int)Extent().width;
    const int height = (int)Extent().height;

    GN3d::cSKELETON_CONTROLLER bindPose;
    bindPose.SetSkeleton(&skeleton);
    GN3d::cSKELETON_CONTROLLER rotated;
    rotated.SetSkeleton(&skeleton);

    std::vector<GNg::sVEC3> bindPositions;
    std::vector<GNg::sVEC3> rotatedPositions;
    const bool bindOk =
        bindPose.ComputePalette(bindPose.BindPoseLocal()) && cpuSkin(vertices, bindPose, bindPositions);
    const bool rotatedOk = rotated.ComputePalette(
                               fixture::poseWithJointRotation(skeleton, 1, 1.0471976f)) &&
                           cpuSkin(vertices, rotated, rotatedPositions);
    if (!bindOk || !rotatedOk) {
        ReportFail("entity.skin.silhouetteDiffers", "palette/skinning failed");
        return;
    }

    const std::vector<sPROJECTED_TRI> bindTris =
        projectTriangles(bindPositions, indices, model, width, height);
    const std::vector<sPROJECTED_TRI> rotTris =
        projectTriangles(rotatedPositions, indices, model, width, height);
    const std::vector<sSAMPLE> bindSamples = buildSamples(bindTris, width, height);
    const std::vector<sSAMPLE> rotSamples = buildSamples(rotTris, width, height);

    int differing = 0;
    for (size_t i = 0; i < bindSamples.size() && i < rotSamples.size(); ++i) {
        if (bindSamples[i].aEdgeDistancePx < kEdgeDeadZonePx &&
            rotSamples[i].aEdgeDistancePx < kEdgeDeadZonePx) {
            continue;
        }
        if (bindSamples[i].aInside != rotSamples[i].aInside) ++differing;
    }

    char detail[192];
    std::snprintf(detail, sizeof(detail), "differingSamples=%d (期望 >100)", differing);
    if (differing > 100) ReportPass("entity.skin.silhouetteDiffers", detail);
    else ReportFail("entity.skin.silhouetteDiffers", detail);
}

// 检查 6：拒绝非法输入（空姿势 / 未算调色板 / 超容量 / 空顶点）。
void checkRejects() {
    const std::vector<GN3d::sSKINNED_VERTEX> vertices = fixture::buildSkinnedCube(1.0f);
    const std::vector<uint32_t> indices = fixture::cubeIndices();
    const GNg::sMAT44 model = makeFixtureModel(0.35f);
    GNg::sMAT44 viewProj = makeIdentity();
    GNg::sVEC3 eye = {0.0f, 0.0f, 1.0f};

    GN3d::cSKELETON_CONTROLLER emptyPose; // 没有骨架
    const GN3d::sSKELETON skeleton = fixture::buildBoneChain(3);
    GN3d::cSKELETON_CONTROLLER uncomputed; // 有骨架但没算调色板
    uncomputed.SetSkeleton(&skeleton);
    const GN3d::sSKELETON huge = fixture::buildBoneChain(5000);
    GN3d::cSKELETON_CONTROLLER hugePose;
    hugePose.SetSkeleton(&huge);
    hugePose.ComputePalette(hugePose.BindPoseLocal());

    uint32_t accepted = 0;
    if (!GNg::GN_VkBeginFrame()) {
        ReportSkip("entity.skin.rejects", "GN_VkBeginFrame failed");
        return;
    }
    GNg::GN_VkBegin3DPass(&viewProj, &eye, nullptr, 0);
    {
        GNg::sSKINNED_DRAW_DESC desc;
        desc.aVertices = vertices.data();
        desc.aVertexCount = (uint32_t)vertices.size();
        desc.aIndices = indices.data();
        desc.aIndexCount = (uint32_t)indices.size();
        desc.aModelMatrix = &model;
        // 1) aPose 为空
        if (GNg::GN_VkDrawSkinnedAuto(desc)) accepted |= 1u;
        // 2) 有姿势但没算调色板
        desc.aPose = &uncomputed;
        if (GNg::GN_VkDrawSkinnedAuto(desc)) accepted |= 2u;
        // 3) 空顶点
        desc.aPose = &uncomputed;
        desc.aVertices = nullptr;
        if (GNg::GN_VkDrawSkinnedAuto(desc)) accepted |= 4u;
        // 4) 没有骨架（palette 空）
        desc.aPose = &emptyPose;
        desc.aVertices = vertices.data();
        if (GNg::GN_VkDrawSkinnedAuto(desc)) accepted |= 8u;
        // 5) 超 SSBO 容量（5000 关节）——必须明确拒绝，而不是静默截断
        desc.aPose = &hugePose;
        if (GNg::GN_VkDrawSkinnedAuto(desc)) accepted |= 16u;
    }
    GNg::GN_VkEnd3DPass();
    GNg::GN_VkEndFrame();
    vkQueueWaitIdle(Queue());

    char detail[192];
    std::snprintf(detail, sizeof(detail),
                  "nullPose/uncomputed/nullVerts/noSkeleton/overCapacity accepted=0x%x", accepted);
    if (accepted == 0) ReportPass("entity.skin.rejects", detail);
    else ReportFail("entity.skin.rejects", detail);
}

// 构造一段"关节固定平移 + 绕 Z 转"的 clip（等价于真实资产里被驱动的骨骼：T+R 都有通道）。
GN3d::sANIM_CLIP makePoseClip(uint32_t node, float degrees, float bindY) {
    GN3d::sANIM_CLIP clip;
    clip.aDuration = 1.0f;
    GN3d::sANIM_CHANNEL rotation;
    rotation.aNode = node;
    rotation.aPath = GN3d::eANIM_ROTATION;
    rotation.aInterp = GN3d::eANIM_LINEAR;
    rotation.aTimes.push_back(0.0f);
    rotation.aTimes.push_back(1.0f);
    const float half = degrees * 0.017453292519943f * 0.5f;
    rotation.aQuatValues.push_back(GNg::sQUAT{0.0f, 0.0f, std::sin(half), std::cos(half)});
    rotation.aQuatValues.push_back(GNg::sQUAT{0.0f, 0.0f, std::sin(half), std::cos(half)});
    clip.aChannels.push_back(rotation);

    GN3d::sANIM_CHANNEL translation;
    translation.aNode = node;
    translation.aPath = GN3d::eANIM_TRANSLATION;
    translation.aInterp = GN3d::eANIM_LINEAR;
    translation.aTimes.push_back(0.0f);
    translation.aTimes.push_back(1.0f);
    translation.aVecValues.push_back(GNg::sVEC3{0.0f, bindY, 0.0f});
    translation.aVecValues.push_back(GNg::sVEC3{0.0f, bindY, 0.0f});
    clip.aChannels.push_back(translation);
    clip.updateDuration();
    return clip;
}

// 采样"当前帧里被蒙皮颜色覆盖的像素集合"（按采样网格，返回位图）。
std::vector<uint8_t> captureCoverage(int width, int height, int *outCount) {
    std::vector<uint8_t> mask((size_t)width * (size_t)height, 0);
    std::vector<uint8_t> pixels;
    int count = 0;
    if (!ReadSwapchain(0, 0, width, height, pixels)) {
        if (outCount) *outCount = -1;
        return mask;
    }
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            if (gpuCovered(pixels, width, x, y)) {
                mask[(size_t)y * (size_t)width + (size_t)x] = 1;
                ++count;
            }
        }
    }
    if (outCount) *outCount = count;
    return mask;
}

// 用实体更新一帧后画一次并回读覆盖位图。
bool drawEntityFrame(GN3d::cENTITY &entity, const std::vector<GN3d::sSKINNED_VERTEX> &vertices,
                     const std::vector<uint32_t> &indices, const GNg::sMAT44 &model,
                     std::vector<uint8_t> &outMask, int *outCount) {
    GNg::sMAT44 viewProj = makeIdentity();
    GNg::sVEC3 eye = {0.0f, 0.0f, 1.0f};
    if (!GNg::GN_VkBeginFrame()) return false;
    GNg::GN_VkBegin3DPass(&viewProj, &eye, nullptr, 0);
    GNg::sSKINNED_DRAW_DESC desc;
    desc.aVertices = vertices.data();
    desc.aVertexCount = (uint32_t)vertices.size();
    desc.aIndices = indices.data();
    desc.aIndexCount = (uint32_t)indices.size();
    desc.aPose = &entity.aSkeleton;
    desc.aModelMatrix = &model;
    desc.aColor[0] = 0.0f;
    desc.aColor[1] = 1.0f;
    desc.aColor[2] = 0.0f;
    const bool submitted = GNg::GN_VkDrawSkinnedAuto(desc);
    GNg::GN_VkEnd3DPass();
    GNg::GN_VkEndFrame();
    vkQueueWaitIdle(Queue());
    if (!submitted) return false;
    outMask = captureCoverage((int)Extent().width, (int)Extent().height, outCount);
    return outCount == nullptr || *outCount >= 0;
}

// 检查 7（P3）：状态机切换 → 实体更新 → GPU 画面跟着变。
// 这条把「参数 → 转换 → 播放器 → 局部姿势 → 调色板 → 蒙皮绘制」整条链在运行时串一遍，
// 也正是文档里 entity.animSwitch 要的那一条。
void checkAnimSwitch() {
    const GN3d::sSKELETON skeleton = fixture::buildBoneChain(3);
    const std::vector<GN3d::sSKINNED_VERTEX> vertices = fixture::buildSkinnedCube(1.0f);
    const std::vector<uint32_t> indices = fixture::cubeIndices();
    const GNg::sMAT44 model = makeFixtureModel(0.35f);

    GN3d::sANIM_CLIP pose0 = makePoseClip(1, 0.0f, fixture::BONE_LENGTH);
    GN3d::sANIM_CLIP pose90 = makePoseClip(1, 90.0f, fixture::BONE_LENGTH);

    GN3d::cENTITY entity;
    entity.aSkeleton.SetSkeleton(&skeleton);
    if (!entity.aAnim.RegisterClip("pose0", &pose0) || !entity.aAnim.RegisterClip("pose90", &pose90)) {
        ReportFail("entity.animSwitch", "RegisterClip failed");
        return;
    }
    const uint16_t state0 = entity.aAnim.StateMachine().AddState("pose0", "pose0", true);
    const uint16_t state90 = entity.aAnim.StateMachine().AddState("pose90", "pose90", true);
    if (state0 == GN3d::cANIM_STATE_MACHINE::INVALID_STATE ||
        state90 == GN3d::cANIM_STATE_MACHINE::INVALID_STATE) {
        ReportFail("entity.animSwitch", "AddState failed");
        return;
    }
    entity.aAnim.StateMachine().AddTransitionOnTrigger(state0, state90, 1 /* trigger id */, 0.0f);
    if (!entity.aAnim.StateMachine().SetInitial(state0) || !entity.aAnim.StartStateMachine()) {
        ReportFail("entity.animSwitch", "StartStateMachine failed");
        return;
    }

    // 状态 A 跑稳
    for (int i = 0; i < 12; ++i) entity.Update(1.0f / 60.0f);
    std::vector<uint8_t> maskA;
    int countA = -1;
    if (!drawEntityFrame(entity, vertices, indices, model, maskA, &countA) || countA < 0) {
        ReportFail("entity.animSwitch", "draw/readback failed (state A)");
        return;
    }

    // 触发切换 → 状态 B
    entity.aParams.Trigger(1);
    for (int i = 0; i < 12; ++i) entity.Update(1.0f / 60.0f);
    std::vector<uint8_t> maskB;
    int countB = -1;
    if (!drawEntityFrame(entity, vertices, indices, model, maskB, &countB) || countB < 0) {
        ReportFail("entity.animSwitch", "draw/readback failed (state B)");
        return;
    }

    if (maskA.size() != maskB.size() || maskA.empty()) {
        ReportFail("entity.animSwitch", "coverage mask size mismatch");
        return;
    }
    long changed = 0;
    long unionCount = 0;
    for (size_t i = 0; i < maskA.size(); ++i) {
        if (maskA[i] != maskB[i]) ++changed;
        if (maskA[i] != 0 || maskB[i] != 0) ++unionCount;
    }
    const double ratio = unionCount > 0 ? (double)changed / (double)unionCount : 0.0;

    char detail[256];
    std::snprintf(detail, sizeof(detail),
                  "state=%s clip=%s coveredA=%d coveredB=%d changed=%ld union=%ld (%.1f%%)",
                  entity.aAnim.StateMachine().StateName(entity.aAnim.StateMachine().CurrentState()),
                  entity.aAnim.Current(), countA, countB, changed, unionCount, ratio * 100.0);
    // 至少 10% 的覆盖像素要变：换姿势却不改画面，说明状态机/骨骼/绘制三者有一处没接上。
    if (changed > 0 && ratio >= 0.10) ReportPass("entity.animSwitch", detail);
    else ReportFail("entity.animSwitch", detail);
}

} // namespace

void RunEntityChecks() {
    checkPathSelection();
    checkBindPosePalette();
    checkSilhouetteDiffers();
    checkSilhouetteUbo();
    checkSilhouetteSsbo();
    checkRejects();
    checkAnimSwitch();
}

} // namespace GpuCheck
