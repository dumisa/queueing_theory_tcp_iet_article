ns-3.47 buffer, loss and stochastic experiments
==============================================
45 independent 100-second simulations; five homogeneous TCP flows per case.
Algorithms: QtSDR, QtTOR, NewReno, Cubic and BBR.
QtSDR uses the installed default <= excitation rule. The symmetric and previous
modified variants were not used. The default source, driver and library hashes
were checked before and after the matrix, and default library loading verified.
ns-3.49 and earlier comparison datasets were not changed.

Original timing: sender start and warmup 0.00001 seconds; samples at absolute
0.001 + 0.1*k seconds, with a final 100-second sample for complete totals.
Seed=12345, run=1; OU stream=42. Four independent worker processes were used.
All cases have 1001 aggregate and 5005 per-flow samples, positive payload delivery
for every flow, and byte-consistent receiver summaries.

Sweeps (15 cases each):
- Buffer: 483, 690, 1381 packets at the DropTail bottleneck.
- Noncongestive forward packet losses: 1e-6, 1e-4, 1e-2.
- Stochastic capacity: low (tau=8,sigma=.15), moderate (2,.45), high (.5,1).
  Capacity clips at 25-175 Mbps; each regime has identical capacity.csv SHA-256
  across all five algorithms. Reverse capacity remains fixed by default.
Nominal bottleneck capacity=100 Mbps; bottleneck one-way delay=20 ms.
Full settings are recorded in each resolved-config.txt.

PNGs:
  fig08_buffer_power.png
  fig09_loss_power.png
  fig10_stochastic_power.png
  fig11_stochastic_rtt.png
  supplement_ou_capacity.png
Power outliers are retained. Graphs with peaks above 1.5 use a labelled scale
that is linear below 1 and logarithmic above 1. Boxplots show temporal samples
from one run, not confidence intervals across independent replications.

experiment-summary.csv reports per-case goodput, fairness, RTT and Power.
boxplot-statistics.csv contains the plotted distribution statistics.
graph-index.json maps all graphs to exact source CSV paths and their hashes.
Result validation, library checks and source/library snapshots are in the
version-specific trace-results directory beside its experiment-index.json.

Repeat from ~/Development/ns3-install/iet-sims:
  ../tools/bin/python3 run_ns347_nonstatic.py
  ../tools/bin/python3 validate_ns347_nonstatic.py
  /opt/anaconda3/bin/python plot_iet_sims.py --results trace-results/ns-3.47/buffer-loss-stochastic-default-original-timing --graphs graphs/ns-3.47/buffer-loss-stochastic-default-original-timing --allow-partial
The partial plotting option skips the unrequested static group; all three
requested experiment groups are complete and fully plotted.
