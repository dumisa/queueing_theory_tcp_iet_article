# IET TCP experiments: ns-3.47 and ns-3.49

This repository contains two complete ns-3 source trees with QtSDR, QtTOR and the `tcp_qt_sims.cc` experiment driver, plus simulation results, metrics and PNG graphs. Build and run each simulator version separately. The instructions below target macOS; the version-specific provenance runners currently refer to macOS `.dylib` libraries.

## Workflow-verification results

The complete launcher verification on 27 October 2026 passed for both versions: **50 fresh simulation cases and ten PNG graphs per version**. Each version's aggregate CSVs matched its own archived reference results byte for byte.

| Simulator | Results and metrics | PNG graphs | Validation report | Build, test and run log |
| --- | --- | --- | --- | --- |
| ns-3.47 | [Results](iet-sims/trace-results/ns-3.47/workflow-verification-20261027/) | [Graphs](iet-sims/graphs/ns-3.47/workflow-verification-20261027/) | [Validation JSON](iet-sims/trace-results/ns-3.47/workflow-verification-20261027/workflow-validation.json) | [Launcher log](iet-sims/trace-results/ns-3.47/workflow-verification-20261027/_workflow-logs/complete-launcher.log) |
| ns-3.49 | [Results](iet-sims/trace-results/ns-3.49/workflow-verification-20261027/) | [Graphs](iet-sims/graphs/ns-3.49/workflow-verification-20261027/) | [Validation JSON](iet-sims/trace-results/ns-3.49/workflow-verification-20261027/workflow-validation.json) | [Launcher log](iet-sims/trace-results/ns-3.49/workflow-verification-20261027/_workflow-logs/complete-launcher.log) |

See the [combined validation report](iet-sims/workflow-verification-20261027.json), [upload checksums](iet-sims/workflow-verification-20261027-manifest.json) and [verification guide](iet-sims/WORKFLOW-VERIFICATION.md). These verification folders are separate from the original reference experiments and earlier controller studies.

## ns-3.47 versus ns-3.49

The default 50-case matrices use the same Qt rules, parameters, original timing and random seed/run. “Match” below means byte-identical `aggregate.csv` results; final `totals.txt` files agree in the same 40 cases.

| Scenario | Matching cases | Differences |
| --- | --- | --- |
| Static | **5/5** | All five algorithms match. |
| Buffer | **12/15** | BBR differs at 483 packets; Cubic differs at 483 and 690 packets. All QtSDR, QtTOR and NewReno cases match. |
| Loss | **10/15** | All algorithms match at error probabilities 1e-6 and 1e-4; all five differ at 1e-2. |
| Stochastic | **13/15** | Cubic differs under moderate and high volatility; the other cases match. All 15 capacity traces match across versions. |
| **Total** | **40/50** | Differences are confined to ten nonstatic cases. |

See the [50-case comparison metrics](iet-sims/graphs/ns-3.49/default-original-timing/ns347-vs-ns349.csv) for goodput, RTT, normalized Power, Jain fairness and exact-match flags. Delta columns mean **ns-3.49 minus ns-3.47**. The largest goodput difference is QtTOR at 1% packet loss: **+31.650 Mbps** in ns-3.49, with mean normalized Power **+0.30061**. QtSDR matches in 9/10 cases; at 1% loss its goodput changes by **+0.882 Mbps**, mean RTT by **+0.457 ms**, and mean Power by **−0.00689**.

The fresh workflow-verification runs reproduced all 50 archived aggregate CSVs within each version ([combined validation](iet-sims/workflow-verification-20261027.json)), so this comparison also applies to the new runs. These are seed-12345/run-1 observations, not estimates across independent replications. The cause of the cross-version differences has not been isolated.

## 1. Get the repository and prepare macOS

Install Xcode command-line tools if they are not already installed:

```sh
xcode-select --install
```

With Homebrew installed, install the build and plotting dependencies, then clone the repository:

```sh
brew install git cmake ninja eigen python
git clone https://github.com/dumisa/queueing_theory_tcp_iet_article.git
cd queueing_theory_tcp_iet_article
python3 -m venv tools
tools/bin/python3 -m pip install numpy matplotlib
```

