# Complete launcher verification: 27 October 2026

Both launchers completed the full release build, selected TCP tests, all 50 simulations and ten PNG graphs from the renamed local repository folder. ns-3.49 also passed its Qt test suite.

| Version | Simulation results and per-version validation | PNG graphs |
| --- | --- | --- |
| ns-3.47 | [Results](trace-results/ns-3.47/workflow-verification-20261027/) | [Graphs](graphs/ns-3.47/workflow-verification-20261027/) |
| ns-3.49 | [Results](trace-results/ns-3.49/workflow-verification-20261027/) | [Graphs](graphs/ns-3.49/workflow-verification-20261027/) |

Each version has five static, 15 buffer, 15 loss and 15 stochastic cases. All cases were freshly simulated. Each algorithm was run independently with five flows using that algorithm. Default timing, seed 12345/run 1 and the default QtSDR rule were retained.

Validation checked configuration and timing, 1001 aggregate samples, 5005 per-flow samples, five flow summaries per case, receiver-byte accounting, matching stochastic capacity paths across algorithms, source/engine hashes and ten PNG graph files per version. All 50 aggregate CSVs in each version matched that version's archived reference results byte for byte. This comparison is within each simulator version, not a claim that all ns-3.47 outputs equal ns-3.49 outputs.

- [Combined validation report](workflow-verification-20261027.json).
- [ns-3.47 validation](trace-results/ns-3.47/workflow-verification-20261027/workflow-validation.json) and [successful full launcher log](trace-results/ns-3.47/workflow-verification-20261027/_workflow-logs/complete-launcher.log).
- [ns-3.49 validation](trace-results/ns-3.49/workflow-verification-20261027/workflow-validation.json) and [successful full launcher log](trace-results/ns-3.49/workflow-verification-20261027/_workflow-logs/complete-launcher.log).
- [Upload checksums](workflow-verification-20261027-manifest.json) list each uploaded result/graph/report/log, its size and SHA-256.

The initial ns-3.47 attempt exposed whitespace truncation in the output path. Both drivers were corrected to preserve spaces, then the successful workflows completed. The ns-3.47 provenance folder retains the first attempt's source snapshot as well as the successful run's snapshot; the active source revision is recorded in `experiment-index.json`. No failed simulation outputs are treated as completed runs.

Compiled provenance binaries and temporary locks are excluded; their hash records remain. Historical absolute paths in reports and indexes identify the original execution location. Locate uploaded files using the corresponding version and workflow folders above. Original reference data and earlier controller studies are unchanged.
