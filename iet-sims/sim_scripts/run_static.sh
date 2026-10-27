#!/usr/bin/env bash
# Article Section 6.3.1: compare five TCP algorithms at the static bottleneck.
# Default matrix, seed and runtime values are defined in ../run_iet_sims.py.
# Example: ./run_static.sh --jobs=4 --seed=12345 --run=1 --duration=100
set -euo pipefail
source "$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)/_common.sh"
iet_matrix run_static static 'Article Section 6.3.1: compare five TCP algorithms at the static bottleneck.' "$@"
