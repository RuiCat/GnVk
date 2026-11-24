#pragma once
#ifndef _GN_GIZMO_H_
#define _GN_GIZMO_H_
// ╔══════════════════════════════════════════════════════════╗
// ║  GN_mini — 编辑器基础控件（网格 / 坐标轴 / Gizmo / 线框） ║
// ╚══════════════════════════════════════════════════════════╝
//
// 为什么单独一层：这些东西都是**纯几何生成 + 纯数学拾取**，跟 GPU 没有关系。
// 把它们写成"能返回线段数组的纯函数"，就能在没有 Vulkan 的机器上逐条断言
// （网格线数、拾取命中哪根轴、样条端点/连续性、线框去重后的边数）。
// 上一轮做视频时的教训：只要几何和渲染混在一起，就只能靠"看画面像不像"来验证。
//
// 全部产出 `sEDIT_LINE`（带**每线颜色**）。为什么不用现成的 `sDEBUG_LINE`：
//   * 后者在 GN_Physics.h 里（编辑器不该依赖物理）；
//   * 它没有颜色字段，而 `GN_VkDrawDebugLines` 只接受**一个**批次共用色 ——
//     网格/坐标轴/Gizmo 必须一帧内多色（X红/Y绿/Z蓝 + 主线/次线），
//     单色批次就只能拆成 5 次绘制，既慢又难保证顺序。
// 对应的 GPU 入口是 `GN_VkDrawEditLines`（见 GN_Vk.h）。
#include <GN.h> // 必须先于本组头（GN_API 等基础宏）
#include "GN_3Dmath.h"
#include "GN_Spatial.h" // sAABB

#include <cstdint>
#include <vector>

namespace GN3d {

// ═══════════════════════════════════════════════════════════
//  线段
// ═══════════════════════════════════════════════════════════
struct sEDIT_LINE {
    GNg::sVEC3 aStart;
    GNg::sVEC3 aEnd;
    uint32_t aColor; // 0xAARRGGBB（与 GN_VkDrawDebugLines 同格式）

    sEDIT_LINE();
    sEDIT_LINE(const GNg::sVEC3 &start, const GNg::sVEC3 &end, uint32_t color);
};

// 语义色板。**不要在这些值之外硬编码颜色**：视频/示例/将来的编辑器 UI 要能一眼对上，
// 同一含义在三个地方用三个不同红是很难看出来的 bug。
namespace gizmoColor {
const uint32_t kAxisX = 0xFFD8443C; // +X 红
const uint32_t kAxisY = 0xFF4CC84C; // +Y 绿
const uint32_t kAxisZ = 0xFF3C7CD8; // +Z 蓝
const uint32_t kGridMinor = 0xFF2B2E34; // 网格次线（暗灰；再亮就会抢主体）
const uint32_t kGridMajor = 0xFF4A505A; // 网格主线
const uint32_t kGridFade = 0xFF1C1E22; // 网格外圈（更暗）
const uint32_t kHighlight = 0xFFFFD24A; // 高亮（当前选中/悬停的轴）
const uint32_t kBoundingBox = 0xFFB4A050; // 包围盒
const uint32_t kWireframe = 0xFF80C8B0; // 线框
const uint32_t kSpline = 0xFFC060E0; // 样条曲线
const uint32_t kSplineHandle = 0xFF7A5A88; // 样条控制点连线
} // namespace gizmoColor

// ═══════════════════════════════════════════════════════════
//  全局网格（Y-up 地面网格）
// ═══════════════════════════════════════════════════════════
struct sGRID_DESC {
    GNg::sVEC3 aCenter; // 网格中心（默认原点）
    float aCellSize; // 一格边长（米），默认 1
    uint32_t aHalfCells; // 中心向两侧各多少格，默认 10（共 21 条线/方向）
    uint32_t aMajorEvery; // 每 N 格画一条主线，0 = 不区分主次
    bool aDrawAxes; // 中心两条线用 X/Z 语义色（默认 true）
    bool aFade; // 外圈两格用更暗的颜色（默认 true）