Use CMake 3.25 or newer and a compiler supporting C++23 for both source trees. The original builds used Intel macOS and Apple Clang 17. Eigen3 is needed by the additional ns-3.49 contrib modules. Python environments and compiled simulator files are not included in Git.

Run the following in each new terminal, from the cloned repository root. Keep this terminal open for the remaining commands:

```sh
export IET_ROOT="$PWD"
export PATH="$IET_ROOT/tools/bin:$PATH"
export IET_PYTHON="$IET_ROOT/tools/bin/python3"
export IET_PLOT_PYTHON="$IET_PYTHON"
# The local math helper includes <eigen3/Eigen/Eigen>.
export CPLUS_INCLUDE_PATH="$(brew --prefix eigen)/include${CPLUS_INCLUDE_PATH:+:$CPLUS_INCLUDE_PATH}"
cmake --version
"$IET_PYTHON" -c 'import matplotlib, numpy; print(matplotlib.__version__, numpy.__version__)'
```

All paths below are based on `IET_ROOT`. Do not point one version's runner at the other version's results.

## One-command reproduction for each version

After completing dependency setup in Section 1, use one launcher per version:

```sh
bash "$IET_ROOT/reproduce_ns347.sh"
bash "$IET_ROOT/reproduce_ns349.sh"
```

Each launcher configures and builds its own release installation with examples/tests enabled, runs the relevant TCP tests, runs all 50 cases (static, buffer, loss and stochastic), then creates ten PNG graphs. It stops on failure and records a pipeline log. Run either launcher independently, from any working directory. Neither launcher builds or executes the other simulator version.

| Launcher | CSV results and metrics | PNG graphs |
| --- | --- | --- |
| `reproduce_ns347.sh` | `iet-sims/trace-results/ns-3.47/reproduction-all/` | `iet-sims/graphs/ns-3.47/reproduction-all/` |
| `reproduce_ns349.sh` | `iet-sims/trace-results/ns-3.49/reproduction-all/` | `iet-sims/graphs/ns-3.49/reproduction-all/` |

These full-matrix output folders differ from the archived reference folders and the separate ns-3.47 static/nonstatic examples below. Defaults remain 100 seconds, five homogeneous flows, seed 12345/run 1, original timing and the default QtSDR rule. The ns-3.47 launcher uses the existing version-specific provenance runner with all four scenarios explicitly selected.

```sh
# Use fewer build/simulation workers and a distinct output folder name.
bash "$IET_ROOT/reproduce_ns347.sh" --build-jobs 2 --jobs 2 --run-id replication-01
bash "$IET_ROOT/reproduce_ns349.sh" --build-jobs 2 --jobs 2 --run-id replication-01

# Inspect commands without building, simulating, or creating output folders.
bash "$IET_ROOT/reproduce_ns347.sh" --dry-run
bash "$IET_ROOT/reproduce_ns349.sh" --help
```

`--run-id` applies beneath each version's own results/graphs directory. Reusing a run ID permits source-matched completed cases to be reused; changed or incomplete cases can be regenerated. Use a new ID for an independent output set. The two archived default folder names are rejected. Pipeline logs are under `iet-sims/trace-results/_launch-logs/ns-3.47/` or `ns-3.49/`.

The launchers use `tools/bin/python3` when the setup environment exists, or Python from `PATH`; override with `IET_PYTHON` and `IET_PLOT_PYTHON`. They select only their own build library and discover Homebrew Eigen's include directory. Build prerequisites are installed in Section 1; the launchers do not install dependencies. The manual commands below remain available for running individual stages.

## 2. Build each simulator

### ns-3.47

```sh
(
  cd "$IET_ROOT/ns-3.47"
  export CCACHE_DIR="$PWD/.ccache"
  "$IET_PYTHON" ./ns3 configure --enable-examples --enable-tests --build-profile=release
  "$IET_PYTHON" ./ns3 build -j 4
  "$IET_PYTHON" ./test.py -s tcp
)
```

### ns-3.49

