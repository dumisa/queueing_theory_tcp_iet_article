#!/usr/bin/env bash
# Build ns-3.47, run the complete IET matrix, and plot its results.
set -euo pipefail
source "$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)/iet-sims/sim_scripts/_reproduce.sh"
iet_reproduce 3.47 "$@"