    sGRID_DESC();
};

// 线数（不含空洞；供调用方预留容量）。
uint32_t gridLineCount(const sGRID_DESC &desc);
void buildGridLines(const sGRID_DESC &desc, std::vector<sEDIT_LINE> &out);

// 全局坐标轴（三根带箭头的轴线）。length 是每根轴长。
void buildAxesLines(const GNg::sVEC3 &origin, float length, std::vector<sEDIT_LINE> &out);

// ═══════════════════════════════════════════════════════════
//  包围盒 / 线框 / 标签十字
// ═══════════════════════════════════════════════════════════
// 12 条棱。空盒（IsValid()==false）什么都不产出。
//
// **padding 不是装饰**：物体的 AABB 棱通常正好贴在它的表面上，而调试线管线是
// 深度测试开 / 深度写关 —— 线与面同深度时会被判为"更远"，整框直接消失（实测现象：
// 网格画得出来，包围盒一根线都看不见）。给一点外扩（建议 0.005~0.02 倍对角长）
// 让线浮在表面之外。padding <= 0 表示不外扩（画在空中的盒、或已知不会 z-fight 时用）。
void buildBoxLines(const sAABB &box, uint32_t color, std::vector<sEDIT_LINE> &out,
                   float padding = 0.0f);
// 角上的三段小十字（"折叠角"标记，3D 视口里比整框更容易看清朝向）。
// padding 同上。
void buildBoxCornerMarks(const sAABB &box, float tickFraction, uint32_t color,
                         std::vector<sEDIT_LINE> &out, float padding = 0.0f);

// 建议的外扩量：**至少让线离开表面 minPixels 个像素**，否则绕不过深度冲突。
//
// 这条函数存在的原因是一次实际踩坑：随手给了 "1.5% 对角长"（1 m 的盒子 ≈ 1.3 cm），
// 在 12.5 m 外根本不到一个像素（该视角下 1 px ≈ 1.4 cm），于是整个包围盒一根线
// 都看不见；把外扩放大到 25% 才出现 —— 说明不是没生成、不是没画，纯粹是深度测试。
// 正确做法是按**屏幕像素**换算世界长度：
//   worldPerPixel = 2 · distance · tan(fovY/2) / screenHeight
//   padding       = max(minPixels · worldPerPixel, kMinFraction · 对角长)
float suggestedBoxPadding(const sAABB &box, float cameraDistance, float screenHeight,
                          float fovYRad, float minPixels = 2.0f);

// 三角网线框。**共享边只画一次**：直接把三条边都画，网格密集时会得到双重亮度的线，
// 看起来像"某些边更粗"。实现用 (min,max) 顶点序号对做去重。
// 顶点布局同引擎 3D 网格（11 floats/顶点，只读前 3 个）；transform 可空 = 不做变换。
void buildIndexedWireframeLines(const float *vertices, uint32_t vertexCount,
                                const uint32_t *indices, uint32_t indexCount,
                                const GNg::sMAT44 *transform, uint32_t color,
                                std::vector<sEDIT_LINE> &out);

// ═══════════════════════════════════════════════════════════
//  3D 样条曲线
// ═══════════════════════════════════════════════════════════
enum eSPLINE_MODE {
    eSPLINE_LINEAR = 0, // 折线（对照用）
    eSPLINE_CATMULL_ROM = 1, // 过点、C1 连续（默认）
    eSPLINE_BEZIER = 2 // 三次贝塞尔，把每 3 个点当一段的 P0,P1,P2（P3 = 下一段的 P0）
};

struct sSPLINE_DESC {
    eSPLINE_MODE aMode;
    bool aClosed; // 首尾相接（Catmull-Rom 会环绕取控制点）
    uint32_t aSamplesPerSegment; // 每段采样数（>= 2；1 会退化成折线）
    float aTension; // Catmull-Rom 张力 0..1（0 = 更尖锐，默认 0.5）

