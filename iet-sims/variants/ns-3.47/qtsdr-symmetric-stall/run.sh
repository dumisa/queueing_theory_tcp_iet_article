#!/usr/bin/env bash
set -euo pipefail
variant="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
installation="$(cd -- "$variant/../../../.." && pwd -P)"
export DYLD_LIBRARY_PATH="$variant/lib:$installation/ns-3.47/build/lib"
exec "$installation/ns-3.47/build/examples/tcp/ns3.47-tcp_qt_sims" --sim_name="$installation/iet-sims/trace-results/ns-3.47/qtsdr-symmetric-stall-study/symmetric" "$@"
