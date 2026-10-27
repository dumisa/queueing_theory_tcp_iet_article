#!/usr/bin/env bash
# Shared implementation for the two version-specific reproduction launchers.
set -euo pipefail

iet_reproduce() {
    local version="$1"
    shift
    local root ns3 python plot_python jobs build_jobs run_id dry_run arg
    root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd -P)"
    ns3="$root/ns-$version"
    jobs=4
    build_jobs="${IET_BUILD_JOBS:-4}"
    run_id=reproduction-all
    dry_run=0
    while [[ $# -gt 0 ]]; do
        arg="$1"
        case "$arg" in
            --help|-h)
                printf 'Usage: bash reproduce_ns%s.sh [--jobs N] [--build-jobs N] [--run-id NAME] [--dry-run]\n' "${version//./}"
                printf 'Build and test ns-%s, run all 50 default cases, and generate ten PNG graphs.\n' "$version"
                printf 'Outputs: iet-sims/{trace-results,graphs}/ns-%s/NAME/ (default NAME: reproduction-all).\n' "$version"
                printf 'Run from any directory. Requires macOS, C++23, CMake, Ninja, Eigen and Python with NumPy/Matplotlib.\n'
                printf 'Environment: IET_PYTHON, IET_PLOT_PYTHON, IET_BUILD_JOBS. --dry-run prints commands only.\n'
                return 0 ;;
            --jobs|--build-jobs|--run-id)
                [[ $# -ge 2 ]] || { printf 'Missing value for %s\n' "$arg" >&2; return 2; }
                case "$arg" in
                    --jobs) jobs="$2" ;;
                    --build-jobs) build_jobs="$2" ;;
                    --run-id) run_id="$2" ;;
                esac
                shift 2 ;;
            --dry-run) dry_run=1; shift ;;
            *) printf 'Unknown option: %s (use --help)\n' "$arg" >&2; return 2 ;;
        esac
    done
    [[ "$jobs" =~ ^[1-9][0-9]*$ && "$build_jobs" =~ ^[1-9][0-9]*$ ]] || {
        printf 'Worker counts must be positive integers.\n' >&2; return 2;
    }
    [[ "$run_id" =~ ^[A-Za-z0-9][A-Za-z0-9._-]*$ && "$run_id" != default-original-timing && "$run_id" != buffer-loss-stochastic-default-original-timing ]] || {
        printf 'Use a simple run name, distinct from the archived default folders.\n' >&2; return 2;
    }
    export PATH="$root/tools/bin:$PATH"
    python="${IET_PYTHON:-python3}"
    plot_python="${IET_PLOT_PYTHON:-$python}"
    local results="$root/iet-sims/trace-results/ns-$version/$run_id"
    local graphs="$root/iet-sims/graphs/ns-$version/$run_id"
    local runner=run_ns349_sims.py
    [[ "$version" != 3.47 ]] || runner=run_ns347_nonstatic.py
    export CCACHE_DIR="$ns3/.ccache"
    export DYLD_LIBRARY_PATH="$ns3/build/lib"
    export MPLCONFIGDIR="$root/iet-sims/.matplotlib-cache"
    export XDG_CACHE_HOME="$root/iet-sims/.cache"
    # The local helper includes eigen3/Eigen/Eigen, requiring its parent include directory.
    if command -v brew >/dev/null 2>&1; then
        local eigen_prefix
        eigen_prefix="$(brew --prefix eigen 2>/dev/null || true)"
        [[ -z "$eigen_prefix" ]] || export CPLUS_INCLUDE_PATH="$eigen_prefix/include${CPLUS_INCLUDE_PATH:+:$CPLUS_INCLUDE_PATH}"
    fi
    iet_reproduce_command() {
        printf 'Command: '; printf '%q ' "$@"; printf '\n'
        [[ "$dry_run" -eq 1 ]] || "$@"
    }
    iet_reproduce_pipeline() {
        if [[ "$dry_run" -eq 0 ]]; then
            [[ "$(uname -s)" == Darwin ]] || { printf 'These provenance runners currently require macOS.\n' >&2; return 1; }
            "$python" -c 'import sys; assert sys.version_info >= (3, 8), "Python 3.8+ required"'
            "$plot_python" -c 'import numpy, matplotlib'
            command -v cmake >/dev/null
            command -v ninja >/dev/null
        fi
        printf 'Simulator: ns-%s\nResults: %s\nGraphs: %s\n' "$version" "$results" "$graphs"
        (
            cd "$ns3"
            iet_reproduce_command "$python" ./ns3 configure --enable-examples --enable-tests --build-profile=release
            iet_reproduce_command "$python" ./ns3 build -j "$build_jobs"
            iet_reproduce_command "$python" ./test.py -s tcp
            if [[ "$version" == 3.49 ]]; then
                iet_reproduce_command "$python" ./test.py -s tcp-qt
            fi
        )
        iet_reproduce_command "$python" "$root/iet-sims/$runner" \
            --results "$results" --graphs "$graphs" --jobs "$jobs" \
            --duration 100 --seed 12345 --run 1 --scenarios static buffer loss stochastic
        iet_reproduce_command "$plot_python" "$root/iet-sims/plot_iet_sims.py" \
            --results "$results" --graphs "$graphs" --duration 100 --seed 12345 --run 1
        printf 'Finished ns-%s: %s\n' "$version" "$graphs"
    }
    if [[ "$dry_run" -eq 1 ]]; then
        iet_reproduce_pipeline
    else
        local log_dir="$root/iet-sims/trace-results/_launch-logs/ns-$version"
        mkdir -p "$log_dir"
        local log="$log_dir/$(date -u +%Y%m%dT%H%M%SZ)-$$.log"
        printf 'Pipeline log: %s\n' "$log"
        iet_reproduce_pipeline 2>&1 | tee "$log"
    fi
}