    sSPLINE_DESC();
};

// 全程 t ∈ [0,1] 求值。点不足 2 个时返回原点（调用方先检查 count）。
// count == 2 时退化成直线；closed 且 count < 3 时忽略 closed。
GNg::sVEC3 evaluateSpline(const GNg::sVEC3 *points, uint32_t count, const sSPLINE_DESC &desc,
                          float t);

// 采样点序列（含首尾）。段数 = closed ? count : count-1；输出点数 = 段数*samples + 1。
void sampleSpline(const GNg::sVEC3 *points, uint32_t count, const sSPLINE_DESC &desc,
                  std::vector<GNg::sVEC3> &outPoints);

// 采样 + 连成线段（一条折线，单色）。
void buildSplineLines(const GNg::sVEC3 *points, uint32_t count, const sSPLINE_DESC &desc,
                      uint32_t color, std::vector<sEDIT_LINE> &out);
// 控制点之间的小球/十字标记 + 控制多边形（虚线画法：隔一段画一段）。
void buildSplineHandles(const GNg::sVEC3 *points, uint32_t count, float markerSize,
                        bool drawPolygon, std::vector<sEDIT_LINE> &out);

// ═══════════════════════════════════════════════════════════
//  变换控件（Gizmo）
// ═══════════════════════════════════════════════════════════
enum eGIZMO_MODE { eGIZMO_TRANSLATE = 0, eGIZMO_ROTATE = 1, eGIZMO_SCALE = 2 };

enum eGIZMO_AXIS {
    eGIZMO_AXIS_NONE = -1,
    eGIZMO_AXIS_X = 0,
    eGIZMO_AXIS_Y = 1,
    eGIZMO_AXIS_Z = 2,
    eGIZMO_AXIS_XYZ = 3 // 中心的自由拖拽球/方块
};

struct sGIZMO_DESC {
    eGIZMO_MODE aMode;
    GNg::sVEC3 aOrigin; // 控件所在位置（通常是物体 AABB 中心）
    bool aLocalSpace; // true = 沿物体局部轴（需要 aBasis）
    GNg::sMAT44 aBasis; // aLocalSpace 时用：只有旋转部分（由 cMODEL::aRotation 构）
    float aSize; // 控件长度（世界单位）；通常由相机距离算出，见 gizmoScreenSize()
    eGIZMO_AXIS aHighlight; // 高亮轴（hover 或正在拖拽），画成 kHighlight 色

