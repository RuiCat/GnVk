# B9 质量门全量（c7）执行账本

> **状态**：🚧 进行中 — B9-1/B9-2/B9-3/B9-4 ✅（去 -fpermissive + C++17 + 告警 19385→7 + 字符串安全）；B9-5 待做
> **上游**：`docs/REFACTOR_PLAN.md` §5 阶段5 / §11 B9；依赖 B7/B8（已完成）。
> **开工基线**：`ab9264e`（2026-11-27，单测 955/0）；快照 `snap-20261128-013049`。

## 目标（验收口径）

| 项 | 起点 | 目标 | 机检 |
|---|---|---|---|
| `-fpermissive` | 引擎/测试均带 | 移除，无该标志可编 | CMakeLists 无该标志 + 全量构建 0 error |
| C++ 标准 | C++11 | C++17（Linux-only，D3） | `CMAKE_CXX_STANDARD 17` + 单测/ABI 通过 |
| 扩展警告 | 19385（2026-11-26 语法检查口径） | <500 | `make warnings` 门禁 |
| 字符串安全 | src 内 strcpy/strcat/sprintf 128 处 | 全仓清零（契约型除外需登记） | lint 门禁 0 命中 |
| 解析器 fuzz | 无 | PNG/ZIP/WAV/模型加载本地 fuzz 1 小时无崩溃 | `tests/fuzz/` 可复跑 |

## 批次（串行，每批独立提交，停止点保证绿构建）

| 批 | 范围 | 状态 |
|---|---|---|
| B9-1 | 修 9 处非法转换 + 移除 `-fpermissive`（保持 C++11） | ✅ 2026-11-28，ci 全绿 955/0，同工具链 ABI diff=0 |
| B9-2 | 升级 C++17（全仓切换；ABI/单测/冒烟验证） | ✅ 2026-11-28，ci 全绿 955/0，ABI diff=0；register×5 清除；headers lint 同步 17 并加基线门禁 |
| B9-3 | 告警大扫除（reorder/ignored-qualifiers/mfi/deprecated/shadow/write-strings/sign-compare/switch/unused/narrowing/format 等）+ `make warnings` 门禁 | ✅ 2026-11-28，19385→7（余 7 为已登记保留：delete-non-virtual-dtor×5 §5.2 + cast-function-type×2 DA-1126-15）；门禁入 ci |
| B9-4 | 字符串安全：strcpy/strcat 62 处→memcpy 精确长度（清零）；sprintf 56 处→snprintf 真实容量；余 9 处契约/动态指针登记 | ✅ 2026-11-28，output.sh：strcpy/strcat 零容忍 + sprintf 基线 9 |
| B9-5 | 解析器 fuzz：PNG/ZIP/WAV/glTF/OBJ 边界加固 + fuzz 种子 + 1 小时无崩溃 | 待做 |

## B9-1 扫描结论（2026-11-28，GCC 13.3，190 cpp 全量 -fsyntax-only）

- C++11 无 `-fpermissive`：**9 error**；C++17 无 `-fpermissive`：**同样 9 error**；C++17 带 `-fpermissive`：0 error。
- 9 处全部为隐式非法转换，修复均为保行为显式化（不改公共签名）：

| # | 位置 | 问题 | 处置 |
|---|---|---|---|
| 1 | `src/core/GN_Init.cpp:445` | `const int* window` → `GN_InitGraphics(void*)` 丢 const | 调用点 `(void*)window`（与旧隐式转换同行为） |
| 2 | `src/graphics/GN_2D.cpp:54` | `GT_ReSetRenderView2D(this, true)`：指针截断成 short | 该函数是空操作兼容桩（GN_Main.h:114，参数全部 (void)）；改无参调用，行为不变；登记潜在缺陷 |
| 3 | `src/graphics/GN_2D.cpp:462` | `img->cFormat`(int) → `SavePngFile(COLOR_FORMAT)` | 显式 `(GNg::COLOR_FORMAT)` |
| 4 | `src/graphics/GN_3Dasset.cpp:225` | cgltf `void* data` → `const uint8_t*` | 显式 `(const uint8_t*)` |
| 5 | `src/input/GN_Input.cpp:912` | `GN_GetSoftKeyboardRange()` 返回 void* → sRECT* | 显式 `(GNg::sRECT*)` |
| 6 | `src/platform/_Sys/sysMutexIo.inc:231-237` | `static const char sysInfo[64]` 却被写入（cast 掉 const） | 改 `static char`（与 _MAC_ 分支一致），去掉 (char*) cast |
| 7 | `src/ui/GN_UiFBrowser.cpp:37` | 宏内 `ImgFmtGetByteNum(IMG->cFormat)` int→枚举 | 显式 `(GNg::COLOR_FORMAT)` |
| 8 | `src/ui/GN_UiFBrowser.cpp:1469` | `DataFill(..., _icon_ids[_i], ...)`：ushort 值被当 `ushort*` | 改传 `&_icon_ids[_i]`（Vulkan 后端 (void)texid，行为不变；登记潜在缺陷） |
| 9 | `src/ui/uiPalette.inc:392` | `Draw2DAlphaDirectionGet()` 返回 uchar → eNEAR_TYPE | 显式 `(GNu::eNEAR_TYPE)` |

## B9-4 字符串安全登记（2026-11-28）

- **strcpy/strcat → memcpy（62 处，清零）**：字面量源用 `sizeof("…")`（含 NUL），非字面量用
  `strlen(src)+1`；均为 STRCPY_AUDIT 已核「精确分配 / 入口守卫 / 契约」站点，行为逐字节等价。
  转换用平衡括号扫描器（首版正则在嵌套括号站点误匹配，已废弃重做并复核）。
- **sprintf → snprintf（56 处）**：按站点真实容量（固定数组 sizeof；偏移写入用 总容量−偏移；
  P<char> 动态缓冲用 New 分配尺寸；`char url_out[384]` 参数契约用 384）。snprintf 返回「应写长度」，
  无截断时与 sprintf 返回一致（这些站点容量均保证不截断）。
- **余 9 处 sprintf 登记契约/动态指针（output.sh 基线=9，只防新增）**：
  `GN_NetTrans.cpp` 406/423/541/787/895/897/898（HTTP body 写入预计算容量的 w_pos/zContent 指针，
  容量由 GT_HttpPostGetOccupancySize 预先算定，本地无容量参数）；
  `sysEncoding.inc` itoa(_DstBuf)（调用方缓冲契约）；`sysMutexIo.inc` GN_GetDeviceInfoRam(info)（调用方缓冲契约）。
- **门禁**：`tests/lint/output.sh` 增 strcpy/strcat 零容忍检查；sprintf 基线 65→9（登记上述）。

## 复算命令

```bash
. /work/.sbx/gnvk-env.sh   # 容器内依赖 sysroot（vulkan/glfw 等头与库）
# 无 -fpermissive 全量语法扫描（B9-1 前 9 error，修复后应 0）
find src -name '*.cpp' | xargs -P2 -I{} g++ -std=c++11 -fPIC -Wall -Wextra -msse2 \
  -include cctype -fsyntax-only <DEFINES> <INCLUDES> {}
make ci        # 绿构建门禁
make warnings  # B9-3 起：告警预算门禁
```