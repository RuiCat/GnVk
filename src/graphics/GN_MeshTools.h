#pragma once
#ifndef _GN_MESH_TOOLS_H_
#define _GN_MESH_TOOLS_H_
// ╔══════════════════════════════════════════════════════════╗
// ║  GN_mini — 网格工具（纯 CPU）：法线焊接 / 质心 / 最近点   ║
// ╚══════════════════════════════════════════════════════════╝
//
// 解决的问题：**导入期的网格修整**散落在各个演示程序里，谁先写谁带着走。
// 这些逻辑与资产格式无关（只吃"顶点数组 + 索引数组"），却被埋在
// `tests/imoScene/imoSceneConvert.cpp` 里，MMD 导入器想用只能复制一遍。
//
// 这里收的四个工具都满足两条：**与格式无关**（不认 glTF/PMX）、**可单测**
// （造几何即可，不需要任何模型资产）。
//
// ═══ 一、法线焊接（weldNormalsByPosition）═══
//
// FBX/Blender 导出的网格常把同一位置的顶点拆成多份，每份带自己那个面的法线
// （实测身体网格 1024 个重复顶点、951 处法线不同）。渲染出来就是**多边形硬边**
// —— 手指像削出来的、肩膀像折纸。原因是法线按顶点存，而顶点被拆开了：着色器
// 看不到"这个位置其实属于同一个表面"。
//
// 做法：按位置把顶点分组，组内取**面积加权平均法线**，但只在"确实是同一片曲面"
// 时才合并 —— 用夹角阈值判定（默认 60°）。超过阈值的组（领口、鞋底、刀锋这类
// 真硬边）保持原样，否则会把该有的棱角抹圆。
//
// 为什么权重是**面积**而不是角度：面积加权与"法线 = 面积矢量和"一致，
// 网格细分密度不均时也不会被小三角形带偏。
//
// 面积加权有一个反直觉的后果，必须写清楚：**它会提高"判为硬边"的倾向**。
// 立方体的一个角上三个面各差 90°，若三面等权，平均法线与各面夹角都是 54.7°，
// 60° 阈值下会被平滑。但四边形被拆成两个三角形后，角上三个面的入射三角形数是
// 2:1:1，加权平均被拉向大的那个面，与另两个面的夹角超过 104°，于是被判硬边。
// 结论：立方体在 60° 阈值下**保角**（实测 welded=8 / smoothed=0 / hard=8）。
// 这不是 bug —— 真几何里"小三角形汇聚在折边上"正是硬边，面积加权自动压住了它。
// 想要无条件平滑就把 thresholdCos 设成 -1。
//
// 为什么阈值比较用"**与平均值**的最大偏差"而不是两两夹角：
// 两两夹角是 O(n²)，且一圈折叠面（如裙摆）两两都接近 90° 却能被平滑。
// 与平均法线比是 O(n)，且语义正是"这一组能不能用一个法线代表"。
//
// ═══ 二、顶点质心（centroidOfVertices）═══
// ═══ 三、最近点（nearestPointIndex）═══
// ═══ 四、逆绑定矩阵取关节位置（jointPositionFromInverseBind）═══
//
// 后三个合起来解决一件事：**没有蒙皮信息的静态物件怎么跟随骨骼**。
// 项圈、发饰、眼镜在 glTF 里没有 skin（顶点已烘进世界坐标，父节点挂在 Armature 下），
// 动画一播它们就留在原地。做法是取顶点质心、找最近的关节、整块刚性绑上去。
// 这是**近似**：多个关节本该按距离加权，但配饰体积小、只跟着一块骨头动，
// 单一关节足够，而且没有关节权重就没有抖动。
//
// ═══ 五、alpha 覆盖率 → 模式（resolveAlphaModeByCoverage）═══
//
// FBX 导出的 glTF 常把**全部材质**标成 BLEND。照抄会让所有网格走进"深度写关"的
// 混合管线，表现是远处盖住近处（帽子压住头发、衣料互相穿透）。真正该判的依据是
// 贴图里 alpha 的分布：几乎没有半透明像素 ⇒ 那只是镂空，该按 MASK 裁。

#include <GN.h> // 必须先于本组头：GN_API 等基础宏
#include "GN_3Dmaterial.h" // eALPHA_MODE
#include "GN_3Dmath.h"

#include <cstdint>

namespace GN3d {

// ═══════════════════════════════════════════════════════════
//  顶点布局：用 stride + 偏移描述，不绑定某一种顶点结构
// ═══════════════════════════════════════════════════════════
// 全引擎的 3D 顶点都是 11 个 float（pos3 / normal3 / tangent3 / uv2），
// 但不同导入器给的位置可能不同。这里显式传布局，而不是到处写 `* 11u + 3`。
struct sVERTEX_FLOATS {
    uint32_t aStride; // 每顶点占几个 float
    uint32_t aPositionOffset; // 位置 x 在该顶点内的浮点下标
    uint32_t aNormalOffset; // 法线 x 在该顶点内的浮点下标

