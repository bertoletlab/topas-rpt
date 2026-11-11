#!/usr/bin/env bash
set -euo pipefail

# An invitro_bind run must write its mode_settings in run_metadata.json also when it has a single
# step. The base generator used to write the file from its constructor, where virtual calls reach
# only the base class, so mode_settings came out empty; later steps rewrote the file, which hid the
# problem except in single-step runs. Runs the canonical invitro_bind deck with one and with two
# steps and checks that both record the binding model and the activity per mL.
#
# Usage:
#   tests/regtests/validate_single_step_metadata.sh /path/to/topas

if [[ $# -ne 1 ]]; then
  echo "Usage: $0 /path/to/topas"
  exit 2
fi

TOPAS_BIN="$1"
if ! command -v "${TOPAS_BIN}" >/dev/null 2>&1 && [[ ! -x "${TOPAS_BIN}" ]]; then
  echo "ERROR: TOPAS binary/command not found: ${TOPAS_BIN}"
  exit 2
fi
if [[ -x "${TOPAS_BIN}" ]]; then
  TOPAS_BIN="$(cd "$(dirname "${TOPAS_BIN}")" && pwd)/$(basename "${TOPAS_BIN}")"
fi

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

RUN_DIR="$(mktemp -d "${TMPDIR:-/tmp}/tsrts_single_step.XXXXXX")"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BASE_DECK="${ROOT}/examples/canonical/invitro_bind_minimal.txt"

fail=0
for steps in 1 2; do
  mkdir -p "${RUN_DIR}/s${steps}"
  { grep -vE '^[a-z]+:(So/BindSource/NumberOfHistoriesInRun|Tf/NumberOfSequentialTimes) *=' "${BASE_DECK}"
    echo "i:So/BindSource/NumberOfHistoriesInRun = 20"
    echo "i:Tf/NumberOfSequentialTimes = ${steps}"; } > "${RUN_DIR}/s${steps}/deck.txt"
  (cd "${RUN_DIR}/s${steps}" && "${TOPAS_BIN}" deck.txt > run.log 2>&1) || {
    echo "ERROR: TOPAS run failed (${steps} step(s)); see ${RUN_DIR}/s${steps}/run.log"; exit 1; }
  meta="${RUN_DIR}/s${steps}/run_metadata_invitro.json"
  [[ -f "${meta}" ]] || { echo "ERROR: missing ${meta}"; exit 1; }
  model="$(jq -r '.mode_settings.binding_model // empty' "${meta}")"
  activity="$(jq -r '.mode_settings.activity_per_ml // empty' "${meta}")"
  entries="$(jq '.entries | length' "${meta}")"
  if [[ -z "${model}" || -z "${activity}" ]]; then
    echo "FAIL: ${steps} step(s): mode_settings lacks binding_model/activity_per_ml"; fail=1
  fi
  if [[ "${entries}" != "${steps}" ]]; then
    echo "FAIL: ${steps} step(s): ${entries} metadata entries"; fail=1
  fi
done

if [[ "${fail}" -ne 0 ]]; then
  echo "Single-step metadata check failed (${RUN_DIR})."
  exit 1
fi
echo "PASS: invitro_bind mode_settings written for single- and two-step runs (${RUN_DIR})"
