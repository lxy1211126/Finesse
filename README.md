# Finesse 复现实验

这是一个独立的 C++17 实现工作区，用于开展 Finesse 与 N-transform 的相似特征（SF）实验，不依赖 Destor。现有的 `fastcdc/` 和 `kruus/` 项目保持不变。

## 阶段一

阶段一实现了 48 字节 Rabin 滚动哈希和 Rabin 内容定义分块（CDC），期望块大小为 8 KiB。分块参数从 `configs/rabin.params` 读取。最小块和最大块大小分别为 512 B 和 64 KiB，与仓库中 vendored 的 Destor 配置一致。

CDC 切分条件是将指纹最低的 `log2(expected_size)` 位与配置的 breakmark 比较。块数和平均块大小由输入数据决定；较小的输入可能只产生一个较短的尾块。

构建与测试：

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j2
ctest --test-dir build --output-on-failure
```

使用现有的冒烟测试样本运行：

```bash
./build/src/run_chunk_only \
  --input data/syn/test-smoke-00_base.bin \
  --config configs/rabin.params
```

该样本是合成数据，仅用于冒烟测试，不能替代 Linux 内核 tar 工作负载或论文原始数据集。

## 阶段二

`FinesseExtractor` 为每个固定子块计算一个最大的 32 位 Rabin 指纹。`NTransformExtractor` 扫描整个块中的 48 字节 Rabin 窗口，并对每个配置的变换计算 `max((m_i * FP + a_i) mod 2^32)`。固定变换参数位于 `configs/ntransform.params`；它们由 `std::mt19937` 使用种子 `20261119` 生成，每个特征依次读取一个乘数和一个加数。默认情况下，两种提取器均返回 12 个特征。

进行敏感性测试时，`run_ntransform --ntransform-seed UINT32` 会根据指定种子重新生成变换参数；未指定该选项时，则原样使用仓库中已保存的参数。

Finesse 对每个块计算连续的 Rabin 滚动指纹流，然后将每个窗口指纹分配给其结束字节所在的子块。这样会保留跨子块边界的窗口，对应论文伪代码 `RabinFunction(Str, position)`。子块大小为 `floor(chunk_length / 12)`；与算法 2 一样，末尾余数会被忽略。

检查输入第一个 Rabin CDC 块的特征：

```bash
./build/src/run_feature_check data/syn/test-smoke-00_base.bin
```

## 阶段三

Finesse 将 12 个特征划分为 4 组连续的三元组。每组的最大值构成 SF0，第二大值构成 SF1，第三大值构成 SF2。N-transform 则将连续特征按每组 4 个进行分组。

每个 32 位特征会编码为 4 个大端字节，从零状态开始按最高有效位优先的顺序，使用独立的 32 位 Rabin 配置 `x^32 + 0x04c11db7` 进行处理（无最终异或）。该配置与 64 位 CDC Rabin 配置相互独立。当前 64 位 SF 索引接口会将 32 位结果零扩展。`run_sf_check` 会打印选定的组成员和哈希值：

```bash
./build/src/run_sf_check
```

Figure 3 中的分组方式已实现，但打印出的哈希值与论文不一致。使用上述明确配置时，三个 SF 哈希值为 `0x3dda360f`、`0xfbbdf144` 和 `0x90cdfa3f`；论文给出的值为 `0xf794de5e`、`0x4b6f535e` 和 `0xd07267d6`。论文没有说明 SF 哈希所用的多项式、位宽、初始化、序列化或终结方式，作者的 SF 实现也尚未得到验证。这里采用的配置使实现具有确定性和可移植性；这是明确记录的工程选择，不代表逐位复现了论文中的 SF 哈希。

## 阶段四

处理流水线包含 SHA-256 精确去重、使用 FirstFit 的 SF 查找、通过内存 API 调用 xdelta3 3.0.11 进行 VCDIFF 编码、每次运行进行一次样本解码往返校验，以及 LRU 基块缓存。内存 API 避免了逐块启动进程和使用临时文件的开销，同时保留 xdelta3 格式和默认压缩设置。合成冒烟测试会覆盖所有观察到的匹配；完整工作负载计时只解码一个样本，因为恢复解码不属于备份/压缩吞吐量的测量范围。

DCR 定义为 `去重后唯一输入字节数 / 存储的原始数据与差分数据总字节数`。DCE 会针对所有 SF 匹配目标报告两种形式：

- `dce_residual_fraction`：`(存储的差分字节数 + 原始数据回退字节数) / 匹配目标字节数`
- `dce_savings_fraction`：`1 - dce_residual_fraction`

将原始数据回退计入，可以避免因排除未能从差分编码中获益的候选匹配而夸大 DCE。论文给出了 residual fraction 的公式，但同时又称 DCE 越高意味着误报越少，两者存在矛盾。报告时应同时给出两种形式，并说明采用的解释方式。如果没有 SF 匹配目标，这两个值均为空。

SF 速度只统计特征提取和 SF 生成阶段处理的字节数。系统吞吐量则按端到端运行时间计算全部输入字节。

仓库中的 xdelta3 3.0.11 源码及其 GPL 许可证位于 `third_party/xdelta3/`；CMake 会链接系统 LibLZMA。通过 `--xdelta /path/to/xdelta3` 仍可使用命令行版本作为回退方案。运行器默认使用 `/tmp` 存放命令行方式所需的临时文件；可设置 `TMPDIR` 更改临时目录。实验脚本会在运行前校验清单中的每个条目，因此临时数据清理后遗留的清单会导致脚本提前报错。

流水线以只读方式内存映射输入，并将基块位置存为源文件偏移量；基块字节优先从 400 MiB 的 LRU 缓存读取，未命中时直接从映射文件读取。

代码按职责拆分：`io/` 负责映射输入；`chunking/` 和 `rolling_hash/` 生成 CDC 块；`dedup/` 执行精确块查找；`feature_extraction/` 和 `super_feature/` 计算相似性签名；`similarity/` 选择候选基块；`cache/` 和 `storage/` 管理基块数据；`delta/` 对原始数据和差分数据进行编码、写入，并生成 TSV 索引；`pipeline/` 协调各模块并收集运行指标。

指定 `--store-dir DIR` 会启用 `delta/` 输出文件（`blocks.bin` 和 `index.tsv`）；不指定时只运行指标统计。该实验产物会存储唯一块的数据和逐块存储元信息，但目前尚未包含完整的文件配方或精确重复块引用，因此不是可独立恢复的备份格式。

候选选择策略默认为论文风格的 `first-fit`。消融实验可使用 `--candidate-policy best-delta`：针对每个不同的 SF 匹配基块进行编码，并保存小于原始目标数据的最小差分。对比两种方法时应使用相同策略；`best-delta` 结果不是 FirstFit 复现结果。

指标会记录策略、存在候选基块的块数、不同候选基块引用总数、存在候选的块所对应的字节数、没有候选时直接存储的原始字节数，以及存在候选但所选策略未找到更小差分时回退存储的原始字节数。扩展后的指标格式需要使用新的 CSV 路径，不能追加到旧格式 CSV 文件。

```bash
./build/src/run_finesse \
  --manifest data/files_smoke.txt \
  --dataset mini-smoke --run-id 1 \
  --csv results/processed/metrics.csv \
  --log results/logs/finesse/mini-smoke-1.log \
  --store-dir results/stores/mini-smoke-run-1
