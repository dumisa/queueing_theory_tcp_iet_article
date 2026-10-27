#!/usr/bin/env bash
# Run the step-delay example across the selected TCP algorithms.
set -euo pipefail
source "$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)/_common.sh"
iet_matrix run_step_delay step-delay 'Run the step-delay example across selected TCP algorithms.' "$@"
