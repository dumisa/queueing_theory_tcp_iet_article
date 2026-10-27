ns-3.47 static simulations: restored original <= rule and default original timing
===============================================================================
Permanent defaults in ns-3.47/examples/tcp/tcp_qt_sims.cc:
  appStart = 0.00001 s (10 microseconds)
  warmup = 0.00001 s (measurement begins with senders; no payload discarded)
  samplingPhaseSeconds = 0.001 s
  data_logging_interval = 0.1 s
Samples follow the absolute phase 0.001 + k*0.1 s. A final sample at 100 s
retains complete receiver totals. A custom warmup uses the next absolute-phase
sample, rather than shifting the phase. The --samplingPhase runtime option
allows an alternative phase without rebuilding.
The default QtSDR rule remains <= excitation, with no additional above-target
window decrement. No controller or ns-3.49 code was changed by this timing update.

Five independent homogeneous algorithms, five flows per run, 100 seconds,
100 Mbps bottleneck, 20 ms one-way bottleneck delay, 1381 packets, seed 12345/run 1.
All five runs completed with 1001 aggregate and 5005 per-flow samples each.
Every sample phase was verified. All four output data files match the respective
original-timing baseline from the prior comparison exactly for all five algorithms.
The five PNG graphs use only this new version-specific result folder.

From iet-sims, repeat with:
  ../tools/bin/python3 run_ns347_static.py --results trace-results/ns-3.47/default-original-timing
  /opt/anaconda3/bin/python plot_iet_sims.py --results trace-results/ns-3.47/default-original-timing --graphs graphs/ns-3.47/default-original-timing --allow-partial
The plot option skips unrequested buffer/loss/stochastic groups while requiring
the complete five-algorithm static group.
Results remain separate from earlier runs and both rule-comparison studies.
