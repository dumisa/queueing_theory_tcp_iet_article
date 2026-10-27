#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
export PATH="$ROOT/tools/bin:$PATH"
for version in 3.47 3.49; do
    (
        cd "$ROOT/ns-$version"
        export CCACHE_DIR="$PWD/.ccache"
        python3 ./ns3 configure --enable-examples --enable-tests --build-profile=release
        python3 ./ns3 build -j "${IET_BUILD_JOBS:-4}"
    )
done
