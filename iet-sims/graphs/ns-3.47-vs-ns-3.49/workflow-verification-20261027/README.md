# ns-3.47 versus ns-3.49 comparison graphs

| Metric | Graph |
| --- | --- |
| Mean aggregate receiver goodput (Mbps) | [Goodput](ns347-vs-ns349-goodput.png) |
| Mean RTT (ms) | [RTT](ns347-vs-ns349-rtt.png) |
| Mean normalized Power | [Power](ns347-vs-ns349-power.png) |
| Jain fairness of mean flow goodput | [Fairness](ns347-vs-ns349-fairness.png) |

Each graph's left panel compares the metric for all 50 paired cases. The diagonal represents equal metric values. Gray circles identify the 40 cases with byte-identical aggregate traces; orange diamonds identify the ten with differing traces. Points can overlap. The right panel labels the ten differing cases and their metric deltas: **ns-3.49 minus ns-3.47**. Blue bars indicate positive deltas and orange bars negative deltas; a positive delta is not always an improvement (lower RTT is preferable). All axes are linear and small changes retain numeric labels.

Goodput is aggregate receiver-delivered payload averaged over the measurement window. RTT and Power are arithmetic means of finite aggregate samples, matching the existing comparison CSV. Fairness is Jain's index across the five flows' time-averaged goodputs, not a time-average of interval Jain values. These are per-case summaries, not entire time-series overlays.

The ten differing cases are buffer/BBR at 483 packets; buffer/Cubic at 483 and 690 packets; all five algorithms at 1% packet loss; and stochastic/Cubic at moderate and high volatility. All static cases match. The simulation duration is 100 seconds, with five homogeneous flows, seed 12345/run 1, default Qt rules and original timing. No independent-replication confidence intervals are claimed.

[comparison-metrics.csv](comparison-metrics.csv) is a byte-identical copy of the original [50-case metrics](../../ns-3.49/default-original-timing/ns347-vs-ns349.csv). The fresh workflow runs matched their respective archived results exactly, so these graphs also describe the workflow-verification matrices. Original results and version-specific figures are unchanged.

To regenerate from the repository root after installing NumPy and Matplotlib:

```sh
python3 iet-sims/plot_ns_versions.py \
  --metrics iet-sims/graphs/ns-3.47-vs-ns-3.49/workflow-verification-20261027/comparison-metrics.csv \
  --output iet-sims/graphs/ns-3.47-vs-ns-3.49/workflow-verification-20261027
```

Four PNGs are saved at 300 dpi. `graph-index.json` records source/script checksums, plotting versions and PNG checksums. Rendering can vary across plotting environments.
