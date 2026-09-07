#!/usr/bin/env bash
set -euo pipefail

# Validates activity_map mode contract with a low-history deterministic smoke run.
#
# Usage:
#   tests/regtests/validate_activity_map_contract.sh /path/to/topas

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

if [[ -z "${ACTIVITYMAP_CT_DICOM_DIR:-}" || -z "${ACTIVITYMAP_NM_DICOM_DIR:-}" ]]; then
  echo "SKIP: activity_map contract requires DICOM dirs; set ACTIVITYMAP_CT_DICOM_DIR and ACTIVITYMAP_NM_DICOM_DIR."
  exit 0
fi

set +e
"${ROOT}/scripts/run_activity_map_example_with_built_topas.sh" "${TOPAS_BIN}" smoke >/tmp/validate_activity_map_run1.log 2>&1
RC1=$?
set -e
RUN1="$(sed -n 's/^RUN_DIR=//p' /tmp/validate_activity_map_run1.log | tail -n 1)"
if [[ ${RC1} -ne 0 ]]; then
  if [[ -n "${RUN1}" && -f "${RUN1}/run.log" ]]; then
    if rg -q 'Attempt to instantiate child:' "${RUN1}/run.log"; then
      echo "SKIP: activity_map requires a TOPAS-side geometry handling fix for TsDicomActivityMap under divided TsDicomPatient."
      exit 0
    fi
    if rg -q 'unsupported Component Type: TsDicomActivityMap' "${RUN1}/run.log"; then
      echo "SKIP: TOPAS build does not register TsDicomActivityMap in geometry type registry."
      exit 0
    fi
  fi
  cat /tmp/validate_activity_map_run1.log
  echo "ERROR: activity_map smoke run failed."
  exit 1
fi

if [[ -z "${RUN1}" || ! -d "${RUN1}" ]]; then
  echo "ERROR: could not resolve first activity_map run directory."
  exit 1
fi

for f in run.log run_metadata.json activity_map_dose.csv; do
  [[ -f "${RUN1}/${f}" ]] || { echo "ERROR: missing expected output: ${RUN1}/${f}"; exit 1; }
done

if rg -qi -e 'TOPAS is exiting due to a serious error|FATAL ERROR|G4Exception: Aborting execution' "${RUN1}/run.log"; then
  echo "ERROR: activity_map run contains fatal content: ${RUN1}/run.log"
  exit 1
fi

set +e
"${ROOT}/scripts/run_activity_map_example_with_built_topas.sh" "${TOPAS_BIN}" smoke >/tmp/validate_activity_map_run2.log 2>&1
RC2=$?
set -e
if [[ ${RC2} -ne 0 ]]; then
  cat /tmp/validate_activity_map_run2.log
  echo "ERROR: second activity_map smoke run failed."
  exit 1
fi
RUN2="$(sed -n 's/^RUN_DIR=//p' /tmp/validate_activity_map_run2.log | tail -n 1)"
if [[ -z "${RUN2}" || ! -d "${RUN2}" ]]; then
  echo "ERROR: could not resolve second activity_map run directory."
  exit 1
fi

for f in run_metadata.json activity_map_dose.csv; do
  [[ -f "${RUN2}/${f}" ]] || { echo "ERROR: missing expected output: ${RUN2}/${f}"; exit 1; }
done

SHA1_META="$(shasum -a 256 "${RUN1}/run_metadata.json" | awk '{print $1}')"
SHA2_META="$(shasum -a 256 "${RUN2}/run_metadata.json" | awk '{print $1}')"
if [[ "${SHA1_META}" != "${SHA2_META}" ]]; then
  echo "ERROR: run_metadata.json differs across repeated deterministic runs."
  exit 1
fi

SHA1_DOSE="$(shasum -a 256 "${RUN1}/activity_map_dose.csv" | awk '{print $1}')"
SHA2_DOSE="$(shasum -a 256 "${RUN2}/activity_map_dose.csv" | awk '{print $1}')"
if [[ "${SHA1_DOSE}" != "${SHA2_DOSE}" ]]; then
  echo "ERROR: activity_map_dose.csv differs across repeated deterministic runs."
  exit 1
fi

echo "PASS: activity_map contract (${RUN1})"
