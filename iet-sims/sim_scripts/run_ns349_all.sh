#!/usr/bin/env bash
# Run the 50 ns-3.49 cases and generate their version-separated PNG graphs.
set -euo pipefail
source "$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)/_common.sh"
python="$(iet_python)"
if [[ "${1:-}" == --help || "${1:-}" == -h ]]; then
    printf 'Run all ns-3.49 experiments with default <= QtSDR and original timing.\n'
    printf 'Results and graphs are saved under their ns-3.49/default-original-timing folders.\n\n'
    "$python" "$IET_BASE_DIR/run_ns349_sims.py" --help
    exit 0
fi
plot_python="$(iet_plot_python)"
IET_ACTIVE_PLOT_PYTHON="$plot_python"
iet_run_logged run_ns349_all "$python" "$IET_BASE_DIR/run_ns349_sims.py" --plot --plot-python "$plot_python" "$@"
