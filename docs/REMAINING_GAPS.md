# 剩余缺口清单（2026-11-25 盘点）

这份清单把 P0–P6 规划之外、以及各轮视频制作时**明确留档未做**的项汇总起来，
按"能否在没有外部资产的容器里落地 + 是否可自动验收"排序。

标记：
- **[可做]** 纯 CPU 或现有 GPU 能力即可完成，能写单测/gpuCheck；
- **[需资产]** 需要真实 MMD/GLB/贴图才能验证；
- **[需桌面]** 需要真 GPU 桌面、逐像素基线或 RenderDoc；
- **[大改]** 涉及新的渲染管线或数据结构，工作量 ≥ 1 天。

---

## A. 编辑器与交互（本轮重点）

| # | 缺口 | 现状 | 影响 | 标记 |
|---|---|---|---|---|
| A1 | ~~Gizmo 未接真实输入~~ | ✅ 2026-11-25 完成：`gizmoRayFromScreen` + `cGIZMO_INTERACTION`（Begin/Drag/End/Hover） | — | 已完成 |
| A2 | ~~无吸附~~ | ✅ 2026-11-25 完成：`sGIZMO_SNAP` + `snapTranslate/snapRotate/snapScale` | — | 已完成 |
| A3 | 无多选 / 对象列表 | `cSCENE` 只有扁平指针数组 | 只能单对象编辑 | [大改] |
| A4 | 无撤销/重做 | 无 | 编辑不可回退 | [大改] |

## B. 渲染

| # | 缺口 | 现状 | 影响 | 标记 |
|---|---|---|---|---|
| B1 | ~~IBL 强度不可连续调节~~ | ✅ 2026-11-25 完成：`GN_VkSetGIIntensity(scale, specularRatio)`，复用 `indirect[0]/[1]` | — | 已完成 |
| B2 | 点光/聚光无阴影 | `AddPoint/AddSpot` 的 `enableShadow` 会被写进 `direction.w`，但着色器只处理 `shadowType ∈ [0.5,1.5)`（= 方向光的 2D 级联） | 火光/聚光灯照不出投影 | [大改] cube shadow map |
| B3 | 无 GPU 侧剔除 | CPU 逐实体剔除（`cSCENE`） | 实体数上万时 CPU 侧成为瓶颈 | [大改] compute culling |
| B4 | 无遮挡剔除 / 阴影剔除 | 只有视锥剔除 | overdraw 偏高 | [大改] |
| B5 | `sMATERIAL::aEmissive` 已打通 ✅ | 2026-11-25 完成 | — | 已完成 |

## C. 场景 / 物理 / 角色

| # | 缺口 | 现状 | 影响 | 标记 |
|---|---|---|---|---|
| C1 | ~~相机跟随未接线~~ | ✅ 2026-11-25 完成：`cENTITY::BuildCameraTarget`（角色 > 网格 > 拒绝） | — | 已完成 |
| C2 | 角色不推动态刚体 | `cCHARACTER` 只消费静态三角网 | 角色走过箱子会穿过去 | [大改] |
| C3 | 布娃娃 / IK / root motion | 无 | 动画表现力受限 | [大改] |
| C4 | 台阶抬升是"抬-移-落" | 不是真正的 capsule sweep | 薄台阶（< 胶囊半径）有轻微穿插感 | [大改] |

## D. 资产与验证

| # | 缺口 | 标记 |
|---|---|---|
| D1 | 真实 MMD/GLB 模型下的画面确认（仓库 `.gitignore` 掉了所有资产） | [需资产] |
| D2 | 逐像素基线截图（桌面 GPU） | [需桌面] |
| D3 | 混合树 / 动画层遮罩 | [大改] |
| D4 | 代码提交（容器内无 GitHub 凭据，只能本地 commit） | — |

---

## 第一阶段完成情况（2026-11-25）

| 项 | 交付 | 验收 |
|---|---|---|
| A1 Gizmo 输入 | `GN_Gizmo.h`：`gizmoRayFromScreen`、`sGIZMO_DRAG_STATE`、`cGIZMO_INTERACTION{Begin,Drag,End,Hover}` | `testGizmo.cpp` 新增 11 例（射线 4 + 拖拽 5 + hover/miss 2） |
| A2 吸附 | `sGIZMO_SNAP`、`snapToStep/snapTranslate/snapRotate/snapScale` | 新增 5 例（关闭/round/全分量/总角度/乘性步进） |
| B1 IBL 强度 | `GN_VkSetGIIntensity(scale, specularRatio)` | 编译 + 用于演示视频 |
| C1 相机跟随 | `cENTITY::BuildCameraTarget(out, heightOffset)` | `testEntity.cpp` 新增 4 例（优先级 + 端到端跟随） |

数字：`make unit` 625 → **643**（+18）；`gpuCheck` 120/0；`smoke` 7/7；`ci` 全绿。
演示视频：`tests/gizmoInput`（`make gizmoInput`），10 s，走**完整鼠标链路**。

### 下一阶段候选（按投入产出）

1. **B2 点光/聚光阴影**（cube shadow map）—— 火光/聚光灯照不出投影，是当前最明显的视觉缺口。
   估计需要：新建 6 面 cube render pass + 采样器 + 着色器分支，约 1 天。
2. **A3 多选 / A4 撤销**（[大改]）—— 需要 `cSCENE` 增加选择集与命令栈。
3. **C2 角色推动态刚体**（[大改]）—— 需要把 `cCHARACTER` 的扫掠结果反馈给物理世界。
4. **D1 真实资产验证**（[需资产]）—— 需要外部模型。
