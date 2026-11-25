# Buffer-size TODO 审计（B5f，2026-11-21 收口）

> **状态**：📊 快照 — 2026-11-21 的 buffer-size TODO 收口：36 → 0 处；逐条给出核销/加固/契约型理由。

> 口径：`grep -rn "TODO: verify buffer size" src` 起点 **36 处**（2026-11-21），收口 **0 处**。
> 每一处都给出判定：**已核销**（改动/边界证明）、**已加固**（改成带容量上限的拷贝）、
> 或**契约型**（调用方必须提供足够空间，改成显式契约注释，不再以 TODO 悬挂）。

## 1. 加固与核销（29 处）

| 位置 | 原写法 | 处置 | 证据 |
|---|---|---|---|
| `GN_String.cpp` 段删除函数（5 处） | `strcpy(pos0, pstr/pos1)` | 同缓冲区、`dest≤src`，长度 = `strlen(src)+1`；重叠 `strcpy` 是 UB → 改 `memmove` | 探针 8 组输入输出改前/改后一致；`stringRemoveSegment*` 4 单测 |
| `GN_StrToPrintFmt`（2 处） | `strcpy` | 分配 `fn+4+n_n+ulen`，最后写入索引恰为 `S-1` → 注释「已核」 | 手算 + 既有 format 用例 |
| `GN_StrInsertSegment`（1 处） | `strncpy` | `new char[inpos+sub_len+尾部+1]` 恰填满 → 注释「已核」 | 分配式与写入长度逐项相等 |
| `GN_UiPopDlg` pointer 拷贝（5 处） | `strcpy((char*)pointer, …)` | 新增 `GN_StrCopyToLimit`，容量取 `chd.maxValue` | `tests/unit/testString.cpp` 的 `stringCopyToLimit` |
| `GN_UiPopDlg` enumIds（1 处） | `strcpy((char*)enumIds, tbuf)` | 容量 `(tlen+1)*2` 字节，改带上限拷贝 | 同上 |
| `GN_UiListGui` 文件名编号（1 处） | `strcpy(number_pos+1, ".png")` | 容量 = `sizeof(str_buf) - 偏移`，改带上限拷贝 | 同上 |
| `GN_UI` 导出声音文件名（1 处） | `strcpy(_filename, …)` | `_filename` 指向 `exp_path[384]` 内部，容量 = 余量，改带上限拷贝 | 同上 |
| `GN_UiPopDlg`/`GN_UiListGui`/`GN_UiListTree`/`GN_UI` 的精确分配（12 处） | `strcpy` 到 `new char[len+1]`、`new char[di+5]`、`P<char>(n+1)` | 分配式与写入长度逐项相等 → 注释「已核」 | 逐处分配表达式 |

## 2. 转为显式契约（7 处）

| 文件 | 数量 | 契约 |
|---|---:|---|
| `src/core/GN_String.cpp` | 3 | `GN_StrUnSplit`（2 处宏内）与 `GN_StrToKey` 类函数把结果写进**调用方缓冲区**；注释写明「调用方须提供足够空间」 |
| `src/platform/_Sys/GN_Sys.cpp` | 4 | `strncpy(..., 383)/(...，255)` 后手动补 NUL：调用方缓冲区 ≥384 / ≥256 字节（ABI 级契约，改签名会波及调用方） |

## 3. 新增 API

```c
// GN_String.h
// 带容量上限的安全拷贝：最多写 cap-1 字节并补 NUL；cap<=0 或 dst 为空返回 0 不写。
GN_API int GN_StrCopyToLimit(char *dst, int cap, const char *src);
```

## 4. 复算命令

```bash
cd /home/ruicat/下载/GnVk/GnVk
grep -rn "TODO: verify buffer size" src | wc -l   # 0
make unit                                        # 420 cases / 0 failed
# 行为对拍探针（只读，不参与构建）
g++ -O0 -std=c++11 -fpermissive -DGN_USING_SOURCE_CODE -DGN_USING_VULKAN_GLFW_LINUX \
  -Isrc -Isrc/core -Isrc/graphics -Isrc/graphics/_Vulkan -Isrc/audio -Isrc/input -Isrc/ui \
  -Isrc/network -Isrc/file -Isrc/platform -Isrc/media -Isrc/plugin -Iexternal/include \
  -Isrc/platform/_SysDef .local/probes/probeRemove.cpp -o .local/probes/probeRemove -L. -lGN_mini
LD_LIBRARY_PATH=. .local/probes/probeRemove
```

## 5. 残留风险

- 7 处契约型在运行时**不做长度校验**；一旦调用方给短了仍会写穿。彻底消除需改 API 签名（`_s` 变体）
  或调用方全局替换，属独立批次。
- UI 输入控件的 `maxValue` 与 `pointer` 实际容量必须由创建方保持一致（当前由 `InitValueToData` 的
  `text_buf_size` 参数保证）；新增调用点需遵守该契约。