```sh
(
  cd "$IET_ROOT/ns-3.49"
  export CCACHE_DIR="$PWD/.ccache"
  "$IET_PYTHON" ./ns3 configure --enable-examples --enable-tests --build-profile=release
  "$IET_PYTHON" ./ns3 build -j 4
  "$IET_PYTHON" ./test.py -s tcp
  "$IET_PYTHON" ./test.py -s tcp-qt
)
```

Both builds include the Qt models and `examples/tcp/tcp_qt_sims.cc` through their CMake registrations. The older standalone Qt example files are retained as references; this workflow uses `tcp_qt_sims.cc`.

Alternatively, `bash "$IET_ROOT/build_installations.sh"` builds both source trees in sequence. It does not run the test commands above. Reduce `-j 4`, or set `IET_BUILD_JOBS=2` for the helper script, if memory is limited.

## 3. Experiment design and default parameters

Each case is a separate simulator process running **five homogeneous flows**: all five use the same congestion-control algorithm. The algorithms are QtSDR, QtTOR, NewReno, Cubic and BBR. Algorithms do not compete with one another within a case.

| Parameter | Default |
| --- | --- |
| Simulation duration | 100 seconds |
| Random seed / run | 12345 / 1 |
| Application start / measurement start | 0.00001 seconds (10 microseconds) |
| Sampling | 0.001 + k × 0.1 seconds, plus final sample at 100 seconds |
| Bottleneck | 100 Mbps, 20 ms one-way propagation delay |
| Access links | 10 Gbps, 0.01 ms one-way propagation delay |
| Reference bottleneck queue | 1381 packets, DropTail device queue |
| MSS / MTU | 1448 / 1500 bytes |
| QtSDR default stall correction | Increase by one when the rounded window is unchanged and RTT <= target |

Each version has the same 50-case matrix:

| Scenario | Conditions per algorithm | Cases |
| --- | --- | --- |
| Static | Reference setting | 5 |
| Buffer | 483, 690, 1381 packets | 15 |
| Loss | Forward packet-error probabilities 1e-6, 1e-4, 1e-2 | 15 |
| Stochastic capacity | Low, moderate, high OU volatility | 15 |

Qt model headers and the simulation driver match between versions. The ns-3.49 model implementation has the include changes required by that version. Alternative controller rules remain in separate study folders and are not the defaults.

## 4. Run new simulations with separate output folders

These commands use `reproduction-*` folders, leaving the committed reference experiments intact. The runners default to 100 seconds, seed 12345 and run 1. They record the sources and compiled engine used by each run. Compile first; runners do not build the simulator.

### ns-3.47: static cases

Run the five static cases, then generate their five graphs:

```sh
"$IET_PYTHON" "$IET_ROOT/iet-sims/run_ns347_static.py" \
  --results "$IET_ROOT/iet-sims/trace-results/ns-3.47/reproduction-static" \
  --jobs 4

"$IET_PLOT_PYTHON" "$IET_ROOT/iet-sims/plot_iet_sims.py" \
  --results "$IET_ROOT/iet-sims/trace-results/ns-3.47/reproduction-static" \
  --graphs "$IET_ROOT/iet-sims/graphs/ns-3.47/reproduction-static" \
  --duration 100 --seed 12345 --run 1 --allow-partial
```

### ns-3.47: buffer, loss and stochastic cases

Run the remaining 45 cases, then generate five graphs:

```sh
"$IET_PYTHON" "$IET_ROOT/iet-sims/run_ns347_nonstatic.py" \
  --results "$IET_ROOT/iet-sims/trace-results/ns-3.47/reproduction-nonstatic" \
  --jobs 4

"$IET_PLOT_PYTHON" "$IET_ROOT/iet-sims/plot_iet_sims.py" \
  --results "$IET_ROOT/iet-sims/trace-results/ns-3.47/reproduction-nonstatic" \
  --graphs "$IET_ROOT/iet-sims/graphs/ns-3.47/reproduction-nonstatic" \
  --duration 100 --seed 12345 --run 1 --allow-partial
```

