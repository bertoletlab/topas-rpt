#!/usr/bin/env bash
set -euo pipefail

# Runs scientific showcase workflows for the three supported modes.
# Usage: scripts/run_all_mode_scientific_showcases.sh /path/to/topas

if [[ $# -ne 1 ]]; then
  echo "Usage: $0 /path/to/topas"
  exit 2
fi
TOPAS_BIN="$1"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

run_step() {
  local name="$1"
  local cmd="$2"
  echo
  echo "==> ${name}"
  set +e
  local out
  out="$(${cmd} 2>&1)"
  local rc=$?
  set -e
  echo "${out}"
  if [[ ${rc} -ne 0 ]]; then
    echo "FAIL: ${name}"
    return 1
  fi
  if echo "${out}" | rg -q '^SKIP:'; then
    echo "SKIP: ${name}"
    return 0
  fi
  echo "PASS: ${name}"
}

run_step "uniform showcase" "${ROOT}/scripts/run_uniform_scientific_showcase.sh ${TOPAS_BIN}"
run_step "invitro showcase" "${ROOT}/scripts/run_invitro_scientific_showcase.sh ${TOPAS_BIN}"
run_step "activity_map showcase" "${ROOT}/scripts/run_activity_map_scientific_showcase.sh ${TOPAS_BIN}"
