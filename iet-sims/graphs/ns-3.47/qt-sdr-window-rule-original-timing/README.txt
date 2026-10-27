QtSDR original-versus-proposed rule: original timing, ns-3.47
==========================================================
New results: trace-results/ns-3.47/qt-sdr-window-rule-original-timing/
New graphs: graphs/ns-3.47/qt-sdr-window-rule-original-timing/
The preceding zero-start study and its outputs remain separate and unchanged.

Both implementations use sender start 0.00001 seconds and absolute sample times
0.001 + 0.1*k seconds. A final sample at 100 s retains complete receiver totals.
Warmup=0.00001 s satisfies the production driver's start/warmup requirement;
no payload has arrived at that time. Measurement duration is 99.99999 s.
Each run has 1001 aggregate samples and 5005 per-flow samples. RTT/Power omit
initial samples without valid RTT, as in the existing plotter.
Other matched settings: 5 homogeneous flows, 100 Mbps, 20 ms bottleneck delay,
1381-packet buffer, alpha=1.2, epsilon=1, seed 12345/run 1.
The sender stop remains at 100 s for both variants. Only the requested original
start and sampling phase were restored; other driver settings are unchanged.

Baseline-recorded and baseline-equality produced identical QtSDR aggregate data.
The baseline agrees exactly with all 600 supplied-original tcp_qt_static samples
below 60 s, as verified against the earlier diagnostic trace.

QtSDR metrics (original -> proposed):
Mean receiver goodput: 95.61017532 -> 95.39795642 Mbps.
Median mean-flow RTT: 48.8 -> 48.6 ms.
Mean mean-flow RTT: 48.8928 -> 48.9680 ms.
Median normalized Power: 0.7907808105 -> 0.7917312682.
Mean normalized Power: 0.7844449324 -> 0.7813795774.
Jain fairness of per-flow mean goodput: 0.9620015668 -> 0.9258838606.
Maximum in-flight during 1 <= t < 60 s: 534 -> 564 packets.
Samples above 500 packets in that interval: 2 -> 7.
The median delay/Power changes are small, while fairness and large excursions
worsen. The earlier improvement at zero start time is not robust to the original
start/sampling phase. These are deterministic single-run observations, not
multi-seed evidence of a general performance effect.

QtTOR, NewReno, Cubic and BBR have exact matching aggregate, per-flow, summary
and totals files between the baseline and proposed builds.
All 11 simulations and their builds passed. The production driver was restored
and rebuilt, leaving the proposed QtSDR controller installed. Original timing is
contained in the saved study driver and provenance; production timing defaults
were not permanently changed.

Reproduce: ../tools/bin/python3 run_qtsdr_original_timing_study.py
Compare: /opt/anaconda3/bin/python compare_qtsdr_original_timing.py
Source/executable/internet-library hashes and copies are in each variant's
_provenance folder. Timing-study driver copy: timing-driver.cc in result root.
Direct graphs, metrics, per-flow rates and timing sensitivity summary are in
comparison/. Five-algorithm static graphs are in baseline-recorded/ and proposed/.
