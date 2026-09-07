#!/usr/bin/env bash
set -euo pipefail

# Validates deterministic reproducibility of the in-vitro smoke example.
#
# Usage:
#   tests/regtests/validate_invitro_smoke_determinism.sh /path/to/topas

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

set +e
"${ROOT}/scripts/run_examples_with_built_topas.sh" "${TOPAS_BIN}" smoke >/tmp/validate_invitro_smoke_run1.log 2>&1
RC1=$?
set -e
if [[ ${RC1} -ne 0 ]]; then
  cat /tmp/validate_invitro_smoke_run1.log
  echo "ERROR: first in-vitro smoke run failed."
  exit 1
fi
RUN1="$(sed -n 's/^RUN_DIR=//p' /tmp/validate_invitro_smoke_run1.log | tail -n 1)"
if [[ -z "${RUN1}" || ! -d "${RUN1}" ]]; then
  echo "ERROR: could not resolve first in-vitro smoke run directory."
  exit 1
fi

set +e
"${ROOT}/scripts/run_examples_with_built_topas.sh" "${TOPAS_BIN}" smoke >/tmp/validate_invitro_smoke_run2.log 2>&1
RC2=$?
set -e
if [[ ${RC2} -ne 0 ]]; then
  cat /tmp/validate_invitro_smoke_run2.log
  echo "ERROR: second in-vitro smoke run failed."
  exit 1
fi
RUN2="$(sed -n 's/^RUN_DIR=//p' /tmp/validate_invitro_smoke_run2.log | tail -n 1)"
if [[ -z "${RUN2}" || ! -d "${RUN2}" ]]; then
  echo "ERROR: could not resolve second in-vitro smoke run directory."
  exit 1
fi

ARTIFACTS=(
  "run_metadata.json"
  "targeted_PDAC_isotopic_abundance.csv"
  "OutputFileName.header"
)

for f in "${ARTIFACTS[@]}"; do
  [[ -f "${RUN1}/${f}" ]] || { echo "ERROR: missing expected output ${RUN1}/${f}"; exit 1; }
  [[ -f "${RUN2}/${f}" ]] || { echo "ERROR: missing expected output ${RUN2}/${f}"; exit 1; }
done

for f in "${ARTIFACTS[@]}"; do
  SHA1="$(shasum -a 256 "${RUN1}/${f}" | awk '{print $1}')"
  SHA2="$(shasum -a 256 "${RUN2}/${f}" | awk '{print $1}')"
  if [[ "${SHA1}" != "${SHA2}" ]]; then
    echo "ERROR: deterministic mismatch for ${f}"
    echo "  RUN1=${RUN1}/${f}"
    echo "  RUN2=${RUN2}/${f}"
    echo "  SHA1=${SHA1}"
    echo "  SHA2=${SHA2}"
    exit 1
  fi
done

echo "PASS: invitro smoke determinism (${RUN1} vs ${RUN2})"
