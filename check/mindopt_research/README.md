# MindOpt 研究辅助工具

工具独立于 HiGHS 默认构建，不向默认 CMake/CTest 增加商业求解器依赖。要求本机 MindOpt 2.3.0 Apple Silicon SDK、有效社区版联网许可证及 Python 3.13 环境。

默认读取与 HiGHS 仓库同级的 `mindopt/`。可通过 `--mindopt-root` 或环境变量 `MINDOPT_RESEARCH_ROOT` 指定其他路径，运行产物默认写入该目录。

在 `mindopt-highs/` 工作区根目录执行：

```bash
source env.sh
python highs/check/mindopt_research/verify_installation.py
python highs/check/mindopt_research/collect_inventory.py
```

`verify_installation.py` 验证 CLI AFIRO、Python LP 和 MILP 的最优状态与预期数值。它正常使用联网授权，在保存和显示 stdout/stderr 前屏蔽许可证的凭据，并显式释放 Python 模型和 Env。结果写入 `mindopt/verification/`。这是一项安装功能检查，不是性能基准。

`collect_inventory.py` 只读取列明的 SDK 文件，记录哈希、大小和非敏感 CPU 元数据，不读取许可证。输出为 `mindopt/analysis/phase1/artifact-manifest.json`。它针对本研究固定的 2.3.0 arm64 SDK，并非通用安装扫描器。

若需要重建环境，在工作区根目录执行：

```bash
uv venv --python 3.13 mindopt/.venv
uv pip install --python mindopt/.venv/bin/python \
  ./mindopt/2.3.0/osx64-aarch/lib/python \
  -r highs/check/mindopt_research/requirements.txt
```

REA 操作通过连接的 REA 工具进行。完整 Evidence bundle、精确视图和快照保存在本地 `mindopt/analysis/phase1/`，研究摘要与下一步问题见 [研究文档](../../docs/research/mindopt/README.md)。
