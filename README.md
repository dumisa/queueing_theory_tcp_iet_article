# IET TCP simulation sources

This repository contains separate, complete ns-3.47 and ns-3.49 source trees for the IET TCP experiments, including the local QtSDR and QtTOR congestion controllers. It is a source snapshot of two installations, rather than a copy of machine-specific compiled installations.

## Contents

- `ns-3.47/` and `ns-3.49/`: simulator sources, Qt models, CMake registrations and `examples/tcp/tcp_qt_sims.cc`.
- `iet-sims/`: Python runners, validation and graph-generation scripts, shell launchers and separate experimental controller sources.
- `qt-source-manifest.json`: SHA-256 checksums of the active Qt model files and simulation drivers at import.
- `source-manifest.json`: checksums of all imported files, for checking this source snapshot.

Both defaults use the original QtSDR `<=` stall correction, application start at 10 microseconds, and sampling at 0.001 + k * 0.1 seconds. The ns-3.49 controller implementation matches ns-3.47, with the required ns-3.49 include changes. Modified controller variants are separate from the defaults.

Each experiment runs one algorithm independently with five flows all using that algorithm. The matrix contains five static cases and 15 cases each for buffer, loss and stochastic capacity experiments, for 50 runs per version. Default duration is 100 seconds, seed 12345, run 1.

## Build on macOS

Install Xcode command-line tools, Python 3, CMake, Ninja, and Eigen3 (needed by the additional ns-3.49 contrib modules). For example, with Homebrew available:

```sh
xcode-select --install
brew install cmake ninja eigen python
python3 -m venv tools
tools/bin/python3 -m pip install numpy matplotlib
export PATH="$PWD/tools/bin:$PATH"
export IET_PYTHON="$PWD/tools/bin/python3"
export IET_PLOT_PYTHON="$IET_PYTHON"
bash build_installations.sh
```

The build script configures release builds with examples and tests, then builds both versions. Compilation can take several minutes. Both original installations were built on Intel macOS with Apple Clang 17. The version-specific provenance wrappers currently use macOS `.dylib` names; adapt those for Linux before using them there.

## Reproduce the experiments

Run these commands from the repository root after building, with the environment above set. Results and PNG graphs remain separate by simulator version.

```sh
# ns-3.49: all 50 experiments and graphs
bash iet-sims/sim_scripts/run_ns349_all.sh

# ns-3.47: five static experiments and graphs
"$IET_PYTHON" iet-sims/run_ns347_static.py \
  --results "$PWD/iet-sims/trace-results/ns-3.47/default-original-timing" \
  --graphs "$PWD/iet-sims/graphs/ns-3.47/default-original-timing" \
  --plot --plot-python "$IET_PLOT_PYTHON"

# ns-3.47: buffer, loss and stochastic experiments and graphs
"$IET_PYTHON" iet-sims/run_ns347_nonstatic.py \
  --plot --plot-python "$IET_PLOT_PYTHON"
```

Use `--help` on the runners to inspect supported runtime options. Result folders contain resolved configuration, aggregate and per-flow CSV traces, summaries and source provenance. Graphs are generated as PNG files. Existing completed runs are reused only when their source fingerprints and configuration match. Keep each simulator version's output path distinct.

The older generic `run_all.sh` launcher targets ns-3.49 but uses shared output paths; prefer the version-specific commands above. Direct invocation of the C++ example retains the original installation's absolute default output path: always pass `--sim_name=/your/output/directory` for direct runs. The Python runners supply their own output paths.

## Validation and limitations

Before import, both installations passed their builds and relevant TCP tests. The ns-3.49 `tcp-qt` and `tcp` suites passed. The completed 50-case ns-3.49 run and corresponding ns-3.47 runs had matching timing, sample counts and byte accounting. All five static cases matched across versions; 40 of the 50 aggregate CSV outputs were identical. Ten nonstatic cases differed, so the simulator versions should not be treated as interchangeable.

This upload does not rerun the experiments. The local installations and existing results remain unchanged. Compiled binaries, build caches, Python environments, generated traces, graphs and the unpublished article PDF are excluded. Experimental source backups are retained separately; their original local paths and historical notes may refer to the original machine. Rebuild variants before running them; their old compiled libraries are excluded.

## Licensing

The ns-3 trees retain their upstream GPL licensing, copyright notices, AUTHORS and LICENSES directories. See `ns-3.47/LICENSE` and `ns-3.49/LICENSE`, and individual source headers. This repository does not replace or relicense upstream code. The article itself is not distributed here.
