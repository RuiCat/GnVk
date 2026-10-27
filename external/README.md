# external/ — 第三方与兼容头清单

本目录只放编译期需要的第三方头文件/源码。**版本与许可以各文件头部声明为准**；
标"待核对"的条目在发布前必须补齐来源、版本与许可文本。

| 目录/文件 | 内容 | 上游与版本（文件内声明） | 许可 | 备注 |
|---|---|---|---|---|
| `AL/` | OpenAL 头：`al.h`、`alc.h`、`efx.h`、`alu.h`、`alut*.h` 等 | OpenAL / OpenAL Soft；版本未标注 | 上游为 LGPL-2.0-or-later；头文件许可**待核对** | 仅声明接口，运行期链接系统 `libopenal` |
| `libpng/` | `png.h`、`pngconf.h`、`zconf.h`、`zlib.h` | libpng 1.2.4（2002-07-08，`png.h` 首行给出） | libpng/zlib 许可（见文件头） | 运行期链接系统 `libpng16`，此处仅编译期使用 |
| `libzip/` | `zip.c`/`zip.h`、`unzip.c`/`unzip.h` | Gilles Vollant，zip 0.15 alpha / unzip 0.15 beta（1998）；2011/2012 有本地修改注释 | zlib 式许可（见 `zip.h` 第 18-33 行） | **已知技术债**：`src/core/GN_Format.cpp` 以 `#include<libzip/zip.c>` 直接编译 C 源（计划阶段 4 拆为独立编译单元） |
| `minimp3.h` | MP3 解码单头 | lieff/minimp3；版本未标注 | CC0 / public domain（文件头声明） | 仅头文件 |
| `soundcard.h` | OSS 音频 API 头 | Copyright Hannu Savolainen 1993 | BSD-3-Clause 式（文件头三条款） | 仅 Linux 音频路径使用 |
| `cgltf.h` | glTF 2.0 解析单头（含内置 jsmn JSON 解析） | jkuhlmann/cgltf **v1.15**（`https://raw.githubusercontent.com/jkuhlmann/cgltf/v1.15/cgltf.h`）；SHA-256 `e378a21c084bf1f288bb799de827bb26906efb024255f1ecf1705ea13f11c6ec` | MIT（文件尾完整许可文本） | 静态审查（2026-10-28）：无网络/子进程/凭据读取/写文件，仅 `fopen(path,"rb")` 读；实现宏在 `GN_3Dasset.cpp` 定义一次 |
| `WinSock2.h`、`Iphlpapi.h` | 大小写兼容包装头（4 行） | 本项目自写，`#include <winsock2.h>` / `<iphlpapi.h>` | 本项目 MIT | 仅 Windows 交叉编译路径使用；非上游代码 |

## 维护规则

1. 新增第三方文件必须同时更新本表：来源 URL、版本、许可、用途。
2. 不允许把 `.c` 源文件长期放在 `include/` 下（`libzip` 为历史遗留，阶段 4 处理）。
3. 发布包必须包含本文件与根目录 `LICENSE`。
4. 版本/许可无法确认的条目，不得标记为"已核对"。
