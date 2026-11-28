# 发布清单（RELEASE）

> **状态**：🚧 进行中 — 发布前核对清单。版本单一来源见 `src/core/GN.h`，`make version-check` 校验。
> 本文件随每次发布更新；ABI 基线与许可清单是发布的硬前提。

## 当前版本

| 项 | 值 | 来源 |
|---|---|---|
| 引擎主版本 `GN_VERSION` | **2026.07.04** | `src/core/GN.h`（单一来源） |
| 接口版本 `GN_INTERFACE_VERSION` | **2.2.0** | `src/core/GN.h` |
| 平台 | 官方仅 Linux（Vulkan + GLFW） | D3 裁决 2026-11-26 |
| 许可 | MIT | 根 `LICENSE` |

## ABI 基线（随版本提交）

- 基线文件：`abi/baseline.txt`（`nm -D --defined-only build-cmake/libGN_mini.so | sort`）。
- 变更登记：`abi/changes.md`（破坏性强符号变更必须登记）。
- **本版本基线**：4217 符号，对应提交 `03df5ab`（B10-4 后），工具链 GCC 13.3.0（容器）。
- 比较口径：按符号名（地址随布局变化不算变更）；**强符号 T/D/B 零差异**为默认硬口径，
  弱符号 W/V 随工具链漂移须说明（宿主作者基线为 GCC 16.2.1，见 `abi/changes.md` B7-2 登记）。
- 复算：`nm -D --defined-only build-cmake/libGN_mini.so | awk '{print $2,$3}' | sort` 与基线对比。

## 发布前核对清单

- [ ] `make ci` 全绿（check + smoke + doc-check + spv-check + lint-strict + warnings）。
- [ ] `make version-check` 通过；README/架构文档版本与 GN.h 一致。
- [ ] `make stats` 已刷新，文档统计数字与代码一致（doc-check 过）。
- [ ] ABI 基线 `abi/baseline.txt` 已随本版本提交；`abi/changes.md` 登记完整。
- [ ] 许可完整：根 `LICENSE`（MIT）+ `external/README.md` 第三方清单全部「已核对」。
- [ ] 过期文档已退役标注（`代码功能简述与规范.txt` 历史横幅、架构文档平台行）。
- [ ] `CONTRIBUTING.md` 与现行门禁一致。
- [ ] 发布包含 `LICENSE` 与 `external/README.md`。

## 许可完整性

| 项 | 状态 |
|---|---|
| 根 `LICENSE`（MIT，Copyright 2018-2025 gncreate Technologies Inc.） | ✅ |
| `external/` libpng（libpng/zlib 许可，文件头） | ✅ |
| `external/` libzip（zlib 式许可，zip.h 18-33 行） | ✅（历史遗留：zip.c 直接 include 编译，阶段 4 拆） |
| `external/` minimp3（CC0/public domain） | ✅ |
| `external/` soundcard.h（BSD-3-Clause 式） | ✅ |
| `external/` cgltf v1.15（MIT，文件尾全文，SHA-256 已核） | ✅ |
| `external/` WinSock2/Iphlpapi 兼容头（本项目 MIT） | ✅ |
| `external/` AL/（OpenAL Soft，Debian libopenal-dev） | ✅ **已核对**：LGPL-2.0-or-later，许可全文 `AL/COPYRIGHT`；2026-11-28 由 Creative Labs 旧头替换，无引用旧头已移除 |

## 已知边界（如实披露）

- **平台**：仅 Linux 官方支持；Win/macOS/Android 分支保留但不维护、不测试（experimental）。
- **告警**：src 主位置余 7 条登记保留（`delete-non-virtual-dtor`×5 按精确类型删除、`cast-function-type`×2 往返转型）。
- **字符串**：余 9 处 `sprintf` 契约/动态指针站点登记基线（HTTP body 预计算缓冲×7、itoa、DeviceInfoRam）。
- **GPU/真机项**：RT GI 硬件变体、视觉 A/B 基线、UI Vulkan 适配等需 Vulkan 设备/资产，无设备不标 done（见 `BACKLOG.md`）。
- **负向测试 UBSan 噪声**：Material/PhysicsJoint/Visibility 故意传非法枚举验拒绝，生产校验正确（`make asan` 不在 ci）。

## 发布阻塞项

无。AL 头许可已于 2026-11-28 解决（替换为 OpenAL Soft 带许可头，LGPL-2+，见 `external/AL/COPYRIGHT`）。

> 全部核对项已落地，可按本清单发布。