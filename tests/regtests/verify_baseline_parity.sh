#!/usr/bin/env bash
set -euo pipefail

# Runs regtests + example smoke and compares numeric outputs to baseline artifacts.
#
# Usage:
#   tests/regtests/verify_baseline_parity.sh /path/to/topas

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
BASE="${ROOT}/tests/regtests/baseline"
TOL="${BASE}/tolerances.env"

if [[ ! -f "${TOL}" ]]; then
  echo "ERROR: Missing tolerances file: ${TOL}"
  exit 2
fi
source "${TOL}"

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

extract_numeric_tokens() {
  local f="$1"
  awk '
    function isnum(x) {
      return x ~ /^[-+]?(([0-9]*\.[0-9]+)|([0-9]+\.?[0-9]*))([eE][-+]?[0-9]+)?$/
    }
    {
      for (i = 1; i <= NF; i++) {
        token = $i
        gsub(/[,\{\}\[\]":]/, "", token)
        if (isnum(token)) print token
      }
    }' "${f}"
}

compare_numeric_files() {
  local baseline="$1"
  local current="$2"
  local abs_tol="$3"
  local rel_tol="$4"

  local n_base n_cur
  n_base="$(extract_numeric_tokens "${baseline}" | wc -l | tr -d ' ')"
  n_cur="$(extract_numeric_tokens "${current}" | wc -l | tr -d ' ')"
  if [[ "${n_base}" != "${n_cur}" ]]; then
    echo "FAIL: token count mismatch: ${current} (baseline=${n_base}, current=${n_cur})"
    return 1
  fi

  paste <(extract_numeric_tokens "${baseline}") <(extract_numeric_tokens "${current}") \
    | awk -v abs_tol="${abs_tol}" -v rel_tol="${rel_tol}" -v file="${current}" '
      BEGIN { n = 0 }
      {
        b = $1 + 0
        c = $2 + 0
        d = b - c
        if (d < 0) d = -d
        rb = b
        if (rb < 0) rb = -rb
        tol = abs_tol
        rt = rel_tol * rb
        if (rt > tol) tol = rt
        n++
        if (d > tol) {
          printf("FAIL: %s token %d differs (baseline=%g current=%g |diff|=%g tol=%g)\n", file, n, b, c, d, tol)
          exit 1
        }
      }
      END {
        if (n == 0) {
          printf("FAIL: %s has no numeric tokens to compare\n", file)
          exit 1
        }
      }'
}

echo "Running regtests..."
"${ROOT}/scripts/run_regtests_with_built_topas.sh" "${TOPAS_BIN}" >/tmp/verify_baseline_regtests.log

echo "Running example smoke..."
"${ROOT}/scripts/run_examples_with_built_topas.sh" "${TOPAS_BIN}" smoke >/tmp/verify_baseline_example.log
EXAMPLE_DIR="$(sed -n 's/^RUN_DIR=//p' /tmp/verify_baseline_example.log | tail -n 1)"
if [[ -z "${EXAMPLE_DIR}" || ! -d "${EXAMPLE_DIR}" ]]; then
  echo "ERROR: Could not find example smoke run output directory."
  exit 1
fi

fail_count=0

while IFS= read -r bf; do
  rel="${bf#${BASE}/regtests/}"
  cf="${ROOT}/AuxFiles/regtests/runs/${rel}"
  if [[ ! -f "${cf}" ]]; then
    echo "FAIL: missing current file ${cf}"
    fail_count=$((fail_count + 1))
    continue
  fi

  if [[ "${bf}" == *"run_metadata.json" ]]; then
    abs="${METADATA_ABS_TOL}"
    reltol="${SCALAR_REL_TOL}"
  elif [[ "${bf}" == *.csv ]]; then
    abs="${ISOTOPE_ABS_TOL}"
    reltol="${SCALAR_REL_TOL}"
  else
    abs="${COMPARTMENT_ABS_TOL}"
    reltol="${SCALAR_REL_TOL}"
  fi

  if ! compare_numeric_files "${bf}" "${cf}" "${abs}" "${reltol}"; then
    fail_count=$((fail_count + 1))
  fi
done < <(find "${BASE}/regtests" -type f \( -name "*.csv" -o -name "prob*.txt" -o -name "run_metadata.json" \) | sort)

if [[ -f "${BASE}/examples/run_metadata.json" ]]; then
  if ! compare_numeric_files "${BASE}/examples/run_metadata.json" "${EXAMPLE_DIR}/run_metadata.json" "${METADATA_ABS_TOL}" "${SCALAR_REL_TOL}"; then
    fail_count=$((fail_count + 1))
  fi
fi

if [[ "${fail_count}" -ne 0 ]]; then
  echo "Parity check failed: ${fail_count} file(s) out of tolerance."
  exit 1
fi

echo "Parity check passed."
