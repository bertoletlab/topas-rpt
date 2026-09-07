#!/usr/bin/env bash
set -euo pipefail

# Runs repository example parameter sets with a user-provided TOPAS binary.
#
# Usage:
#   scripts/run_examples_with_built_topas.sh /path/to/topas [smoke|full]
#
# Modes:
#   smoke (default): reduced histories/timeline for quick validation
#   full:            runs examples as-is
#
# Optional env:
#   TOPAS_G4_DATA_DIR (legacy fallback when Geant4 data vars are not already set)
#   EXAMPLE_OUT_ROOT (defaults to /tmp/tsrts_examples_runs)

if [[ $# -lt 1 || $# -gt 2 ]]; then
  echo "Usage: $0 /path/to/topas [smoke|full]"
  exit 2
fi

TOPAS_BIN="$1"
MODE="${2:-smoke}"
if [[ "${MODE}" != "smoke" && "${MODE}" != "full" ]]; then
  echo "ERROR: mode must be 'smoke' or 'full'"
  exit 2
fi

if [[ ! -x "${TOPAS_BIN}" ]]; then
  echo "ERROR: TOPAS binary not executable: ${TOPAS_BIN}"
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
EXAMPLE_DIR="${REPO_ROOT}/examples"
OUT_ROOT="${EXAMPLE_OUT_ROOT:-/tmp/tsrts_examples_runs}"
RUN_DIR="${OUT_ROOT}/$(date +%Y%m%d_%H%M%S)_${MODE}_$$"
mkdir -p "${RUN_DIR}"

echo "TOPAS_BIN=${TOPAS_BIN}"
echo "TOPAS_G4_DATA_DIR=${TOPAS_G4_DATA_DIR:-<unset>}"
echo "MODE=${MODE}"
echo "RUN_DIR=${RUN_DIR}"

EXAMPLE_FILE="${EXAMPLE_DIR}/exampleInVitroRPT.txt"
if [[ ! -f "${EXAMPLE_FILE}" ]]; then
  echo "ERROR: example file not found: ${EXAMPLE_FILE}"
  exit 2
fi

PARAM_FILE="${RUN_DIR}/exampleInVitroRPT_${MODE}.txt"
cp "${EXAMPLE_FILE}" "${PARAM_FILE}"

if [[ "${MODE}" == "smoke" ]]; then
  # Keep physics and model the same; reduce runtime knobs only.
  sed -i.bak \
    -e 's/i:So\/212PbSource\/NumberOfHistoriesInRun[[:space:]]*=.*/i:So\/212PbSource\/NumberOfHistoriesInRun = 200/' \
    -e 's/d:Tf\/TimelineEnd[[:space:]]*=.*/d:Tf\/TimelineEnd = 2 * hour s/' \
    -e 's/i:Tf\/NumberOfSequentialTimes[[:space:]]*=.*/i:Tf\/NumberOfSequentialTimes = 2/' \
    "${PARAM_FILE}"
  rm -f "${PARAM_FILE}.bak"
fi

(
  cd "${RUN_DIR}"
  "${TOPAS_BIN}" "$(basename "${PARAM_FILE}")" > run.log 2>&1
)

echo
echo "Run complete. Key outputs:"
ls -1 "${RUN_DIR}" | sed -n '1,120p'

if grep -Eiq 'TOPAS is exiting due to a serious error|FATAL ERROR|G4Exception: Aborting execution' "${RUN_DIR}/run.log"; then
  echo "ERROR: run completed with fatal content in log: ${RUN_DIR}/run.log"
  exit 1
fi

echo "Log: ${RUN_DIR}/run.log"
echo "PASS: exampleInVitroRPT (${MODE})"
