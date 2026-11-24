# 数据目录

所有数据集均按工作负载类型放在一级目录中，不再设置 `mini/` 这类中间数据集目录：

| 类型 | 目录 | 包含内容 |
|---|---|---|
| WEB | `web/` | 论文类别合成代理样本、网页类测试样本 |
| TAR | `tar/` | 论文 Linux 源码样本、源码类测试样本、Linux 2.6/5.10、GCC、GDB、Emacs 归档 |
| RDB | `rdb/` | 论文类别键值代理样本、键值类测试样本 |
| SYN | `syn/` | 论文类别合成代理样本、冒烟/边界样本、Kruus 合成快照 |
| VMA | `vma/` | 论文类别 VM 代理样本、VM 类测试样本 |
| VMB | `vmb/` | 论文类别 VM 代理样本 |

小型测试样本以 `test-` 开头，直接放在对应类型目录中。`tar/linux-5.10/` 是同一 TAR 类型下的版本数据组织：`raw/` 放压缩包，`processed/` 放解压 tar。约 34 GB 的 Kruus 版本数据分别位于 `tar/kruus/` 和 `syn/kruus/`。这些大文件被 `.gitignore` 排除，不会提交到 Git。

`manifest-paper-categories.json` 记录六类论文代理样本的路径、大小、SHA-256 和来源，可运行以下命令验证：

```bash
node scripts/verify_github_samples.mjs data manifest-paper-categories.json
```

`manifest-test-fixtures.json` 及 `files_main.txt`、`files_smoke.txt`、`files_edges.txt` 描述小型测试数据，运行以下命令验证：

```bash
node scripts/verify_test_fixtures.mjs data
```

这些样本不是论文原始数据集。除 `tar/` 中有来源标记的 Linux 源码文件和本地公开源码归档外，其余均为合成代理或测试数据，不可用来声称完整复现论文工作负载。

要在独立空目录中重建小型样本（不会覆盖 `data/`），可执行：

```bash
node scripts/generate_github_samples.mjs --out /tmp/finesse-paper-samples
python3 scripts/add_linux_tar_sample.py --output /tmp/finesse-paper-samples
node scripts/verify_github_samples.mjs /tmp/finesse-paper-samples
node scripts/generate_test_fixtures.mjs --out /tmp/finesse-test-fixtures
node scripts/verify_test_fixtures.mjs /tmp/finesse-test-fixtures
```

Linux 源码提取脚本默认从 `data/tar/kruus/public/linux/raw/` 读取已有归档；没有该目录时须通过 `--source-dir` 指定本地源码归档。生成器要求输出目录为空。