./build/src/run_ntransform \
  --manifest data/files_smoke.txt \
  --dataset mini-smoke --run-id 1 \
  --csv results/processed/metrics.csv \
  --log results/logs/ntransform/mini-smoke-1.log
```

## 阶段五

数据脚本会下载并校验连续的 2 到 10 个 Linux 5.10.x tar.xz 压缩包，并将其解压为 tar 数据流。默认下载三个版本。在磁盘空间有限的机器上准备十个版本时，可将压缩包保留在磁盘上，并将 tar 数据流解压到容量足够的 tmpfs：

```bash
scripts/download_linux_tar.sh data/tar/linux-5.10/raw 10
scripts/prepare_subset.sh data/tar/linux-5.10/raw \
  /dev/shm/linux-tar-subset 10 data/tar/linux-5.10/processed
scripts/run_all_experiments.sh \
  /dev/shm/linux-tar-subset/files.txt linux-5.10.1-5.10.10 \
  results/processed/linux-tar-metrics.csv 5
python3 scripts/collect_metrics.py results/processed/linux-tar-metrics.csv
```

例如，设置 `START_RUN=2` 后可从第 2 轮开始续跑：`START_RUN=2 scripts/run_all_experiments.sh ... 5`。下载的内核源码不等同于论文使用的完整 TAR 工作负载；报告结果时必须说明这是本地复现的子集。

## 数据集

`data/` 直接按论文的六类数据（WEB、TAR、RDB、SYN、VMA、VMB）建一级目录；Linux、GCC、GDB、Emacs 源码归档集中在 `data/tar/`，合成快照集中在 `data/syn/`。小型测试文件按内容归入对应类型目录，并用 `test-` 文件名前缀区分。数据布局和来源见 [`data/README.md`](data/README.md)，论文类别样本的校验清单见 `data/manifest-paper-categories.json`。

本地工作区中没有找到 Finesse 原始数据集。类别样本和带 `test-` 前缀的小型数据均不代表论文原始工作负载，不能用来声称复现论文表 3。原有约 34 GB 的 Kruus 数据仍保留在本地，对应的大型 TAR 与合成快照目录不纳入 Git 追踪。

## 当前实验结果

论文的 TAR 工作负载包含 258 个 Linux 内核版本，总计 112 GB。本地实验数据与论文的历史版本集合不同，不能视为同一工作负载。

当前最终版本采用 SF32 特征哈希和 DCE 原始数据回退统计口径，每种方法、每组数据运行 3 轮。下表为三轮均值：

| 数据集 | 方法 | 数据量 | DCR | DCE 节省率 | SF 速度（MB/s） | 总吞吐量（MB/s） |
|---|---|---:|---:|---:|---:|---:|
| Linux 5.10.1–5.10.3 | Finesse | 3.06 GB | 1.838955 | 98.9830% | 443.941 | 140.477 |
| Linux 5.10.1–5.10.3 | N-transform | 3.06 GB | 1.981956 | 98.8079% | 200.711 | 108.638 |
| Linux 2.6.0、2.6.9 | Finesse | 392 MB | 1.137248 | 97.5515% | 447.126 | 134.209 |
| Linux 2.6.0、2.6.9 | N-transform | 392 MB | 1.266287 | 95.8351% | 197.460 | 79.526 |

本地 Linux 5.10.1–5.10.3 样本中，N-transform 的 DCR 高于 Finesse；论文 TAR 结果则是 Finesse 的 DCR 略高于 N-transform。因此目前尚未复现论文中的 DCR 趋势。该本地样本只有三个相邻版本，而论文包含 258 个版本，不能将两者的指标当作同口径结果直接比较。Linux 2.6.0 和 2.6.9 两版本样本中，N-transform 的 DCR 也高于 Finesse。

本轮所有运行的 `delta_fallback_raw_bytes` 均为 0。新版 DCE 统计已将回退的原始字节纳入计算，但本次正式数据中没有触发回退，因此该口径未改变这两组数据的 DCE 数值。SF32 是当前明确记录的工程实现，尚未验证为与论文的 SF 哈希逐位一致。

最终数据保存在 [`results/processed/final-sf32-dce-v2.csv`](results/processed/final-sf32-dce-v2.csv)，汇总与差异说明见[最终结果报告](results/processed/final-report.md)。本地 `results/logs/` 中保留了对应的 12 份运行日志；日志目录已加入忽略规则，不纳入公开仓库。

Destor 是可选的外部 checkout，不纳入本仓库；如需在本地构建，先按 `integration/README.md` 获取源码，再使用其中的兼容性命令。此前本地 checkout 的帮助命令可以运行，C ABI 分块范围适配器已通过 C 调用方测试；由于 Destor 的有状态 Rabin 边界行为与独立分块器不同，Destor 的完整备份路径尚未接入。
