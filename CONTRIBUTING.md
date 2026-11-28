# 贡献指南（CONTRIBUTING）

GnVk / GN_mini 引擎的开发约定与提交要求。与 `docs/REFACTOR_PLAN.md` 的工程纪律一致，
机检门禁见 `tests/lint/` 与 `make ci`。

## 1. 构建与门禁

只用 `make <target>` 构建（CMake 为唯一来源，Makefile 是薄封装；产物在 `build-cmake/`）。

```bash
make            # 配置 + 构建全部产物（引擎库 + 单测/验收/示例）
make unit       # 单元测试（纯逻辑，无需 GPU）
make ci         # 全门禁：check + smoke + doc-check + spv-check + lint-strict + warnings
make help       # 列出真实目标
```

`make ci` 是合并前的硬门槛，包含：

| 门禁 | 脚本/目标 | 口径 |
|---|---|---|
| 单元测试 | `unit` | 全过（当前 955 例），新增逻辑须配测试 |
| 示例冒烟 | `smoke` | 无窗口各示例 8 帧不崩 |
| 文档统计/索引/版本 | `doc-check` | `make stats` 数字一致、索引双向校验、版本单一来源 |
| SPIR-V | `spv-check` | 内嵌着色器与源同步 |
| 分层/输出/分组/输入 | `lint-strict` | `tests/lint/{layers,output,context_groups,input}.sh` |
| 告警预算 | `warnings` | 扩展警告口径 src 主位置 < 500（当前 7，均为登记保留项） |

GPU/真机相关验收（`gpuCheck`、视觉 A/B）需 Vulkan 设备；无设备时冒烟自动 SKIP，
**不得在无 GPU 主机把 GPU 项标 done**。

## 2. 命名约定

详见 `docs/代码功能简述与规范.txt`（历史文档，命名部分仍有效）。摘要：

- **全局函数/变量**：大写开头驼峰（`GlobalValue`）；引擎内部前缀 `GT_`（不建议应用层使用）。
- **结构体** `s+全大写`（`sCOLOR`）；**类** `c+全大写`（`cMODEL`）；**常量** 全大写（`GN_VERSION`）；
  **枚举标志** `e+全大写`（`eAM_LOOP`），按 2 的幂取值以便 `|` 组合。
- **局部变量/参数**：全小写下划线；裸指针前缀 `_`（`char *_pstr`）。
- **函数内 static**：首词小写后驼峰（`supportExtNames`）。
- **类成员**：`a` 开头为用户可写属性；`z` 开头为内部成员（不建议外部使用）；大写开头成员函数为用户接口。
- **命名空间**：`GNg`（2D/3D 通用）、`GN3d`（3D）、`GNu`（UI）、`GNn`（网络）、`GNa`（音频）、
  `GNf`（文件）、`GNm`（多媒体）、`GNd`（加密转码）、`GN`（系统/核心）。
- 数学参数带 `Nz` 表示须传入已标准化向量。

## 3. 文件长度与模块分层

- **单功能文件 < 500 有效行，多功能文件 < 1000**（`wc -l` 有效行口径；存量超限另行登记，不顺手扩）。
- 模块依赖**单向**，层级 L0 platform → L1 core/file → L2 graphics 资源 → L3 `_Vulkan` →
  L4 仿真 → L5 ui/input/audio/network/media/mmd。越级包含须进 `tests/lint/layers.sh` 白名单并写明原因。
- 全局/static 状态收敛进子系统上下文，再由 `sEngineContext` 门面持有（见 `docs/B7_STATIC_LEDGER.md`）；
  新字段进对应匿名 struct 组（`context_groups.sh` 守卫），不往顶层平铺。

## 4. 字符串与告警

- 禁用裸 `strcpy`/`strcat`（用 `memcpy` 精确长度 / `GN_StrCopyToLimit`）；`sprintf` 用 `snprintf`
  真实容量（契约/动态指针站点须在 `tests/lint/output.sh` 登记基线）。
- 不引入新告警；确需保留的告警（如按精确类型删除的 `delete-non-virtual-dtor`）须登记理由。
- 解析器（PNG/JPEG/glTF/PMD/PMX/WAV 等）改动须跑 `make fuzz`（ASan/UBSan）确认无崩溃。

## 5. ABI 纪律

- 破坏性符号变更（强符号 T/D/B 增删改）须登记 `abi/changes.md` 并更新 `abi/baseline.txt`。
- 同工具链前后对比：`nm -D --defined-only build-cmake/libGN_mini.so | awk '{print $2,$3}' | sort`。
- 弱符号（W/V）随工具链漂移须说明；**强符号零差异**是默认硬口径。
- 版本单一来源：`src/core/GN.h` 的 `GN_VERSION` / `GN_INTERFACE_VERSION`，`make version-check` 校验。

## 6. 提交粒度

- Conventional Commits：`type(scope):  subject`。type ∈ `feat/fix/refactor/build/test/docs/chore`；
  scope 用批次号或模块（如 `b9-3`、`b7-6`、`abi`）。
- **一批一提交**，停在绿构建（`make ci` 过）再提交；大批次拆子批，每子批独立可回滚。
- 提交信息写清「做了什么 + 实测数字 + 已知边界」；文档统计数字由 `make stats` 生成，不手改。
- 缺陷修复登记 `docs/DEFECT_ANALYSIS.md`（带「文件:行」锚点 + 可复算命令）；待办进 `docs/BACKLOG.md`。

## 7. 平台口径

官方**仅支持 Linux**（Vulkan + GLFW）。Windows/macOS/Android 分支代码保留但不维护、不测试、
不承诺可用（D3 裁决）。交叉编译检查 `make crosscheck` 为 experimental。新增平台代码按路径拆分。

## 8. 许可

根目录 `LICENSE`（MIT）。新增第三方文件须同步 `external/README.md`（来源 URL、版本、许可、用途）；
版本/许可无法确认的条目不得标「已核对」。发布包须含 `LICENSE` 与 `external/README.md`。