# MindOpt 分析与 HiGHS 性能改进

目标是先解释 MindOpt 在相关工作负载上的并发与求解表现，再在 Dongbox/HiGHS fork 中独立实现、测量和验证改进。能否达到接近的性能由统一基准决定，目前尚无性能对比结论。

工作分为两个阶段：

| 阶段 | 工作 | 完成条件 | 当前状态 |
| --- | --- | --- | --- |
| 1 | 用 REA 分析 MindOpt 的求解入口、模块边界、并发路径和热点，建立性能基线 | 关键结论有可追溯证据及运行验证，形成可检验的改进假设 | 已完成安装验证、目标指纹、首轮静态清单及 CLI 入口反编译；核心算法与性能分析待开展 |
| 2 | 将已验证的设计假设映射到 HiGHS，逐项实现和比较 | 正确性回归通过，代表模型上有可复现收益 | 仅建立源码地图与实施约定，尚未修改求解算法 |

阶段 1 的交付是带证据的架构说明、并发机制说明、性能报告及未知问题清单。反编译输出属于工具生成的伪代码，不能视为恢复了原始源码。阶段 2 的实现基于抽象设计、公开算法和 HiGHS 自身代码；原始反编译材料留在本地研究目录。

本地目录采用同级布局：

```text
mindopt-highs/
├── README.md
├── env.sh
├── mindopt/                         # 本地 SDK 与实验环境，不属于 HiGHS Git 仓库
│   ├── 2.3.0/
│   ├── ce_license.ini               # 权限 600，不提交
│   ├── .venv/
│   ├── analysis/phase1/
│   │   ├── artifact-manifest.json
│   │   ├── evidence/                # REA Evidence 原始记录与 bundle
│   │   ├── snapshots/               # 可复用的 REA 分析快照
│   │   └── decompilation/           # 后续函数级伪代码与分析材料
│   ├── benchmarks/instances/       # 模型数据，以 manifest 和哈希确定样本
│   ├── benchmarks/runs/            # 运行日志、结果和 profiling
│   └── verification/
└── highs/                           # Dongbox/HiGHS Git 仓库
    ├── docs/research/mindopt/        # 计划、证据摘要、研究结论和设计
    ├── check/mindopt_research/       # 本次新增的复现工具
    └── highs/                       # 后续阶段 2 修改的求解器源码
```

研究分支为 `codex/mindopt-performance-research`，从 fork 的 `latest` 分支提交 `0d615a2bf9548ce0fe6c97b2a37d19e0aa9bbd6e` 建立，版本为 HiGHS 1.15.1。后续若更新基线，应单独记录提交及构建信息。

阅读顺序：[阶段 1 计划](phase1/plan.md)、[首轮发现](phase1/findings.md)、[基准规范](benchmark-protocol.md)、[HiGHS 源码地图](phase2/highs-source-map.md)、[阶段 2 计划](phase2/plan.md)。工具说明见 [check/mindopt_research](../../../check/mindopt_research/README.md)。

默认只提交研究文档、复现工具和 HiGHS 自有代码改动。SDK、许可证、虚拟环境、大型模型、原始 REA bundle 和运行产物保存在相邻的 `mindopt/` 目录。
