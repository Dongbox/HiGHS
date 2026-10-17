# 首轮证据与未知问题

记录日期为 2026-10-17。以下结论来自本机 MindOpt 2.3.0 arm64 文件及 HiGHS 指定提交，没有进行代表性性能对比。

| 观察 | 依据 | 能支持的结论与限制 |
| --- | --- | --- |
| CLI 入口是一个跳转包装层 | Ghidra 12.1.4：`ev_780e46dcce2cbaf3fd86e9d15fa0656fff9d87bf81d41e98ddf6486ca99691c3`；入口 `0x100003fa8` 跳至 `_MDOstartcmd` stub `0x100003fac` | CLI 入口本身不是求解内核。反编译器对间接跳转给出警告，恢复的函数签名可信度低 |
| CLI 动态依赖核心库，并导入 `_MDOstartcmd` | `ev_f5c2e0d51623c998c4e8fb89fcfe1b16187186deb8d47354da2169b60d72ecfc`；`cli-imports.json` 与 `cli-dependencies.json` | 将后续 CLI 路径分析定位到核心库；不代表完整运行时调用图 |
| 核心库的本次清单返回 122 个导出、519 个导入 | `ev_7ed84980a58a60998c72644cd2bd0e2d2ec54934fa05859c82a8e243414b000a` | 导出包括 `_MDOoptimize`（`0x275c0`）、`_MDOnewmodel`、`_MDOstartenv` 等；原工具标注导入/导出非穷尽，不能用未发现证明不存在 |
| 核心库导入 pthread、C++ thread、`dlopen` 和 `dlsym` | `core-imports.json`，视图 Evidence `ev_7d693fd3f0d0d40555e1dd450229be08738f6505151bc913c2cb6ca862270a1e` | 提供线程与动态加载的追踪入口；不能据此断言线程用于并行搜索，也不能认定特定模块确被加载 |
| SDK 中有核心、MILP、MIQCP、power 等多个 dylib | `artifact-manifest.json`，仅记录路径、大小和 SHA-256 | 目前只是文件边界；模块选择规则与职责仍需分析 |
| HiGHS 当前基线已实现并行 MIP 和任务窃取 | `highs/mip/HighsMipSolver.h:177`、`highs/parallel/HighsTaskExecutor.h:59` | 阶段 2 应先测量现有实现；不能将差距预设为“缺少并行模块”。启用条件与源码地图见阶段 2 文档 |

核心库 SHA-256：`78c2d58ae919e2688e124703f950739b6f0b7b8eb12b15ee566843ff3078ad62`。CLI SHA-256：`6588e8f85db910a933925edb72d568d325236ef9e6e46fe6887d8bdee5ca8e45`。完整清单位于本地 `$MINDOPT_RESEARCH_ROOT/analysis/phase1/artifact-manifest.json`。

REA 原始记录保存在 `$MINDOPT_RESEARCH_ROOT/analysis/phase1/evidence/`，包括 CLI 会话的完整 bundle `20261017-cli-inventory.json`，以及核心库和 CLI 按主题保存的精确 Evidence 视图。CLI 分析快照位于 `analysis/phase1/snapshots/mindopt-cli.json`。分析会话已关闭，可在后续分析时复用快照。

本机初始功能验证通过 CLI AFIRO、Python LP 与 MILP，目标值分别约为 `-464.75314286`、`8.5`、`8.0`。这些小算例不能说明性能领先或线程扩展性。初始 MILP 日志报告 `root=8, tree=2`，即使 Python 设置了 `NumThreads=2`；基准必须检查实际各阶段线程预算，不能只记录一个总线程参数。

下一步先解析 `MDOoptimize` 到 LP/MILP 的分派路径，确认并发参数及其生效范围，再用非平凡模型测量。是否存在共享搜索树、portfolio、work stealing、并行 cuts 或特殊数值内核，当前均未得到证实。
