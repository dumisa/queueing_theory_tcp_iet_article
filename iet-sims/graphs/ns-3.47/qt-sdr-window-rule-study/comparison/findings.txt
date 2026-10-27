QtSDR proposed window rule: ns-3.47 static comparison
====================================================
The proposed rule is installed in ns-3.47/src/internet/model/tcp-qt-sdr.cc.
Its redundant nested condition was simplified without changing the requested logic.
The user's previously edited source is saved in ns-3.47/qt-window-rule-backup/.
ns-3.49 and its source code were not changed.

Results are separate under trace-results/ns-3.47/qt-sdr-window-rule-study/:
  baseline-recorded: all five algorithms; saved baseline with inner <=.
  baseline-equality: QtSDR only; literal earlier snippet with inner ==.
  proposed: all five algorithms; inner <= and above-target increment restraint.
The two baselines have identical complete QtSDR aggregate.csv files in this test.

Matched settings: 100 seconds, 5 homogeneous flows, 100 Mbps, 20 ms one-way
bottleneck delay, 1381-packet DropTail buffer, alpha=1.2, epsilon=1,
seed=12345/run=1, sample interval=0.1 s, appStart=0, warmup=0.
The original-driver 10-microsecond timing and 0.001-second sampling phase were
NOT introduced here, so this experiment isolates the controller rule change.

QtSDR findings (baseline -> proposed):
Mean receiver goodput: 95.69102208 -> 95.86084352 Mbps (+0.1775%).
Mean sampled RTT: 51.2014 -> 49.8206 ms (-2.697%).
Median sampled RTT: 49 -> 49 ms.
Median normalized Power: 0.7843516982 -> 0.7875531337 (+0.4082%).
Jain index from per-flow mean goodput: 0.9603104810 -> 0.9880155870.
Maximum sampled in-flight, 1 <= t < 60 s: 688 -> 615 packets.
Samples above 500 packets in that window: 66 -> 20.
The broad delay/in-flight excursion is reduced, with some shorter spikes remaining.
These are matched single-run observations, not multi-seed statistical conclusions.

The aggregate, per-flow, summary and total CSV/text outputs for QtTOR, NewReno,
Cubic and BBR match exactly between baseline-recorded and proposed.

Validation: full build and TCP test suite passed. All 11 runs completed with
1000 aggregate and 5000 per-flow samples per run. Each variant saves source,
executable, and internet-library hashes plus copies in its provenance folder.

Graphs:
  baseline-recorded/ and proposed/: five static comparisons per variant.
  comparison/: direct QtSDR in-flight/RTT, distribution, and goodput/fairness PNGs.
  comparison/qtsdr-metrics.csv: numeric comparison.
  comparison/comparison-index.json: settings, source paths, hashes and changes.

Reproduce from iet-sims with run_qtsdr_window_rule_study.py, which stages,
builds and runs each version and leaves the proposed version installed.
Use compare_qtsdr_window_rules.py to regenerate the direct comparison PNGs.
