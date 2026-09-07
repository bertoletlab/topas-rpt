#!/usr/bin/env bash
set -euo pipefail

# Minimal smoke hook for PR-B time/decay kernel bookkeeping.
# Verifies primary_weight consistency from run_metadata first entry:
#   primary_weight == n_decays_step / n_histories_step
# (for smoke defaults where correct_by_number_of_histories is true)
#
# Usage:
#   tests/regtests/validate_time_decay_kernel_smoke.sh /path/to/topas

if [[ $# -ne 1 ]]; then
  echo "Usage: $0 /path/to/topas"
  exit 2
fi

TOPAS_BIN="$1"
if [[ ! -x "${TOPAS_BIN}" ]]; then
  echo "ERROR: TOPAS binary not executable: ${TOPAS_BIN}"
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

"${ROOT}/scripts/run_examples_with_built_topas.sh" "${TOPAS_BIN}" smoke >/tmp/validate_time_decay_kernel_smoke.log 2>&1
RUN_DIR="$(sed -n 's/^RUN_DIR=//p' /tmp/validate_time_decay_kernel_smoke.log | tail -n 1)"
if [[ -z "${RUN_DIR}" || ! -d "${RUN_DIR}" ]]; then
  cat /tmp/validate_time_decay_kernel_smoke.log
  echo "ERROR: could not resolve smoke run output directory."
  exit 1
fi

META="${RUN_DIR}/run_metadata.json"
if [[ ! -f "${META}" ]]; then
  echo "ERROR: missing run metadata file: ${META}"
  exit 1
fi

N_DECAYS="$(sed -n 's/.*"n_decays_step":[[:space:]]*\([0-9eE+.-]*\).*/\1/p' "${META}" | head -n1)"
N_HIST="$(sed -n 's/.*"n_histories_step":[[:space:]]*\([0-9eE+.-]*\).*/\1/p' "${META}" | head -n1)"
P_WEIGHT="$(sed -n 's/.*"primary_weight":[[:space:]]*\([0-9eE+.-]*\).*/\1/p' "${META}" | head -n1)"
CBNH="$(sed -n 's/.*"correct_by_number_of_histories":[[:space:]]*\([a-z]*\).*/\1/p' "${META}" | head -n1)"

if [[ -z "${N_DECAYS}" || -z "${N_HIST}" || -z "${P_WEIGHT}" || -z "${CBNH}" ]]; then
  echo "ERROR: could not parse required fields from ${META}"
  exit 1
fi

if [[ "${CBNH}" != "true" ]]; then
  echo "SKIP: smoke kernel check expects correct_by_number_of_histories=true; got ${CBNH}"
  exit 0
fi

EXPECTED="$(awk -v n="${N_DECAYS}" -v h="${N_HIST}" 'BEGIN{if (h<=0){print "nan"} else {printf "%.16g", n/h}}')"
DIFF="$(awk -v a="${P_WEIGHT}" -v b="${EXPECTED}" 'BEGIN{d=a-b; if (d<0) d=-d; printf "%.16g", d}')"
OK="$(awk -v d="${DIFF}" 'BEGIN{if (d <= 1e-10) print "yes"; else print "no"}')"

if [[ "${OK}" != "yes" ]]; then
  echo "ERROR: primary_weight mismatch in ${META}"
  echo "  n_decays_step=${N_DECAYS}"
  echo "  n_histories_step=${N_HIST}"
  echo "  primary_weight=${P_WEIGHT}"
  echo "  expected=${EXPECTED}"
  echo "  abs_diff=${DIFF}"
  exit 1
fi

echo "PASS: time/decay kernel smoke (${META})"
