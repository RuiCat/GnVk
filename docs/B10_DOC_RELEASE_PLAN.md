# B10 文档与发布账本（c8）

> **状态**：✅ 完成 — B10-1..B10-5 全落地（版本门禁/CONTRIBUTING/README/退役标注/RELEASE 清单）。唯一发布阻塞：AL 头许可待核对（需用户决策）。
> 依据：`REFACTOR_PLAN.md` 阶段 6 + C14（版本单一来源，硬门禁）。不做站点。

## 口径

- 版本单一来源已立：`src/core/GN.h` 的 `GN_VERSION`（引擎主版本）/ `GN_INTERFACE_VERSION`（接口版本）。
  前批已把架构文档版本改为引用 GN.h。本批补 `make version-check` 门禁（C14）并核对全仓一致。
- 文档不撒谎：README/架构文档与代码树同步；过期段落标注退役而非静默删除。
- 许可可追溯：根 `LICENSE`（MIT）+ `external/README.md` 第三方清单；发布前补齐「待核对」。

## 批次

| 批 | 范围 | 状态 |
|---|---|---|
| B10-1 | 版本单一来源门禁 `make version-check`（C14）+ 入 doc-check | ✅ 2026-11-28，GN_VERSION 2026.07.04 / Interface 2.2.0，架构文档引用 GN.h，README 无冲突 |
| B10-2 | `CONTRIBUTING.md`（命名/文件长度/提交粒度/测试要求） | ✅ 2026-11-28，8 节：构建门禁/命名/长度分层/字符串告警/ABI/提交粒度/平台/许可 |
| B10-3 | README 核对 + make 目标表 + 目录结构同步 | ✅ 2026-11-28，修 ci/smoke 过时描述、补 mmd/tests/abi/tools 目录、加 15 目标速查表 |
| B10-4 | 过期文档退役标注（代码功能简述与规范.txt）+ external AL 许可核对 | ✅ 2026-11-28，txt 加历史横幅（命名仍有效/跨平台口径被 D3 取代）、架构平台行修正、AL 头判定为 Creative Labs OpenAL 1.1 SDK（许可剥离，登记发布阻塞） |
| B10-5 | `RELEASE.md` 发布清单 + ABI 基线随版本 + 许可完整性收尾 | ✅ 2026-11-28，RELEASE.md（版本/ABI 基线 4217 符号绑定/许可清单/已知边界/发布阻塞）；AL 头许可待核对为唯一发布阻塞 |

## 复算命令

```bash
make version-check     # 版本单一来源一致性
make doc-check         # 文档统计 + 索引 + 版本
make ci                # 全门禁
```