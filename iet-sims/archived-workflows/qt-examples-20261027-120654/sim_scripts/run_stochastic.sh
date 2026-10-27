#!/usr/bin/env bash
# Article Section 6.3.4 and Appendix C: low, moderate and high OU service volatility.
# Default matrix, seed and runtime values are defined in ../run_iet_sims.py.
# Example: ./run_stochastic.sh --jobs=4 --seed=12345 --run=1 --duration=100
set -euo pipefail
source "$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)/_common.sh"
iet_matrix run_stochastic stochastic 'Article Section 6.3.4 and Appendix C: low, moderate and high OU service volatility.' "$@"