`--allow-partial` skips scenario groups absent from that result root; it does not fill in missing data. Do not add `--plot` to these ns-3.47 runner commands: their automatic plotting call expects the complete four-scenario matrix. Use the explicit plotting commands above for the two separate groups.

### ns-3.49: all cases

Run all 50 cases and generate all ten graphs:

```sh
bash "$IET_ROOT/iet-sims/sim_scripts/run_ns349_all.sh" \
  --results "$IET_ROOT/iet-sims/trace-results/ns-3.49/reproduction-all" \
  --graphs "$IET_ROOT/iet-sims/graphs/ns-3.49/reproduction-all" \
  --jobs 4
```

The launcher uses `IET_PYTHON` and `IET_PLOT_PYTHON`. It calls the ns-3.49 runner, supplies `--plot`, and records a launcher log. The older generic `run_all.sh` and `run_static.sh` launchers use shared output defaults; use the version-specific instructions above.

To select a subset, add `--algorithms TcpQtSdr` or `--scenarios static` where appropriate. Matched comparison graphs require all five algorithms in each selected scenario group. For a quick simulation check, use `--duration 2` with a new result folder and omit plotting; a short run is not a reproduction of the reported 100-second experiment.

The runners reuse completed cases only when the configuration and source fingerprint match. Otherwise they can overwrite cases within the selected output root. Use a new folder name for every intentional parameter/controller change. Do not mix controller variants or simulator versions in one root.

## 5. Regenerate graphs from the committed results, without simulation

Building ns-3 is unnecessary for this step; only the Python plotting dependencies are needed. These commands read the committed CSV data and write regenerated PNGs to new folders:

```sh
# ns-3.47: reference static data -> five PNGs
"$IET_PLOT_PYTHON" "$IET_ROOT/iet-sims/plot_iet_sims.py" \
  --results "$IET_ROOT/iet-sims/trace-results/ns-3.47/default-original-timing" \
  --graphs "$IET_ROOT/iet-sims/graphs/ns-3.47/regenerated-static" \
  --duration 100 --seed 12345 --run 1 --allow-partial

# ns-3.47: reference buffer/loss/stochastic data -> five PNGs
"$IET_PLOT_PYTHON" "$IET_ROOT/iet-sims/plot_iet_sims.py" \
  --results "$IET_ROOT/iet-sims/trace-results/ns-3.47/buffer-loss-stochastic-default-original-timing" \
  --graphs "$IET_ROOT/iet-sims/graphs/ns-3.47/regenerated-nonstatic" \
  --duration 100 --seed 12345 --run 1 --allow-partial

# ns-3.49: reference full matrix -> ten PNGs
"$IET_PLOT_PYTHON" "$IET_ROOT/iet-sims/plot_iet_sims.py" \
  --results "$IET_ROOT/iet-sims/trace-results/ns-3.49/default-original-timing" \
  --graphs "$IET_ROOT/iet-sims/graphs/ns-3.49/regenerated-all" \
  --duration 100 --seed 12345 --run 1
```

Expected outputs are Figures 03–07 for static experiments, Figures 08–11 for buffer/loss/stochastic experiments, and `supplement_ou_capacity.png`. Graphs default to 300 dpi. Each graph directory also receives `graph-index.json` and `boxplot-statistics.csv`. Existing summary and cross-version comparison CSVs are separate archived analysis outputs; the plotting command does not regenerate them.

The plotter checks receiver-byte accounting and completed runs, compares only matched parameter/source families, and rejects duplicate comparison cells. If several equally complete families exist, select the intended `--family` reported by the error, or use a clean result root. Boxplots describe temporal samples from a run, not independent random replications. Plotting uses measured data without smoothing or clipping; Power can exceed one transiently. PNG bytes may vary with Matplotlib versions and fonts even when the underlying data agree.

## 6. Locate and interpret outputs