    sVERTEX_FLOATS();
    // 11 float 布局（pos 在 0、法线在 3），与 VkVertex3D 一致。
    static sVERTEX_FLOATS Standard();
    // 只有位置的布局（位置在 0，法线偏移无意义）。
    static sVERTEX_FLOATS PositionOnly();
    // 是否自洽（stride > 0 且位置 3 个分量、法线 3 个分量都落在 stride 内）。
    bool IsValid() const;
    // 是否含可用法线（需要 aNormalOffset 与位置偏移不同）。
    bool HasNormal() const;
};

// ═══════════════════════════════════════════════════════════
//  一、按位置焊接法线
// ═══════════════════════════════════════════════════════════
struct sNORMAL_WELD_STATS {
    uint32_t aWeldedGroups; // 位置重复（成员 ≥ 2）的组数
    uint32_t aSmoothedGroups; // 实际写入平均法线的组数
    uint32_t aHardEdgeGroups; // 超过夹角阈值、保持原样的组数
    uint32_t aDegenerateGroups; // 面法线互相抵消（平均长度为 0）的组数
    uint32_t aAffectedVertices; // 被改写法线的顶点数

    sNORMAL_WELD_STATS();
    // 有没有发生任何改写。
    bool Changed() const { return aSmoothedGroups > 0; }
};

// 按位置焊接法线：同位置的顶点组，若组内所有面法线与面积加权平均法线的夹角
// 都不超过阈值，就全部改写成该平均法线；否则整组保持原样。
//
// 参数：
//   vertices/vertexCount  顶点数组（原地改写 normal 分量）
//   indices/indexCount    三角形索引（indexCount 必须是 3 的倍数）
//   thresholdCos          夹角阈值的余弦，越小越宽松。60° ⇒ 0.5；<= -1 表示不限
//   layout                顶点布局
//   stats                 可为 null
//
// 返回 true 表示至少改写了一个顶点。**输入非法时返回 false 且不修改任何数据**
// （索引越界、count 为 0、layout 不自洽都算非法）—— 宁可不动，也不要写出半个网格。
//
// 位置量化精度 0.1 mm（与导出器一致）；容差内视为同位置。
bool weldNormalsByPosition(float *vertices, uint32_t vertexCount, const uint32_t *indices,
                           uint32_t indexCount, float thresholdCos, const sVERTEX_FLOATS &layout,
                           sNORMAL_WELD_STATS *stats);

// ═══════════════════════════════════════════════════════════
//  二、顶点质心
// ═══════════════════════════════════════════════════════════
// 顶点位置的平均值（不是 AABB 中心 —— 半边空的网格两者差很远）。
// vertexCount 为 0 返回 false 且不写 out。
bool centroidOfVertices(const float *vertices, uint32_t vertexCount, const sVERTEX_FLOATS &layout,
                        GNg::sVEC3 *out);

// ═══════════════════════════════════════════════════════════
//  三、最近点
// ═══════════════════════════════════════════════════════════
// 在 points[0..count) 里找离 point 最近的一个，返回下标；count 为 0 返回 -1。
// 并列时返回**下标较小者**（确定性：同一输入永远同一结果）。
int32_t nearestPointIndex(const GNg::sVEC3 &point, const GNg::sVEC3 *points, uint32_t count);

// ═══════════════════════════════════════════════════════════
//  四、逆绑定矩阵 → 关节世界位置
// ═══════════════════════════════════════════════════════════
// 逆绑定矩阵 = (绑定世界矩阵)^-1。关节的世界位置 = 该逆矩阵的平移部分取负、
// 再经其线性部分变换：p = -M.linear * M.translation（列主序 4x4）。
// 返回 false 表示矩阵含非有限值（不写 out）。
bool jointPositionFromInverseBind(const GNg::sMAT44 &inverseBind, GNg::sVEC3 *out);

// ═══════════════════════════════════════════════════════════
//  五、alpha 覆盖率 → 模式
// ═══════════════════════════════════════════════════════════
// 判定顺序（每一步的必要性都有用例）：
//   1. 源模式已是 OPAQUE ⇒ 保持 OPAQUE（导入了 alpha 但没声明，不擅自升级）；
//   2. 贴图**没有 alpha 通道**（hasAlpha=false）⇒ **OPAQUE**。这一条最容易被漏：
//      材质标了 BLEND 但贴图全不透明 ⇒ 走混合管线 ⇒ 深度写关 ⇒ 远侧盖住近侧；
//   3. 半透明像素占比 > blendThreshold ⇒ BLEND（玻璃、薄纱真心要混合）；
//   4. 其余 ⇒ MASK（二值镂空：衣料外侧 alpha=0、本体 1；能写深度、不用排序）。
//
// blendThreshold 语义是"半透明像素占比阈值"，默认 0.30 —— 取这个值是因为
// 实测薄纱类材质在 0.3~0.6 之间，而纯镂空衣物接近 0。
eALPHA_MODE resolveAlphaModeByCoverage(bool hasAlpha, float partialAlphaRatio,
                                       eALPHA_MODE source, float blendThreshold = 0.30f);

// ═══════════════════════════════════════════════════════════
//  六、网格名过滤（逗号分隔的子串匹配）
// ═══════════════════════════════════════════════════════════
// filter 为空 ⇒ 命中（不过滤）；否则按逗号切分，逐项做**大小写不敏感**的子串匹配，
// 命中任意一项即返回 true。前后空白会被忽略，空项被跳过。
// name 为 null 视为不命中；filter 为 null 等价于空。
bool matchNameFilter(const char *name, const char *filter);

} // namespace GN3d
#endif // _GN_MESH_TOOLS_H_
