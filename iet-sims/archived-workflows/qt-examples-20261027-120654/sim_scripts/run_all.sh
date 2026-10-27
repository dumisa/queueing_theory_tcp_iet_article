#!/usr/bin/env bash
# Run all 50 Qt example configurations, then generate PNG graphs.
# Default matrix, seed and runtime values are defined in ../run_iet_sims.py.
# Example: ./run_all.sh --jobs=4 --seed=12345 --run=1 --duration=100
set -euo pipefail
source "$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)/_common.sh"
iet_matrix run_all all 'Run all 50 Qt example configurations, then generate PNG graphs.' "$@"
