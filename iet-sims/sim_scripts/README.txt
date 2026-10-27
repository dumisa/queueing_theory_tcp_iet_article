IET tcp_qt_sims launchers (restored 27 October 2026)
==================================================
Run from any working directory:
  ./build.sh
  ./run_static.sh --plot
  ./run_buffer.sh
  ./run_loss.sh
  ./run_stochastic.sh
  ./run_all.sh
  ./plot_results.sh

All simulation launchers use run_iet_sims.py and the compiled tcp_qt_sims target.
plot_results.sh uses plot_iet_sims.py and the original CSV output schema.
run_all.sh runs the 50 article configurations and creates the ten PNG figures.
Defaults: 100 seconds, five TCP algorithms, seed 12345, run 1, two workers.
Options: --duration, --jobs, --seed, --run, --algorithms, --scenarios,
--results, --tracing, --plot, --plot-python, --graphs. Use --help for details.
The legacy runner does not support --dry-run, --no-plot, --flows or --interval.
Build before running after any C++ changes. IET_BUILD_JOBS defaults to 4.
Interpreter overrides: IET_PYTHON, IET_PLOT_PYTHON. Log override: IET_LOG_DIR.

Results: ../trace-results/<scenario>/<algorithm>/seed.../
Graphs: ../graphs/
The runner checks complete CSV outputs and source fingerprints, saves source
snapshots and run records, and locks each run directory against duplicate work.
New source fingerprints regenerate results under existing configuration paths.

The alternative three-example workflow is saved in ../archived-workflows/.
Its .data results remain in ../trace-results/qt-examples/ and its graphs remain
in ../graphs/qt-examples/. They are not consumed by this restored pipeline.
The optional native plotting helpers remain available but are not launched.
The three alternative example sources and contrib modules remain on disk;
the example CMake targets have been replaced by tcp_qt_sims.
See ../README.txt for the article experiment definitions and CSV metrics.
