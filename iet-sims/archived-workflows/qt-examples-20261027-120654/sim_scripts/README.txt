Qt example launchers
====================
Run these scripts from any working directory. Build first:
  ./build.sh
  ./run_static.sh
  ./run_buffer.sh
  ./run_loss.sh
  ./run_stochastic.sh
  ./run_step_delay.sh
  ./run_all.sh
  ./plot_results.sh

run_all runs 50 configurations (five TCP algorithms across static, three buffer,
three loss and three stochastic cases) and plots them. Step-delay is an optional
extension, or use run_all.sh --scenarios static buffer loss stochastic step-delay.

Common options:
  --duration 100 --interval 0.1 --flows 5 --jobs 2 --seed 12345 --run 1
  --algorithms TcpQtSdr TcpQtTor
  --results /path/to/results --graphs /path/to/graphs
  --plot / --no-plot
  --dry-run  (show exact commands; do not run simulations)
Duration must exceed 3 seconds because the topology stops senders 3 seconds
before simulation end. Short smoke test:
  ./run_all.sh --duration 6 --algorithms TcpQtSdr --jobs 2
Build workers: IET_BUILD_JOBS=4. Interpreter overrides: IET_PYTHON and
IET_PLOT_PYTHON. IET_LOG_DIR overrides shell invocation log location.

Targets:
  static/buffer/loss -> tcp_qt_static
  stochastic        -> tcp_qt_stochBW
  step-delay        -> tcp_qt_step_delay
The shared tcp_qt_examples.h and contrib/my-dumbbell, contrib/my_maths_tools
are compiled dependencies. tcp_qt_sims is no longer used by these launchers.

New results: ../trace-results/qt-examples/<scenario>/<algorithm>/<variant>/
seed<seed>-run<run>-<configuration-hash>/native/...
Each run records command, source and binary hashes, trace checksums and logs.
Source snapshots are in qt-examples/_provenance. Completed matching runs are
reused only if trace checksums match; partial native traces are archived.
Always rebuild after changing C++ sources. Seeds/runs are passed as ns-3 global
values for every example and as OU parameters for the stochastic example.
Socket tracing stays enabled because aggregate metrics depend on its callbacks.

Plots: ../graphs/qt-examples/<experiment-index-name>/*.png
plot_results.sh uses the latest successful batch. To plot a particular batch:
  ./plot_results.sh --index /path/to/experiment-index-....json
Each scenario/variant gets a three-panel PNG (in-flight, RTT, sender-estimated
goodput). The plot checks trace hashes and never converts native traces into
legacy CSV metrics or mixes them with historical datasets.

Scientific differences from the old driver:
- Native examples currently default to PfifoFastQueueDisc; the buffer sweep sets
  queueSize_bytes = requested packets * 1448. These are not labelled DropTail.
- Native goodput estimates transmitted bytes minus retransmissions at sender;
  it is not measured receiver-delivered payload. RTT is rounded to integer ms.
- The stochastic example uses its native OU implementation, capped at 175 Mbps,
  with (tau,sigma) = (8,.15), (2,.45), (.5,1). It does not expose the old lower
  capacity bound; these runs should not be claimed as exact legacy reproductions.
- Step-delay retains example defaults at 15, 25 and 35 seconds. Short runs may
  end before changes occur.
Historical CSV data and article PNGs remain untouched. plot_iet_sims.py is
retained for those old CSV datasets. run_iet_sims_legacy.py is an archived runner
that requires the now-excluded tcp_qt_sims target; do not use it for new runs.
