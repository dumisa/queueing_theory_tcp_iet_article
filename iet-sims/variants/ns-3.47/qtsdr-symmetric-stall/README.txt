Symmetric QtSDR stall-correction study, ns-3.47
================================================
Default rule: if computed window == current window and RTT <= target, increment.
Symmetric variant: if computed window == current window, increment for RTT <=
target and decrement for RTT > target. The existing minimum-window clamp remains.
Only the QtSDR object was replaced in a separately linked internet library.
The compiled default library and driver were never replaced. The manually edited
symmetric source was saved in the variant source folder and the default source
was restored before testing, so future default builds retain the original <= rule.

Both matrices: 100 seconds, five algorithms independently evaluated with five
homogeneous flows each, 100 Mbps, 20 ms bottleneck delay, 1381-packet buffer,
alpha=1.2, epsilon=1, seed 12345/run 1. Original timing: start/warmup=10 us;
samples 0.001 + k*0.1 seconds, plus final sample at 100 seconds.
Each run has 1001 aggregate and 5005 per-flow samples. Library loading was
explicitly verified using dyld diagnostics for both default and variant.
The symmetric library passed the standard TCP test suite.

QtSDR results (default -> symmetric):
Mean receiver goodput: 95.61017532 -> 95.62766716 Mbps.
Median mean-flow RTT: 48.8 -> 48.6 ms.
Mean mean-flow RTT: 48.8928 -> 48.6554 ms.
Median normalized Power: 0.7907808105 -> 0.7940350525.
Mean normalized Power: 0.7844449324 -> 0.7885226868.
Jain fairness of mean per-flow receiver goodput: 0.9620015668 -> 0.8378132136.
Maximum sampled aggregate in-flight, 1 <= t < 60 s: 534 -> 593 packets.
Samples above 500 packets in that window: 2 -> 6.
Default per-flow rates span 14.73-24.36 Mbps; symmetric rates span 7.92-28.75 Mbps.
Small delay/Power gains accompany substantially worse flow sharing in this run.
These matched single-run observations do not establish a general performance claim.
The default <= rule remains installed.

QtTOR, NewReno, Cubic and BBR have exactly matching aggregate, flows, summary
and totals outputs between the two library variants.
Results: trace-results/ns-3.47/qtsdr-symmetric-stall-study/{default,symmetric}/
Graphs: graphs/ns-3.47/qtsdr-symmetric-stall-study/{default,symmetric,comparison}/
Separate implementation and build manifest: variants/ns-3.47/qtsdr-symmetric-stall/
All source, executable and active internet-library hashes and copies are retained
in each result variant's _provenance. Default unchanged checks are retained in
result root/default-unchanged.json and the build manifest.

Rebuild isolated library: ../tools/bin/python3 build_ns347_symmetric_variant.py
Repeat matrix: ../tools/bin/python3 run_ns347_symmetric_study.py
Compare: /opt/anaconda3/bin/python compare_ns347_symmetric_study.py
These commands are run from iet-sims. The variant's run.sh launches only the
isolated driver/library combination; ordinary ns3-3.47 commands use the default.
