#!/usr/bin/env bash
set -euo pipefail

# Runs bundled regtests with a user-provided TOPAS binary.
#
# Usage:
#   scripts/run_regtests_with_built_topas.sh /path/to/topas
#
# Optional env:
#   TOPAS_G4_DATA_DIR (legacy fallback when Geant4 data vars are not already set)

if [[ $# -ne 1 ]]; then
  echo "Usage: $0 /path/to/topas"
  exit 2
fi

TOPAS_BIN="$1"
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

echo "TOPAS_BIN=${TOPAS_BIN}"
echo "TOPAS_G4_DATA_DIR=${TOPAS_G4_DATA_DIR:-<unset>}"

TOPAS_BIN="${TOPAS_BIN}" bash "${REPO_ROOT}/AuxFiles/regtests/run_tests.sh"
