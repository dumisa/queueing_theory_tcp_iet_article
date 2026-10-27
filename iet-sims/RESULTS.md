# Archived simulation results and graphs

The existing local results and PNG figures are included without rerunning experiments. Simulator versions remain separate:

| Version | Results | Graphs |
| --- | --- | --- |
| ns-3.47 | [trace-results/ns-3.47](trace-results/ns-3.47/) | [graphs/ns-3.47](graphs/ns-3.47/) |
| ns-3.49 | [trace-results/ns-3.49](trace-results/ns-3.49/) | [graphs/ns-3.49](graphs/ns-3.49/) |

## Default experiments

For ns-3.47, `default-original-timing/` contains the five static runs and `buffer-loss-stochastic-default-original-timing/` contains the other 45 runs. For ns-3.49, `default-original-timing/` contains all 50 runs. Corresponding graph folders use the same names.

These experiments use the default QtSDR `<=` rule, a 10 microsecond application start, and the original sampling phase. Each case runs one algorithm independently with five flows using that algorithm.

## Earlier runs and controller comparisons

The ns-3.47 folders also retain the earlier `static/` runs, `qt-sdr-window-rule-study/`, `qt-sdr-window-rule-original-timing/`, and `qtsdr-symmetric-stall-study/`. These are distinct experiments; use the default folders above for the current defaults. Their graphs and comparison metrics retain their original study and variant subfolders.

CSV traces, computed metrics, resolved configurations, run logs, validation records, source provenance and PNG graphs are retained. Temporary locks and compiled provenance binaries are omitted; their original hash records remain for reference. Historical provenance files and graph indexes retain original absolute paths from the execution machine. Use the corresponding version/study folders in this repository to locate those files.

`results-manifest.json` lists every imported data/graph file, its byte count and SHA-256 checksum, plus excluded local files. The root `source-manifest.json` records the original source-only import and does not cover this later results commit. Regenerating graphs requires Python with NumPy and Matplotlib; see the root README and plotting script help.

## Complete launcher verification

The new `workflow-verification-20261027/` folders contain 50 fresh runs and ten graphs for each simulator version, separate from the original reference folders. See [WORKFLOW-VERIFICATION.md](WORKFLOW-VERIFICATION.md) for links, validation reports and execution logs.
