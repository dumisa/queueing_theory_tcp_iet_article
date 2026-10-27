#!/usr/bin/env bash
# Shared launcher infrastructure. Experiment definitions remain in run_iet_sims.py.
set -euo pipefail

IET_BASE_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd -P)"
IET_NS3_DIR="$(cd -- "$IET_BASE_DIR/../ns-3.49" && pwd -P)"
export MPLCONFIGDIR="${MPLCONFIGDIR:-$IET_BASE_DIR/.matplotlib-cache}"
export XDG_CACHE_HOME="${XDG_CACHE_HOME:-$IET_BASE_DIR/.cache}"

iet_error() {
    printf 'IET simulations: %s\n' "$*" >&2
    return 1
}

iet_python() {
    local candidate
    if [[ -n "${IET_PYTHON:-}" ]]; then
        candidate="$IET_PYTHON"
        [[ -x "$candidate" ]] || candidate="$(command -v -- "$candidate" || true)"
        [[ -n "$candidate" && -x "$candidate" ]] || { iet_error 'IET_PYTHON is not executable.'; return 1; }
        "$candidate" -c 'import sys; sys.exit(0 if sys.version_info >= (3, 8) else 1)' || {
            iet_error 'Python 3.8 or newer is required.'; return 1;
        }
        printf '%s\n' "$candidate"
        return
    fi
    for candidate in "$IET_BASE_DIR/../tools/bin/python3" "$(command -v python3 || true)"; do
        if [[ -n "$candidate" && -x "$candidate" ]] &&
            "$candidate" -c 'import sys; sys.exit(0 if sys.version_info >= (3, 8) else 1)'; then
            printf '%s\n' "$candidate"
            return
        fi
    done
    iet_error 'No Python 3.8+ found. Set IET_PYTHON to a Python executable.'
}

iet_plot_python() {
    local candidate
    export MPLCONFIGDIR="${MPLCONFIGDIR:-$IET_BASE_DIR/.matplotlib-cache}"
    export XDG_CACHE_HOME="${XDG_CACHE_HOME:-$IET_BASE_DIR/.cache}"
    if [[ -n "${IET_PLOT_PYTHON:-}" ]]; then
        candidate="$IET_PLOT_PYTHON"
        [[ -x "$candidate" ]] || candidate="$(command -v -- "$candidate" || true)"
        if [[ -n "$candidate" && -x "$candidate" ]] &&
            "$candidate" -c 'import sys, matplotlib, numpy; sys.exit(0 if sys.version_info >= (3, 8) else 1)' >/dev/null 2>&1; then
            printf '%s\n' "$candidate"
            return
        fi
        iet_error 'IET_PLOT_PYTHON must be Python 3.8+ with Matplotlib and NumPy.'
        return 1
    fi
    for candidate in "${IET_PYTHON:-}" /opt/anaconda3/bin/python "$(command -v python3 || true)"; do
        if [[ -n "$candidate" && -x "$candidate" ]] &&
            "$candidate" -c 'import sys, matplotlib, numpy; sys.exit(0 if sys.version_info >= (3, 8) else 1)' >/dev/null 2>&1; then
            printf '%s\n' "$candidate"
            return
        fi
    done
    iet_error 'Set IET_PLOT_PYTHON to Python 3.8+ with Matplotlib and NumPy installed.'
}

iet_help() {
    local name="$1" purpose="$2"
    printf '%s — %s\n\n' "$name" "$purpose"
    printf 'Runs from any working directory. All extra arguments are forwarded unchanged.\n'
    printf 'Simulation defaults: 100 seconds, seed 12345, run 1, 2 workers.\n'
    printf 'Results: %s/trace-results/\nGraphs:  %s/graphs/\n' "$IET_BASE_DIR" "$IET_BASE_DIR"
    printf 'Environment overrides: IET_PYTHON, IET_PLOT_PYTHON, IET_LOG_DIR.\n\n'
}

