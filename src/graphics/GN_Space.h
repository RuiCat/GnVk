#pragma once
#ifndef _GN_SPACE_H_
#define _GN_SPACE_H_
// ╔══════════════════════════════════════════════════════════╗
// ║  GN_mini — 坐标空间约定（单一事实来源）                   ║
// ║  世界 +Y 上 / -Z 相机前向 / +Z 资产正面 / CCW 正面        ║
// ╚══════════════════════════════════════════════════════════╝
//
// 契约（docs/AXIS_UNIFICATION_PLAN.md v2）：
//   * 3D 世界：右手系；+X 右、+Y 上、-Z 相机前向、+Z 资产正面；世界正面 = CCW（从外侧看）。
//   * 正高度视口 + 投影 Y 翻转 ⇒ 世界内容按正常朝向成像，CCW 内容在帧缓冲里仍是 CCW，
//     故含 Y 翻转的 3D pass 用 frontFace=CCW；不含 Y 翻转（identity / 未翻转正交）把内容
//     垂直镜像，CCW 内容呈 CW，只能配 frontFace=CW。
//     判据：Vulkan 的正面判定在**帧缓冲坐标**（+Y 向下）里做，投影 Y 翻转不改变视觉绕序。
//   * 2D 屏幕是独立空间：左上原点、+Y 下、屏幕绕序 CW；不参与 3D 规则。
//   * 视口只允许正高度；负高度等价于再翻一次 Y。
//   * 纹理/UV：左上原点、v 向下、图像 top-down；OBJ 的 v 在导入时翻转。
//   * 资产导入：glTF（+Z 前、CCW）零转换；OBJ 仅翻 v；MMD 按 fixture 做手性转换。
//   * 本文件只放约定与纯函数，不依赖 Vulkan，可被 CPU 单测直接链接。
#include "GN_3Dmath.h"

#include <cstdint>

namespace GN3d {

// 世界基向量（只读）
extern const GNg::sVEC3 SPACE_WORLD_RIGHT; // ( 1, 0, 0)
extern const GNg::sVEC3 SPACE_WORLD_UP; // ( 0, 1, 0)
extern const GNg::sVEC3 SPACE_CAMERA_FORWARD; // ( 0, 0,-1)
extern const GNg::sVEC3 SPACE_ASSET_FRONT; // ( 0, 0, 1)

// MMD 单位换算：v1 固定 1.0（不换算，见规划 §4.3）
extern const float SPACE_MMD_UNIT_SCALE;

enum eSPACE_HANDEDNESS { eSPACE_RH, eSPACE_LH };
enum eSPACE_FACE { eSPACE_FACE_CCW, eSPACE_FACE_CW };

// 世界 pass 的正面：投影含 Y 翻转 ⇒ 帧缓冲绕序反转 ⇒ CW；否则 CCW。
GN_API eSPACE_FACE frontFaceForWorldPass(bool projectionFlipsY);

// 2D 屏幕 pass 的正面：屏幕绕序 CW（左上原点、+Y 下），固定 CW。
GN_API eSPACE_FACE frontFaceForScreenPass();

// 视口尺寸校验：只接受正宽高（负高度视口等价于再翻一次 Y）。
GN_API bool isValidViewportSize(int width, int height);

// 就地反转三角形绕序（每 3 个索引交换后两个）。indexCount 必须为 3 的倍数；
// indices 为空且 indexCount==0 视为成功（无事可做）。
GN_API bool reverseWinding(uint32_t *indices, uint32_t indexCount);

// 就地镜像一组向量（每 strideFloats 个 float 为一个向量）的 X 分量：v.x = -v.x。
// 用于 MMD 的左手→右手转换；镜像会同时反转绕序。
GN_API bool mirrorVectorsX(float *values, uint32_t vectorCount, uint32_t strideFloats);

// 纹理 V 翻转：v' = 1 - v；非有限值原样返回。
GN_API float flipTextureV(float v);

// 天空「指向太阳」与方向光「传播方向」互为相反数；这里做显式转换，禁止直接互传。
GN_API GNg::sVEC3 sunDirectionToLightDirection(const GNg::sVEC3 &towardSun);
GN_API GNg::sVEC3 lightDirectionToSunDirection(const GNg::sVEC3 &lightDirection);

} // namespace GN3d
#endif // _GN_SPACE_H_
