#!/usr/bin/env bash
set -euo pipefail

# Validates the time-feature + decay-chain control example contract.
#
# Usage:
#   tests/regtests/validate_time_feature_decay_chain_control.sh /path/to/topas
#   tests/regtests/validate_time_feature_decay_chain_control.sh topas

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

"${ROOT}/scripts/run_time_feature_decay_chain_test.sh" "${TOPAS_BIN}" >/tmp/validate_time_chain_control.log

RUN_DIR="$(sed -n 's/^RUN_DIR=//p' /tmp/validate_time_chain_control.log | tail -n 1)"
RUN_ON_DIR="${RUN_DIR}/chain_on"
RUN_OFF_DIR="${RUN_DIR}/chain_off"
if [[ -z "${RUN_DIR}" || ! -d "${RUN_ON_DIR}" || ! -d "${RUN_OFF_DIR}" ]]; then
  echo "ERROR: could not resolve run directory."
  exit 1
fi

for f in run_metadata_chain.json iso_chain.csv run.log; do
  [[ -f "${RUN_ON_DIR}/${f}" ]] || { echo "ERROR: missing expected output: ${RUN_ON_DIR}/${f}"; exit 1; }
done
for f in run_metadata_chain_off.json iso_chain_off.csv run.log; do
  [[ -f "${RUN_OFF_DIR}/${f}" ]] || { echo "ERROR: missing expected output: ${RUN_OFF_DIR}/${f}"; exit 1; }
done

if command -v jq >/dev/null 2>&1; then
  jq -e '.schema_version == "tsrts.run_metadata.v1" and .mode == "uniform" and (.entries | length == 4)' \
    "${RUN_ON_DIR}/run_metadata_chain.json" >/dev/null
  jq -e '.schema_version == "tsrts.run_metadata.v1" and .mode == "uniform" and (.entries | length == 4)' \
    "${RUN_OFF_DIR}/run_metadata_chain_off.json" >/dev/null
else
  rg -q '"schema_version"[[:space:]]*:[[:space:]]*"tsrts.run_metadata.v1"' "${RUN_ON_DIR}/run_metadata_chain.json"
  rg -q '"schema_version"[[:space:]]*:[[:space:]]*"tsrts.run_metadata.v1"' "${RUN_OFF_DIR}/run_metadata_chain_off.json"
  rg -q '"mode"[[:space:]]*:[[:space:]]*"uniform"' "${RUN_ON_DIR}/run_metadata_chain.json"
  rg -q '"mode"[[:space:]]*:[[:space:]]*"uniform"' "${RUN_OFF_DIR}/run_metadata_chain_off.json"
fi

# 4 sequential times => header + 4 rows in isotopic files.
if [[ "$(wc -l < "${RUN_ON_DIR}/iso_chain.csv")" -lt 5 ]]; then
  echo "ERROR: isotopic abundance file appears incomplete: ${RUN_ON_DIR}/iso_chain.csv"
  exit 1
fi
if [[ "$(wc -l < "${RUN_OFF_DIR}/iso_chain_off.csv")" -lt 5 ]]; then
  echo "ERROR: isotopic abundance file appears incomplete: ${RUN_OFF_DIR}/iso_chain_off.csv"
  exit 1
fi

# Geant4 >= 11.3 compatibility behavior should be explicit only on those versions.
if command -v /Applications/GEANT4/geant4-install/bin/geant4-config >/dev/null 2>&1; then
  G4_VERSION="$(/Applications/GEANT4/geant4-install/bin/geant4-config --version)"
else
  G4_VERSION="unknown"
fi

if [[ "${G4_VERSION}" == 11.3* || "${G4_VERSION}" == 11.4* || "${G4_VERSION}" == 12* ]]; then
  rg -q 'Recursive daughter-chain sampling is disabled for Geant4 >= 11.3' "${RUN_ON_DIR}/run.log"
  if rg -q 'Recursive daughter-chain sampling is disabled for Geant4 >= 11.3' "${RUN_OFF_DIR}/run.log"; then
    echo "ERROR: chain-off run unexpectedly emitted recursive-chain warning."
    exit 1
  fi
else
  if rg -q 'Recursive daughter-chain sampling is disabled for Geant4 >= 11.3' "${RUN_ON_DIR}/run.log"; then
    echo "ERROR: Geant4 ${G4_VERSION} run emitted unexpected 11.3 compatibility warning."
    exit 1
  fi
fi

echo "PASS: time-feature decay-chain control contract (${RUN_DIR})"