iet_run_logged() {
    local name="$1"
    shift
    local root="${IET_LOG_DIR:-$IET_BASE_DIR/trace-results/_launch-logs}"
    local session="$root/$(date -u +%Y%m%dT%H%M%SZ)-$name-$$"
    mkdir -p -- "$session"
    {
        printf 'launcher=%s\nstartedUtc=%s\nworkingDirectory=%s\n' "$name" "$(date -u +%Y-%m-%dT%H:%M:%SZ)" "$PWD"
        printf 'command='
        printf '%q ' "$@"
        printf '\n'
        printf 'commandArgument=%q\n' "$@"
        printf 'platform='
        uname -srm
        if [[ -f "$IET_NS3_DIR/VERSION" ]]; then
            printf 'ns3Version='
            cat "$IET_NS3_DIR/VERSION"
            printf '\n'
        fi
        printf 'pythonVersion='
        "$1" --version 2>&1
        if command -v shasum >/dev/null 2>&1; then
            shasum -a 256 "$IET_BASE_DIR/run_iet_sims.py" "$IET_BASE_DIR/plot_iet_sims.py"
            local binary
            for binary in "$IET_NS3_DIR"/build/examples/tcp/ns3.*-tcp_qt_sims; do
                [[ ! -f "$binary" ]] || shasum -a 256 "$binary"
            done
        fi
        if [[ -n "${IET_ACTIVE_PLOT_PYTHON:-}" ]]; then
            printf 'plotPython=%s\n' "$IET_ACTIVE_PLOT_PYTHON"
            "$IET_ACTIVE_PLOT_PYTHON" -c 'import matplotlib, numpy; print("matplotlibVersion=" + matplotlib.__version__); print("numpyVersion=" + numpy.__version__)'
        fi
        if command -v shasum >/dev/null 2>&1; then
            shasum -a 256 "$IET_BASE_DIR/sim_scripts/_common.sh" "$IET_BASE_DIR/sim_scripts/$name.sh"
        fi
    } > "$session/invocation.txt"
    printf 'Launcher log: %s\n' "$session/console.log"
    local -a status
    set +e
    "$@" 2>&1 | tee "$session/console.log"
    status=("${PIPESTATUS[@]}")
    set -e
    local result="${status[0]}"
    if [[ "$result" -eq 0 && "${status[1]}" -ne 0 ]]; then
        result="${status[1]}"
    fi
    printf 'finishedUtc=%s\nexitStatus=%s\n' "$(date -u +%Y-%m-%dT%H:%M:%SZ)" "$result" >> "$session/invocation.txt"
    return "$result"
}

iet_matrix() {
    local name="$1" scenario="$2" purpose="$3"
    shift 3
    local python
    python="$(iet_python)"
    if [[ "${1:-}" == --help || "${1:-}" == -h ]]; then
        iet_help "$name" "$purpose"
        "$python" "$IET_BASE_DIR/run_iet_sims.py" --help
        return
    fi
    export PATH="$IET_BASE_DIR/../tools/bin:$PATH"
    local -a command
    command=("$python" "$IET_BASE_DIR/run_iet_sims.py")
    if [[ "$scenario" != all ]]; then
        command+=(--scenarios "$scenario")
    fi
    local plot=0 arg previous='' plot_python
    [[ "$scenario" != all ]] || plot=1
    for arg in "$@"; do
        [[ "$arg" != --plot ]] || plot=1
        if [[ "$previous" == --plot-python ]]; then
            export IET_PLOT_PYTHON="$arg"
        elif [[ "$arg" == --plot-python=* ]]; then
            export IET_PLOT_PYTHON="${arg#--plot-python=}"
        fi
        previous="$arg"
    done
    if [[ "$plot" -eq 1 ]]; then
        plot_python="$(iet_plot_python)"
        IET_ACTIVE_PLOT_PYTHON="$plot_python"
        command+=(--plot --plot-python "$plot_python")
        printf 'Plot Python: %s\n' "$plot_python"
    fi
    command+=("$@")
    iet_run_logged "$name" "${command[@]}"
}