    sGIZMO_DESC();
};

// 让控件在屏幕上看起来恒定大小：size = distance * 2*tan(fov/2) * screenFraction。
float gizmoScreenSize(float cameraDistance, float fovYRad, float screenFraction);

// 生成控件的全部线段。
//   平移：3 根轴线 + 箭头（4 段） + 中心方块；
//   旋转：3 个圆环（每环 aRingSegments 段） + 中心小方块；
//   缩放：3 根轴线 + 末端方块（12 段） + 中心方块。
void buildGizmoLines(const sGIZMO_DESC &desc, std::vector<sEDIT_LINE> &out);

// 拾取：射线（已归一化）指向控件，返回**最近的**命中轴。pickRadius 是像素级的容差，
// 通常取 gizmo size 的 0.08~0.12 倍。
eGIZMO_AXIS pickGizmo(const sGIZMO_DESC &desc, const GNg::sVEC3 &rayOrigin,
                      const GNg::sVEC3 &rayDir, float pickRadius);

// ── 拖拽 ──
// 沿轴拖拽的位移量：把射线投影到轴线上，返回"沿轴移动了多少"。
// 调用方在拖拽开始时记下起始值，每帧用当前射线重算并取差值（不要累加，
// 累加会随帧率漂移）。dragAxis = X/Y/Z。
float gizmoDragTranslate(const sGIZMO_DESC &desc, eGIZMO_AXIS dragAxis,
                         const GNg::sVEC3 &rayOrigin, const GNg::sVEC3 &rayDir);

// 绕轴旋转的角度增量（弧度）。射线与环所在平面求交，取与起始点的夹角。
// 起止点都由调用方传（拖拽开始时记起点的平面坐标）。
float gizmoDragRotate(const sGIZMO_DESC &desc, eGIZMO_AXIS dragAxis,
                      const GNg::sVEC3 &startPlanePoint, const GNg::sVEC3 &rayOrigin,
                      const GNg::sVEC3 &rayDir);

// 缩放倍率（1.0 = 没变）。startProjection 是**拖拽开始时**由 gizmoDragTranslate 得到的
// 投影值；返回 current / start。传 0 或负值时返回 1（不缩放，而不是无穷大）。
float gizmoDragScale(const sGIZMO_DESC &desc, eGIZMO_AXIS dragAxis,
                     const GNg::sVEC3 &rayOrigin, const GNg::sVEC3 &rayDir,
                     float startProjection);

// 取控件在给定模式下的世界轴向量（已含 local/global 变换）。
GNg::sVEC3 gizmoAxisVector(const sGIZMO_DESC &desc, eGIZMO_AXIS axis);

// 由四元数构造一个"只有旋转"的基（列 0/1/2 = 局部 X/Y/Z）。
void gizmoBasisFromRotation(const GNg::sQUAT &rotation, GNg::sMAT44 *outBasis);

// ═══════════════════════════════════════════════════════════
//  吸附（A2）
// ═══════════════════════════════════════════════════════════
// 口径（每条都有用例）：
//   * **0 或负值 = 关闭**。这是唯一合理的"关闭"表达 —— 不能要求调用方传一个
//     "很大但不够大"的数来近似关闭。
//   * 吸附用 `round(v / step) * step`，**不是** `floor`：floor 会让物体永远偏向负方向，
//     拖到 0.9 格处会吸到 0 而不是 1。
//   * 角度吸附在**弧度**上做，步长也按弧度给（调用方可传 deg*π/180）。
//   * 缩放的"吸附"是**乘性步进**：1.0 → 1.25 → 1.5，而不是加性，否则放大后每档间距
//     相对变小，手感不均匀。
struct sGIZMO_SNAP {
    float aTranslateStep; // 位置吸附步长（米）；<= 0 关闭
    float aRotateStepRad; // 角度吸附步长（弧度）；<= 0 关闭
    float aScaleStep; // 缩放吸附步长（倍率增量）；<= 0 关闭

