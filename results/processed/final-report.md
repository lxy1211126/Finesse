# Final Experiment Results

Experiment version: SF32 feature hash with DCE raw-fallback accounting, three runs per method and dataset.

| Dataset | Method | Input | DCR | DCE savings | SF speed (MB/s) | Throughput (MB/s) |
|---|---|---:|---:|---:|---:|---:|
| Linux 5.10.1-5.10.3 | Finesse | 3.06 GB | 1.838955 | 98.9830% | 443.941 | 140.477 |
| Linux 5.10.1-5.10.3 | N-transform | 3.06 GB | 1.981956 | 98.8079% | 200.711 | 108.638 |
| Linux 2.6.0, 2.6.9 | Finesse | 392 MB | 1.137248 | 97.5515% | 447.126 | 134.209 |
| Linux 2.6.0, 2.6.9 | N-transform | 392 MB | 1.266287 | 95.8351% | 197.460 | 79.526 |

Values are means across three runs. DCR and DCE counts were identical across repetitions. `delta_fallback_raw_bytes` was zero in all runs.

## Comparison and Limitations

For the paper's TAR workload, the reported DCR is 15.34 for Finesse and 15.00 for N-transform, a 2.27% advantage for Finesse. On the local Linux 5.10.1-5.10.3 sample, N-transform instead has a 7.78% higher DCR than Finesse. This is a trend mismatch, not a successful reproduction of the paper's result.

The paper evaluates 258 Linux kernel versions (112 GB TAR workload); this run uses only three adjacent kernel versions totaling about 3.06 GB. The local sample is too small and not equivalent to the paper workload, so the DCR values are not a like-for-like reproduction. The separate Linux 2.6 sample contains two versions and also shows N-transform with a higher DCR.

The updated DCE accounting path is covered by a fallback test, but no raw fallback bytes occurred in these workloads. SF32 is the current implementation and has not been verified as an exact match for the paper's feature hash. Report these as local engineering results, not paper-matched reproduction results.
