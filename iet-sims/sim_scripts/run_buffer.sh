#!/usr/bin/env bash
# Article Section 6.3.2: sweep 483, 690 and 1381 DropTail packets.
# Default matrix, seed and runtime values are defined in ../run_iet_sims.py.
# Example: ./run_buffer.sh --jobs=4 --seed=12345 --run=1 --duration=100
set -euo pipefail
source "$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)/_common.sh"
iet_matrix run_buffer buffer 'Article Section 6.3.2: sweep 483, 690 and 1381 DropTail packets.' "$@"