| Item | Location or meaning |
| --- | --- |
| ns-3.47 archived default static results / graphs | `iet-sims/{trace-results,graphs}/ns-3.47/default-original-timing/` |
| ns-3.47 archived default nonstatic results / graphs | `iet-sims/{trace-results,graphs}/ns-3.47/buffer-loss-stochastic-default-original-timing/` |
| ns-3.49 archived default full matrix results / graphs | `iet-sims/{trace-results,graphs}/ns-3.49/default-original-timing/` |
| Individual case | `<result-root>/<scenario>/<algorithm>/seed12345-run1-<configuration-id>/` |
| `resolved-config.txt` | Actual runtime settings |
| `aggregate.csv`, `flows.csv` | Sampled aggregate and per-flow measurements |
| `summary.csv`, `totals.txt` | Delivered payload, mean goodput and fairness summaries |
| `capacity.csv`, `delay.csv` | Link conditions over time |
| `run-metadata.json`, `experiment-index*.json` | Execution status and source fingerprint |
| `_provenance/`, `_run-logs/` | Recorded sources/hashes and simulator logs |

At default timing, each completed case has 1001 aggregate samples, 5005 per-flow samples and five flow-summary rows. The final sample is at 100 seconds. Goodput is receiver-delivered payload; normalized Power combines throughput utilization and the base-to-measured RTT ratio. See the driver and resolved configuration for the exact definitions.

Add `--tracing` to a runner to record high-resolution socket events in `socket-events.csv`; use a separate output root. For driver-level options such as timing or queue size, inspect `./ns3 run 'tcp_qt_sims --help'` inside the appropriate source tree. Always supply `--sim_name=/your/output/root` for direct driver runs: its inherited default path refers to the original machine. Python runners supply this path automatically.

[RESULTS.md](iet-sims/RESULTS.md) describes archived comparisons and variants. Historical index files retain original absolute paths; the plotter discovers CSVs beneath the supplied `--results` root and does not require those original paths. The `validate_ns347_nonstatic.py` and `validate_ns349_matrix.py` scripts are tied to the original default folders and execution records; they are not generic validators for the new reproduction paths.

## 7. Reproducibility limits and provenance

The complete launchers were verified on 27 October 2026 from the renamed repository folder, including a parent path containing spaces. Both release builds and their selected TCP tests passed; each version completed 50 fresh simulations and generated ten PNG graphs. Timing, sample counts, delivered-byte accounting and source hashes passed validation. All 50 aggregate CSVs in each new matrix matched that version's archived reference results byte for byte. The verification used the existing Python 3.11 build environment and Matplotlib 3.3.2/NumPy 1.18.5 plotting environment on this Mac, rather than a new dependency installation.

The full run exposed whitespace truncation in the driver's `--sim_name` argument. Both driver copies now parse that output path through a callback that preserves spaces; experiment parameters and numerical results are unchanged. Verification outputs use `workflow-verification-20261027` beneath each version's local results/graphs folder. These verification results, graphs, per-version validation reports and successful launcher logs are included in the repository, separately from the original reference data. See [the workflow verification guide](iet-sims/WORKFLOW-VERIFICATION.md).

Both original installations built successfully and passed the relevant TCP tests before import; ns-3.49 also passed its `tcp-qt` suite. All five default static cases matched between versions, and 40 of 50 aggregate CSV outputs were identical. Ten nonstatic cases differed. Keep version labels in any reported comparison and do not assume the engines are interchangeable. These scripts reproduce the repository's experiment setup and figures; they do not guarantee exact agreement with every published article value or across hardware/compiler environments.

- `qt-source-manifest.json` records the active Qt source checksums at import.
- `source-manifest.json` records the original source-only snapshot; it is not a checksum list for later documentation/results updates.
- `iet-sims/results-manifest.json` records checksums and sizes for imported results and graphs.
- Original compiled provenance binaries and temporary locks are excluded; hash records remain. New runs create their own compiled-engine provenance locally.

The reference figures used Matplotlib 3.3.2 and NumPy 1.18.5; these older versions may require an older Python environment. Newer plotting versions can render the same data differently. Consult `graph-index.json` for the recorded environment of each archived graph set.

## Licensing

The simulator trees retain their upstream GPL licensing, copyright notices, AUTHORS and LICENSES directories. See [ns-3.47/LICENSE](ns-3.47/LICENSE), [ns-3.49/LICENSE](ns-3.49/LICENSE) and individual source headers. The unpublished article PDF is not distributed here.
