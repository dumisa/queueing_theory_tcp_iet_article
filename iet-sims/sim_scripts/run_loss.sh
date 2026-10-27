#!/usr/bin/env bash
# Article Section 6.3.3: sweep packet error rates 1e-6, 1e-4 and 1e-2.
# Default matrix, seed and runtime values are defined in ../run_iet_sims.py.
# Example: ./run_loss.sh --jobs=4 --seed=12345 --run=1 --duration=100
set -euo pipefail
source "$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)/_common.sh"
iet_matrix run_loss loss 'Article Section 6.3.3: sweep packet error rates 1e-6, 1e-4 and 1e-2.' "$@"
