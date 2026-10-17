# HiGHS 源码地图

核对基线：`0d615a2bf9548ce0fe6c97b2a37d19e0aa9bbd6e`（fork 的 `latest`，HiGHS 1.15.1）。路径相对仓库根目录；行号只对应此提交。以下为静态源码入口，不是性能瓶颈结论。

| 领域 | 文件与行号 | 后续研究用途 |
| --- | --- | --- |
| 任务调度 | `highs/parallel/HighsTaskExecutor.h:59`、`HighsParallel.h:73` | 随机任务窃取、任务组 spawn/wait |
| MIP 并行启用 | `highs/mip/HighsMipSolver.h:177` | `parallel=on` 且非 sub-MIP 时使用多 worker；其他情况为 1 |
| MIP 并行编排 | `highs/mip/HighsMipSolver.cpp:76,295,372,475` | 分派、worker 创建、cut pool 同步和每 worker 100 节点的批次预算 |
| 搜索与分支 | `highs/mip/HighsSearch.cpp:879,1809`、`HighsNodeQueue.cpp:388` | 节点评估、深度优先处理与节点选择 |
| 原始启发式 | `highs/mip/HighsPrimalHeuristics.cpp:419,668,1568` | RENS、RINS、feasibility pump 的工作预算与触发时机 |
| Cuts | `highs/mip/HighsSeparation.cpp:26,46,183` | 分离器注册、分离轮次及 cut/resolve 循环 |
| 传播与节点 LP | `highs/mip/HighsDomain.cpp:2629`、`HighsLpRelaxation.cpp:1125,1441` | 变量域传播、节点 LP 求解和重求解 |
| Simplex | `highs/simplex/HEkk.cpp:1001`、`HEkkDual.cpp:28`、`HEkkPrimal.cpp:20` | 算法分派及 primal/dual 入口 |
| 并行 simplex | `highs/simplex/HEkkDualMulti.cpp:201,491`、`HEkkDual.cpp:1833` | dual multi-iteration 与分片 pricing |
| IPM | `highs/ipm/IpxWrapper.cpp:29,408`、`highs/ipm/hipo/ipm/Solver.cpp:143,178,393` | IPX/HiPO、IPM 迭代与 Newton 系统求解 |
| Presolve | `highs/presolve/HPresolve.cpp:3860,4492` | 简化循环、状态编排与 postsolve 正确性 |
| 稀疏基分解 | `highs/util/HFactor.cpp:356,463,491,516` | factorization、FTRAN、BTRAN 和增量更新 |
| IPM 并行分解 | `highs/ipm/hipo/factorhighs/Factorise.cpp:104,270`、`HybridSolveHandler.cpp:207,395` | 超节点任务、消元树及并行前后代入 |

CLI 入口为 `app/RunHighs.cpp:42`。CTest 单元测试由 `check/CMakeLists.txt:184` 注册，实例回归见同文件 `:420`。

相关测试入口包括 `check/TestHighsParallel.cpp:233,241,357`、`TestMipSolver.cpp:26,997`、`TestLpSolvers.cpp:369,406`、`TestIpx.cpp:44`、`TestHipo.cpp:46`、`TestPresolve.cpp:19,94` 和 `TestFactor.cpp:23`。现有回归测试不能替代统一性能 harness。
