#!/usr/bin/env bash
# Generate the article's PNG comparisons from completed simulation CSVs.
# Example: ./plot_results.sh --run=2 --graphs=/path/to/graphs/run2
set -euo pipefail
source "$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)/_common.sh"
if [[ "${1:-}" == --help || "${1:-}" == -h ]]; then
    iet_help plot_results 'Generate PNG graphs from existing results; does not run simulations.'
    # argparse help still needs the plotting module imports in this program.
    plot_python="$(iet_plot_python)"
    "$plot_python" "$IET_BASE_DIR/plot_iet_sims.py" --help
    exit 0
fi
plot_python="$(iet_plot_python)"
IET_ACTIVE_PLOT_PYTHON="$plot_python"
iet_run_logged plot_results "$plot_python" "$IET_BASE_DIR/plot_iet_sims.py" "$@"
