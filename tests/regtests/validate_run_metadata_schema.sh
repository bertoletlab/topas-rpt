#!/usr/bin/env bash
set -euo pipefail

# Validates run_metadata.json schema/consistency for regtests and example smoke.
#
# Usage:
#   tests/regtests/validate_run_metadata_schema.sh /path/to/topas

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

echo "Running regtests to generate metadata..."
"${ROOT}/scripts/run_regtests_with_built_topas.sh" "${TOPAS_BIN}" >/tmp/validate_metadata_regtests.log

echo "Running example smoke to generate metadata..."
"${ROOT}/scripts/run_examples_with_built_topas.sh" "${TOPAS_BIN}" smoke >/tmp/validate_metadata_example.log
EXAMPLE_DIR="$(sed -n 's/^RUN_DIR=//p' /tmp/validate_metadata_example.log | tail -n 1)"
if [[ -z "${EXAMPLE_DIR}" || ! -d "${EXAMPLE_DIR}" ]]; then
  echo "ERROR: Could not resolve example output directory."
  exit 1
fi

ACTIVITYMAP_DIR=""
if [[ -n "${ACTIVITYMAP_CT_DICOM_DIR:-}" && -n "${ACTIVITYMAP_NM_DICOM_DIR:-}" ]]; then
  echo "Running activity_map smoke example to generate metadata..."
  "${ROOT}/scripts/run_activity_map_example_with_built_topas.sh" "${TOPAS_BIN}" smoke \
    >/tmp/validate_metadata_activity_map.log
  ACTIVITYMAP_DIR="$(sed -n 's/^RUN_DIR=//p' /tmp/validate_metadata_activity_map.log | tail -n 1)"
  if [[ -n "${ACTIVITYMAP_DIR}" && ! -d "${ACTIVITYMAP_DIR}" ]]; then
    echo "ERROR: Could not resolve activity_map output directory."
    exit 1
  fi
fi

validate_with_jq() {
  local f="$1"
  jq -e '
    .schema_version == "tsrts.run_metadata.v1" and
    (.mode | type == "string" and length > 0) and
    (.mode_settings | type == "object") and
    (.entries | type == "array" and length > 0) and
    all(.entries[];
      has("run_id") and
      has("time_start_s") and
      has("time_end_s") and
      has("step_duration_s") and
      has("n_decays_step") and
      has("n_decays_first") and
      has("n_histories_step") and
      has("correct_by_number_of_histories") and
      has("initial_activity_bq") and
      has("effective_activity_bq") and
      has("primary_weight")
    ) and
    all(.entries[];
      (.run_id | type == "number") and
      (.time_start_s | type == "number") and
      (.time_end_s | type == "number") and
      (.step_duration_s | type == "number") and
      (.n_decays_step | type == "number") and
      (.n_decays_first | type == "number") and
      (.n_histories_step | type == "number") and
      (.correct_by_number_of_histories | type == "boolean") and
      (.initial_activity_bq | type == "number") and
      (.effective_activity_bq | type == "number") and
      (.primary_weight | type == "number")
    ) and
    all(.entries[]; .time_end_s >= .time_start_s) and
    all(.entries[]; ((.time_end_s - .time_start_s - .step_duration_s) | abs) < 1e-9) and
    (reduce .entries[].run_id as $id ({prev: null, ok: true};
      .ok = (
        .ok and
        (($id | floor) == $id) and
        (.prev == null or $id == (.prev + 1))
      ) |
      .prev = $id
    ) | .ok)
  ' "${f}" >/dev/null
}

validate_with_shell_fallback() {
  local f="$1"
  local required_top=("schema_version" "mode" "mode_settings" "entries")
  local required_entry=(
    "run_id" "time_start_s" "time_end_s" "step_duration_s"
    "n_decays_step" "n_decays_first" "n_histories_step"
    "correct_by_number_of_histories" "initial_activity_bq"
    "effective_activity_bq" "primary_weight"
  )

  for k in "${required_top[@]}"; do
    rg -q "\"${k}\"" "${f}" || return 1
  done
  rg -q '"schema_version"[[:space:]]*:[[:space:]]*"tsrts.run_metadata.v1"' "${f}" || return 1

  local n_entries
  n_entries="$(rg -c '"run_id"' "${f}")"
  [[ "${n_entries}" -gt 0 ]] || return 1
  for k in "${required_entry[@]}"; do
    local count
    count="$(rg -c "\"${k}\"" "${f}")"
    [[ "${count}" -eq "${n_entries}" ]] || return 1
  done
}

validate_file() {
  local f="$1"
  if command -v jq >/dev/null 2>&1; then
    validate_with_jq "${f}"
  else
    validate_with_shell_fallback "${f}"
  fi
}

fail_count=0
files=()
while IFS= read -r f; do
  files+=("$f")
done < <(find "${ROOT}/AuxFiles/regtests/runs" -type f -name run_metadata.json | sort)
files+=("${EXAMPLE_DIR}/run_metadata.json")
if [[ -n "${ACTIVITYMAP_DIR}" ]]; then
  files+=("${ACTIVITYMAP_DIR}/run_metadata.json")
fi

for f in "${files[@]}"; do
  if [[ ! -f "${f}" ]]; then
    echo "FAIL: missing metadata file ${f}"
    fail_count=$((fail_count + 1))
    continue
  fi
  if validate_file "${f}"; then
    echo "PASS: ${f}"
  else
    echo "FAIL: ${f}"
    fail_count=$((fail_count + 1))
  fi
done

if [[ "${fail_count}" -ne 0 ]]; then
  echo "Metadata schema validation failed (${fail_count} file(s))."
  exit 1
fi

echo "Metadata schema validation passed."
