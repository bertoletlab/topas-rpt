#!/usr/bin/env bash
set -euo pipefail

# End-to-end validator for RadioactiveTimeSource features and legacy parity.
#
# Usage:
#   tests/regtests/validate_all_features_vs_legacy.sh /path/to/topas
#   tests/regtests/validate_all_features_vs_legacy.sh /Applications/TOPAS/topas-env.sh
#
# What it covers:
# - Legacy-equivalent parity (invitro_bind regtests + smoke example) vs baseline artifacts.
# - Parent/uniform mode time-feature + chain-control contract.
# - Run metadata schema contract across generated outputs.
# - invitro_bind washout contract (trace on/off parity, washout on a step boundary).
# - invitro_bind Manual model contract (split kept every step, ModeParams keys, washout).
# - Decay timing (Bi-212 against Bateman) and one position per decay with the chain off.
# - Decay-chain bookkeeping independent of thread count and CorrectByNumberOfHistories.
# - Output files: default abundance file, full-precision times and read-back, uptake id check.
# - Activity-map smoke run (optional; SKIP without required DICOM env).

if [[ $# -ne 1 ]]; then
  echo "Usage: $0 /path/to/topas"
  exit 2
fi

TOPAS_BIN="$1"
if ! command -v "${TOPAS_BIN}" >/dev/null 2>&1 && [[ ! -x "${TOPAS_BIN}" ]]; then
  echo "ERROR: TOPAS binary/command not found: ${TOPAS_BIN}"
  exit 2
fi

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

if [[ -z "${TOPAS_G4_DATA_DIR:-}" && -z "${G4LEVELGAMMADATA:-}" ]]; then
  if [[ -f "/Applications/GEANT4/geant4-install/bin/geant4.sh" ]]; then
    # shellcheck source=/dev/null
    source "/Applications/GEANT4/geant4-install/bin/geant4.sh"
  elif [[ -d "/Applications/GEANT4/G4DATA" ]]; then
    export TOPAS_G4_DATA_DIR="/Applications/GEANT4/G4DATA"
  else
    echo "ERROR: Geant4 data environment is not set and defaults were not found."
    exit 2
  fi
fi

PASS=0
FAIL=0
SKIP=0

run_step() {
  local name="$1"
  shift
  echo
  echo "==> ${name}"
  if "$@"; then
    echo "PASS: ${name}"
    PASS=$((PASS + 1))
  else
    echo "FAIL: ${name}"
    FAIL=$((FAIL + 1))
  fi
}

run_optional_step() {
  local name="$1"
  shift
  echo
  echo "==> ${name}"
  set +e
  local out
  out="$($@ 2>&1)"
  local rc=$?
  set -e
  if [[ ${rc} -eq 0 ]]; then
    echo "${out}"
    if echo "${out}" | rg -q '^SKIP:'; then
      echo "SKIP: ${name}"
      SKIP=$((SKIP + 1))
    else
      echo "PASS: ${name}"
      PASS=$((PASS + 1))
    fi
  else
    echo "${out}"
    echo "FAIL: ${name}"
    FAIL=$((FAIL + 1))
  fi
}

run_step "legacy parity (baseline numeric comparison)" \
  "${ROOT}/tests/regtests/verify_baseline_parity.sh" "${TOPAS_BIN}"

run_step "invitro smoke determinism" \
  "${ROOT}/tests/regtests/validate_invitro_smoke_determinism.sh" "${TOPAS_BIN}"

run_step "parent mode: time-feature + chain-control contract" \
  "${ROOT}/tests/regtests/validate_time_feature_decay_chain_control.sh" "${TOPAS_BIN}"

run_step "run metadata schema" \
  "${ROOT}/tests/regtests/validate_run_metadata_schema.sh" "${TOPAS_BIN}"

run_step "time/decay kernel smoke contract" \
  "${ROOT}/tests/regtests/validate_time_decay_kernel_smoke.sh" "${TOPAS_BIN}"

run_step "invitro_bind washout: trace on/off parity and step-boundary washout" \
  "${ROOT}/tests/regtests/validate_washout_trace_consistency.sh" "${TOPAS_BIN}"

run_step "invitro_bind Manual model: split kept every step, ModeParams keys, washout" \
  "${ROOT}/tests/regtests/validate_manual_model.sh" "${TOPAS_BIN}"

run_step "decay timing against Bateman; one position per decay" \
  "${ROOT}/tests/regtests/validate_decay_chain_timing.sh" "${TOPAS_BIN}"

run_step "decay-chain bookkeeping: thread count and CorrectByNumberOfHistories" \
  "${ROOT}/tests/regtests/validate_multithread_chain_bookkeeping.sh" "${TOPAS_BIN}"

run_step "output files: default abundance file, read-back, uptake ids" \
  "${ROOT}/tests/regtests/validate_output_files.sh" "${TOPAS_BIN}"

run_step "decay chain keeps decaying once scaled histories reach zero" \
  "${ROOT}/tests/regtests/validate_zero_history_steps.sh" "${TOPAS_BIN}"

run_step "restart from an abundance file, with and without UseFractionOfActivityFromFile" \
  "${ROOT}/tests/regtests/validate_abundance_restart.sh" "${TOPAS_BIN}"

run_optional_step "activity_map mode contract (optional)" \
  "${ROOT}/tests/regtests/validate_activity_map_contract.sh" "${TOPAS_BIN}"

echo
echo "Summary: PASS=${PASS} FAIL=${FAIL} SKIP=${SKIP}"
if [[ ${FAIL} -ne 0 ]]; then
  exit 1
fi
