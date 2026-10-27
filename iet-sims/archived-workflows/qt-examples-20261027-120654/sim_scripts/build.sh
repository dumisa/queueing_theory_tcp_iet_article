#!/usr/bin/env bash
# Build ns-3 including the three Qt examples using the existing ns-3 configuration; no clean/reset.
# Configure ns-3 first if this is a fresh checkout. IET_BUILD_JOBS defaults to 4.
set -euo pipefail
source "$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)/_common.sh"
python="$(iet_python)"
if [[ "${1:-}" == --help || "${1:-}" == -h ]]; then
    iet_help build 'Build ns-3 including the three Qt examples in the existing configured ns-3 tree.'
    printf 'Default build workers: 4. Override with IET_BUILD_JOBS=2.\n'
    "$python" "$IET_NS3_DIR/ns3" build --help
    exit 0
fi
export PATH="$IET_BASE_DIR/../tools/bin:$PATH"
cd -- "$IET_NS3_DIR"
iet_run_logged build "$python" "$IET_NS3_DIR/ns3" build -j "${IET_BUILD_JOBS:-4}" "$@"
