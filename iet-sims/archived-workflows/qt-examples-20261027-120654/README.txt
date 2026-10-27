CURRENT LAUNCHERS (27 October 2026)
==================================
Use sim_scripts/README.txt for the current three-example workflow.
New traces: trace-results/qt-examples/; new PNGs: graphs/qt-examples/.
The following older guide describes the preserved tcp_qt_sims CSV pipeline.

IET TCP SIMULATIONS AND PNG GRAPHS

RESEARCHER SHELL ENTRY POINTS
  cd ~/Documents/Codex/ns3-install/iet-sims/sim_scripts
  ./build.sh
  ./run_all.sh --jobs=4

The sim_scripts folder also provides run_static.sh, run_buffer.sh, run_loss.sh,
run_stochastic.sh and plot_results.sh. Every launcher supports --help and
forwards runtime overrides to the existing Python scripts. See
sim_scripts/README.txt for experiment mapping, examples, environments and logs.

Locations
  Simulation driver:
    ~/Documents/Codex/ns3-install/ns-3.49/examples/tcp/tcp_qt_sims.cc
  Traced and computed results:
    ~/Documents/Codex/ns3-install/iet-sims/trace-results/
  PNG graphs:
    ~/Documents/Codex/ns3-install/iet-sims/graphs/

Individual simulations now default to trace-results regardless of the current
working directory. They retain separate scenario/algorithm/seed-run-config
subfolders. --sim_name can explicitly override the results root.

Run an individual experiment
  source ~/Documents/Codex/ns3-install/tools/bin/activate
  cd ~/Documents/Codex/ns3-install/ns-3.49
  ./ns3 run 'tcp_qt_sims --scenario=static --transport_prot=TcpQtSdr'

Optional high-resolution traces and packet captures
  ./ns3 run 'tcp_qt_sims --scenario=static --transport_prot=TcpQtSdr --tracing=1 --pcap=1'

The default data still contain all sampled RTT, congestion-window, in-flight,
receiver-goodput, Power, fairness and capacity measurements without these flags.
The flags add socket-events.csv and packet captures; they are not necessary for
the article's comparison graphs and can create much larger output files.

Generate or refresh graphs using existing results
  /opt/anaconda3/bin/python ~/Documents/Codex/ns3-install/iet-sims/plot_iet_sims.py

This uses the existing Anaconda Python installation, which has Matplotlib and
NumPy. No extra plotting packages were installed. The script also works with
other Python environments containing those two packages.

Run/resume all 50 article configurations, then generate graphs
  python3 ~/Documents/Codex/ns3-install/iet-sims/run_iet_sims.py --jobs=4 --plot

The runner uses independent simulator processes and fixed seed/run settings.
It reuses complete results only when the source fingerprint also matches.
Partial results or runs from a changed source revision are regenerated with an
explicit --overwrite option. Run it after completing any source edits/builds;
it records the current driver and Qt model sources, not a general binary audit.
Per-result-folder advisory locks serialize overlapping matrix invocations; completed
matching runs are then reused. Index files are written atomically per invocation.

For independent replications
  python3 ~/Documents/Codex/ns3-install/iet-sims/run_iet_sims.py --run=2 --jobs=4
  /opt/anaconda3/bin/python ~/Documents/Codex/ns3-install/iet-sims/plot_iet_sims.py --run=2 --graphs=~/Documents/Codex/ns3-install/iet-sims/graphs/run2

The scripts accept other result and graph roots through --results and --graphs.
Use --duration for a different observation window. The default is 100 seconds.
The plotter normally requires all configurations for complete comparisons.
--allow-partial skips incomplete figure groups; it never invents missing data.
When multiple equally complete settings/source families exist, choose the
reported --family identifier rather than pooling incompatible experiments.

PNG figures (300 dpi)
  fig03_static_inflight.png      Aggregate in-flight data, first 60 seconds; base BDP.
  fig04_static_rtt.png           Temporal distribution of mean flow RTT.
  fig05_static_power.png         Temporal normalized interval-averaged Power.
  fig06_static_goodput.png       Receiver-delivered time-average aggregate goodput.
  fig07_static_fairness.png      Jain fairness from time-average per-flow goodputs.
  fig08_buffer_power.png         Power for 483, 690 and 1381 packet buffers.
  fig09_loss_power.png           Power for PER 1e-6, 1e-4 and 1e-2.
  fig10_stochastic_power.png     Power for low, moderate and high OU volatility.
  fig11_stochastic_rtt.png       RTT for low, moderate and high OU volatility.
  supplement_ou_capacity.png     Actual capacity paths for the three OU regimes.

Graph provenance and statistics
  graphs/graph-index.json records exact source directories and aggregate CSV
  SHA-256 checksums, comparison family, source revision and figure metrics.
  graphs/boxplot-statistics.csv records observation count and distribution
  minimum, quartiles, median and maximum for each plotted box.
  trace-results/_provenance/ preserves the driver and four Qt model files,
  together with SHA-256 hashes.
  Each completed run has run-metadata.json with the command and source revision.
  trace-results/_run-logs/ contains individual simulation console logs.
  trace-results/experiment-index.json indexes the completed batch.

Plotting conventions
  Data come from measured CSVs. No smoothing or invented/filled observations.
  Non-finite early RTT/Power values are omitted from distributions.
  Boxes span the 25th-75th percentiles; median line; whiskers at 1.5 IQR;
  outliers are shown. Zero-goodput intervals are retained when otherwise valid.
  Power plots with peaks above 1.5 use a clearly labelled scale that is linear
  below 1 and logarithmic above 1; no outliers are removed or values clipped.
  Fairness bars use time-average delivered per-flow goodputs, not an average
  of per-interval fairness. Boxplots describe temporal variation from one run,
  not confidence intervals across independent replications.
  Capacity paths must match byte-for-byte across algorithms within each OU
  regime before the supplemental figure is generated.

These figures correspond to the article's experiment types and metric
formulas. They use the locally improved Qt controllers; they are not assertions
that the published numerical results or original controller binaries have been
reproduced. The step-delay extension remains available in tcp_qt_sims.cc but is
not part of the article's Figures 3-11 matrix.

UPDATE: launchers now use tcp_qt_static, tcp_qt_step_delay and tcp_qt_stochBW.
See sim_scripts/README.txt for current commands, output paths and metric caveats.
Earlier sections describe the historical tcp_qt_sims CSV pipeline.