    sGIZMO_SNAP();
};
// 单值吸附。step <= 0 时原样返回（**不是**返回 0）。
float snapToStep(float value, float step);

// 把拖拽结果吸附成最终值。
//   translate：对**每个分量**吸附；base 是拖拽开始时的位置，delta 是沿轴位移。
//   rotate：把 base 角 + delta 吸附到最近的步长（不是吸附 delta —— 否则累积误差）。
GNg::sVEC3 snapTranslate(const GNg::sVEC3 &base, const GNg::sVEC3 &axis, float delta,
                         const sGIZMO_SNAP &snap);
float snapRotate(float baseAngleRad, float deltaRad, const sGIZMO_SNAP &snap);
float snapScale(float scale, const sGIZMO_SNAP &snap);

// ═══════════════════════════════════════════════════════════
//  输入接线（A1）：屏幕坐标 → 射线 → 拾取 → 拖拽 → 写回变换
// ═══════════════════════════════════════════════════════════
// 为什么需要"屏幕坐标 → 射线"这一步：`pickGizmo` 要的是世界空间射线，而输入子系统给的
// 是像素坐标。这条转换（含 Vulkan 的 y 向下、NDC 的 z ∈ [0,1]）是**最容易写错的一步**，
// 而它写错的表现是"控件点不中/点中隔壁轴"，不是崩溃 —— 所以单独成函数、单独测。
//
// screenX/screenY：像素坐标，**原点在左上角**（与 GN::MouseMX/MY 一致）。
// viewProj：与渲染同一个矩阵（列主序）。
// 返回 false 表示矩阵不可逆（此时 outOrigin/outDir 不修改）。
bool gizmoRayFromScreen(const GNg::sMAT44 &viewProj, float screenX, float screenY, float screenW,
                        float screenH, GNg::sVEC3 *outOrigin, GNg::sVEC3 *outDir);

// 一次完整的拖拽会话。典型用法：
//   onMouseDown:   interaction.Begin(desc, snap, &model->aTransform, &model->aPosition,
//                                    &model->aRotation, &model->aScale,
//                                    mouseX, mouseY, viewProj, w, h)
//   onMouseMove:   interaction.Drag(mouseX, mouseY, viewProj, w, h)   → 自动写回 TRS
//   onMouseUp:     interaction.End()
//   onMouseMove（无按键时）：interaction.Hover(&desc, x, y, viewProj, w, h) → 只更新高亮
//
// TRS 写回后调用方仍需自己 `cMODEL::computeWorldAABB()`（引擎不替你重算 AABB ——
// 那是每帧一次的操作，不该由控件隐式触发）。`aTransform` 由 UpdateTransform 重建，
// 这里只在 Begin/Drag 里调一次，避免调用方忘记。
//
// 设计取舍：**不持有 cMODEL 的所有权**，也不缓存"每一帧的矩阵"。拖拽开始那一刻记下
// 物体的原始 TRS，之后每次都用"原始值 + 吸附后的总位移"重算 —— 这样
//   * 吸附不会累积误差（每帧都从 base 重算）；
//   * 拖拽中途改吸附步长也不会跳变；
//   * 松开鼠标不需要"回滚"逻辑。
struct sGIZMO_DRAG_STATE {
    bool aActive; // 是否正在拖拽
    eGIZMO_MODE aMode;
    eGIZMO_AXIS aAxis;
    GNg::sVEC3 aStartOrigin; // 拖拽开始时控件所在位置
    GNg::sVEC3 aStartModelPos; // 拖拽开始时模型的位置
    GNg::sQUAT aStartModelRot; // 拖拽开始时模型的旋转
    GNg::sVEC3 aStartModelScale; // 拖拽开始时模型的缩放
    float aStartProjection; // translate: 起始投影值；rotate: 起始角；scale: 起始投影
    GNg::sVEC3 aStartPlanePoint; // rotate: 起始点在该轴平面上的世界坐标
    float aStartAngleRad; // rotate: 起始角
    bool aLocalSpace;

    sGIZMO_DRAG_STATE();
};

class cGIZMO_INTERACTION {
  public:
    // 开始拖拽。返回是否真的抓住了一根轴（false 时 aActive 保持 false）。
    // model 必须非空；snap 里的 <= 0 字段表示该维度不吸附。
    bool Begin(const sGIZMO_DESC &desc, const sGIZMO_SNAP &snap, GNg::sMAT44 *modelTransform,
               GNg::sVEC3 *modelPosition, GNg::sQUAT *modelRotation, GNg::sVEC3 *modelScale,
               float screenX, float screenY, const GNg::sMAT44 &viewProj, float screenW,
               float screenH);

    // 拖动。命中轴时把结果写回调用方给的 TRS 指针（Begin 时传的那几个）。
    // 返回本帧是否有变化（无变化时不写，避免无谓的 AABB 重算）。
    // 屏幕尺寸必须每次都传：窗口可能中途 resize（resize 后 viewProj 也变了）。
    bool Drag(float screenX, float screenY, const GNg::sMAT44 &viewProj, float screenW,
              float screenH);

    void End();
    bool Active() const;
    eGIZMO_AXIS Axis() const;

    // 悬停：只更新 desc.aHighlight，不改变任何变换。
    // 返回命中的轴（用于光标形状/提示）。
    static eGIZMO_AXIS Hover(sGIZMO_DESC *desc, float screenX, float screenY,
                             const GNg::sMAT44 &viewProj, float screenW, float screenH);

  private:
    sGIZMO_DRAG_STATE aState;
    sGIZMO_SNAP aSnap;
    GNg::sMAT44 *aModelTransform;
    GNg::sVEC3 *aModelPosition;
    GNg::sQUAT *aModelRotation;
    GNg::sVEC3 *aModelScale;
};

} // namespace GN3d
#endif // _GN_GIZMO_H_
