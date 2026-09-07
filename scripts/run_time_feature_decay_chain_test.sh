#!/usr/bin/env bash
set -euo pipefail

# Runs the time-feature + decay-chain control example.
#
# Usage:
#   scripts/run_time_feature_decay_chain_test.sh /path/to/topas
#   scripts/run_time_feature_decay_chain_test.sh topas

if [[ $# -ne 1 ]]; then
  echo "Usage: $0 /path/to/topas"
  exit 2
fi

TOPAS_BIN="$1"
if ! command -v "${TOPAS_BIN}" >/dev/null 2>&1 && [[ ! -x "${TOPAS_BIN}" ]]; then
  echo "ERROR: TOPAS binary/command not found: ${TOPAS_BIN}"
  exit 2
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

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
EXAMPLE_FILE="${REPO_ROOT}/examples/exampleTimeFeatureDecayChainControl.txt"

if [[ ! -f "${EXAMPLE_FILE}" ]]; then
  echo "ERROR: missing example file: ${EXAMPLE_FILE}"
  exit 2
fi

OUT_ROOT="${EXAMPLE_OUT_ROOT:-/tmp/tsrts_examples_runs}"
RUN_DIR="${OUT_ROOT}/$(date +%Y%m%d_%H%M%S)_time_chain_control"
RUN_ON_DIR="${RUN_DIR}/chain_on"
RUN_OFF_DIR="${RUN_DIR}/chain_off"
mkdir -p "${RUN_ON_DIR}" "${RUN_OFF_DIR}"

echo "TOPAS_BIN=${TOPAS_BIN}"
echo "TOPAS_G4_DATA_DIR=${TOPAS_G4_DATA_DIR:-<unset>}"
echo "RUN_DIR=${RUN_DIR}"

cp "${EXAMPLE_FILE}" "${RUN_ON_DIR}/exampleTimeFeatureDecayChainControl.txt"
(
  cd "${RUN_ON_DIR}"
  "${TOPAS_BIN}" exampleTimeFeatureDecayChainControl.txt > run.log 2>&1
)
if grep -Eiq 'TOPAS is exiting due to a serious error|FATAL ERROR|G4Exception: Aborting execution' "${RUN_ON_DIR}/run.log"; then
  echo "ERROR: chain_on run completed with fatal content in log: ${RUN_ON_DIR}/run.log"
  exit 1
fi

cp "${EXAMPLE_FILE}" "${RUN_OFF_DIR}/exampleTimeFeatureDecayChainControl.txt"
sed -i.bak \
  -e 's/b:So\/ChainSource\/IncludeWholeDecayChain[[:space:]]*=.*/b:So\/ChainSource\/IncludeWholeDecayChain = "False"/' \
  -e 's/s:So\/ChainSource\/WriteIsotopicAbundanceFileName[[:space:]]*=.*/s:So\/ChainSource\/WriteIsotopicAbundanceFileName = "iso_chain_off.csv"/' \
  -e 's/s:So\/ChainSource\/RunMetadataFileName[[:space:]]*=.*/s:So\/ChainSource\/RunMetadataFileName = "run_metadata_chain_off.json"/' \
  "${RUN_OFF_DIR}/exampleTimeFeatureDecayChainControl.txt"
rm -f "${RUN_OFF_DIR}/exampleTimeFeatureDecayChainControl.txt.bak"
(
  cd "${RUN_OFF_DIR}"
  "${TOPAS_BIN}" exampleTimeFeatureDecayChainControl.txt > run.log 2>&1
)
if grep -Eiq 'TOPAS is exiting due to a serious error|FATAL ERROR|G4Exception: Aborting execution' "${RUN_OFF_DIR}/run.log"; then
  echo "ERROR: chain_off run completed with fatal content in log: ${RUN_OFF_DIR}/run.log"
  exit 1
fi

echo
echo "Run complete. Key outputs:"
ls -1 "${RUN_DIR}" | sed -n '1,120p'
echo "Chain-on log: ${RUN_ON_DIR}/run.log"
echo "Chain-off log: ${RUN_OFF_DIR}/run.log"
echo "PASS: time-feature decay-chain control example"
