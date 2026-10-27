ns-3.49 simulations using the current default Qt files from ns-3.47
===================================================================
Copied files: tcp-qt-sdr.cc/.h, tcp-qt-tor.cc/.h, examples/tcp/tcp_qt_sims.cc.
The two model .cc files replace core-module.h with double.h, nstime.h and
simulator.h because ns-3.49 prohibits module aggregator includes in models.
After reversing that include substitution, all five files match ns-3.47 exactly.
No congestion-control algorithm logic was changed. QtSDR uses the default <=
excitation rule and has no symmetric or extra above-target decrement.

Defaults: start/warmup 10 microseconds, absolute sampling phase 0.001 seconds,
interval 0.1 seconds, final sample at 100 seconds. Five homogeneous flows per
case, 100 Mbps nominal bottleneck, 20 ms one-way bottleneck delay, seed 12345/run 1.
50 cases: static 5, buffer 15, loss 15, stochastic 15. Every case passed checks
for 1001 aggregate samples, 5005 per-flow samples, phase alignment, receiver
byte sums, positive delivery for all flows and final time 100 seconds.

Full build, standard TCP tests and current Qt regression tests passed.
The previous Qt-specific test source described an earlier modified controller;
it was backed up along with the replaced files. Current tests check the copied
models' RTT tracking, predictor corrections, excitation, loss factor and cloning.
The backup and sync manifest are under ns-3.49/qt-sync-backups/20261027-165547/.
ns-3.47 sources, library and datasets were not changed.

Results are separate in trace-results/ns-3.49/default-original-timing/.
All ten PNG figures were generated from this version-specific result root and
visually checked. graph-index.json records exact CSV sources and checksums.
Each source snapshot includes the ns-3 version, executable and internet library.

Cross-version comparison:
All five static cases match ns-3.47 exactly. Across the complete matrix, 40 of
50 aggregate CSV and totals files match exactly; ten non-static cases differ.
The changed cases are BBR buffer 483; Cubic buffers 483 and 690; all five
algorithms at loss probability 0.01; and Cubic moderate/high stochastic service.
All 15 stochastic capacity paths match their corresponding ns-3.47 paths and
all five algorithms share the same path within each regime in ns-3.49.
ns347-vs-ns349.csv records numeric differences for all 50 matched cases, including
large differences in some high-loss results; exact equality is not claimed there.
The specific internal simulator changes responsible were not isolated in this run.

Repeat all experiments and graphs from any working directory:
  ~/Development/ns3-install/iet-sims/sim_scripts/run_ns349_all.sh
The version-specific launcher selects separate result and graph roots. Extra
runner options are forwarded unchanged. Use --help for supported options.
The copied driver's direct --sim_name default remains inherited from ns-3.47;
use the version-specific launcher to maintain output separation automatically.

Metric summary: experiment-summary.csv
Cross-version summary: ns347-vs-ns349.csv
Distribution statistics: boxplot-statistics.csv
Validation and engine checks: validation.json and engine-check.json in result root.
Boxplots describe temporal samples from one fixed seed/run, not statistical
confidence intervals across independent replications. All outliers are retained;
Power plots with peaks above 1.5 use a labelled linear/log scale.
