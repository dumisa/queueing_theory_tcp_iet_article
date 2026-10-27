Static experiments using ns-3.47
===============================
Five algorithms: TcpQtSdr, TcpQtTor, TcpNewReno, TcpCubic, TcpBbr.
Five flows per case, 100 seconds, 100 Mbps, seed 12345, run 1.
Sampling interval: 0.1 second. See resolved-config.txt in each run for all values.
Trace root: ../../trace-results/ns-3.47/
Figures 3-7 use measured receiver goodput, sampled RTT, computed normalized
Power, and Jain fairness from time-averaged per-flow receiver goodput.
Boxplots show temporal samples from a single run, not independent replications.
This is the local Qt model implementation, not a claim of published-value reproduction.
The engine version and compiled executable SHA-256 are recorded in the run
provenance; graph-index.json identifies the CSV sources and their checksums.

Repeat from ~/Development/ns3-install/iet-sims:
  ../tools/bin/python3 run_ns347_static.py
  /opt/anaconda3/bin/python plot_iet_sims.py --results trace-results/ns-3.47 --graphs graphs/ns-3.47 --allow-partial
The partial option skips buffer, loss and stochastic figures because only
static experiments were requested. All five static cases are required.
