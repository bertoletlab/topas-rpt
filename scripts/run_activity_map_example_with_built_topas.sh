#!/usr/bin/env bash
set -euo pipefail

# Runs activity_map example with a user-provided TOPAS binary.
#
# Usage:
#   scripts/run_activity_map_example_with_built_topas.sh /path/to/topas [smoke]
#
# Required env:
#   ACTIVITYMAP_CT_DICOM_DIR         Path to CT DICOM directory (for TsDicomPatient parent).
#   ACTIVITYMAP_NM_DICOM_DIR         Path to NM/PT DICOM directory (for TsDicomActivityMap).
#
# Optional env:
#   ACTIVITYMAP_HU_INCLUDE           Path to HU conversion include file.
#                                    Default: /Applications/TOPAS/OpenTOPAS/examples/Patient/HUtoMaterialSchneider.txt
#   TOPAS_G4_DATA_DIR               Geant4 data root.
#   EXAMPLE_OUT_ROOT                Output root directory.

if [[ $# -lt 1 || $# -gt 2 ]]; then
  echo "Usage: $0 /path/to/topas [smoke]"
  exit 2
fi

TOPAS_BIN="$1"
MODE="${2:-smoke}"
if [[ "${MODE}" != "smoke" ]]; then
  echo "ERROR: only 'smoke' mode is currently supported"
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

if [[ -z "${ACTIVITYMAP_CT_DICOM_DIR:-}" || -z "${ACTIVITYMAP_NM_DICOM_DIR:-}" ]]; then
  echo "SKIP: activity_map example requires ACTIVITYMAP_CT_DICOM_DIR and ACTIVITYMAP_NM_DICOM_DIR."
  exit 0
fi

if [[ ! -d "${ACTIVITYMAP_CT_DICOM_DIR}" ]]; then
  echo "ERROR: ACTIVITYMAP_CT_DICOM_DIR does not exist: ${ACTIVITYMAP_CT_DICOM_DIR}"
  exit 2
fi

if [[ ! -d "${ACTIVITYMAP_NM_DICOM_DIR}" ]]; then
  echo "ERROR: ACTIVITYMAP_NM_DICOM_DIR does not exist: ${ACTIVITYMAP_NM_DICOM_DIR}"
  exit 2
fi

DEFAULT_HU_INCLUDE="/Applications/TOPAS/OpenTOPAS/examples/Patient/HUtoMaterialSchneider.txt"
ACTIVITYMAP_HU_INCLUDE="${ACTIVITYMAP_HU_INCLUDE:-${DEFAULT_HU_INCLUDE}}"
if [[ ! -f "${ACTIVITYMAP_HU_INCLUDE}" ]]; then
  echo "ERROR: HU include file not found: ${ACTIVITYMAP_HU_INCLUDE}"
  exit 2
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
EXAMPLE_DIR="${REPO_ROOT}/examples"
OUT_ROOT="${EXAMPLE_OUT_ROOT:-/tmp/tsrts_examples_runs}"
RUN_DIR="${OUT_ROOT}/$(date +%Y%m%d_%H%M%S)_activity_map_${MODE}_$$"
mkdir -p "${RUN_DIR}"

echo "TOPAS_BIN=${TOPAS_BIN}"
echo "TOPAS_G4_DATA_DIR=${TOPAS_G4_DATA_DIR:-<unset>}"
echo "RUN_DIR=${RUN_DIR}"

TEMPLATE_FILE="${EXAMPLE_DIR}/exampleActivityMap.template.txt"
if [[ ! -f "${TEMPLATE_FILE}" ]]; then
  echo "ERROR: template file not found: ${TEMPLATE_FILE}"
  exit 2
fi

PARAM_FILE="${RUN_DIR}/exampleActivityMap_${MODE}.txt"
sed \
  -e "s|{{HU_TO_MATERIAL_INCLUDE}}|${ACTIVITYMAP_HU_INCLUDE}|g" \
  -e "s|{{CT_DICOM_DIR}}|${ACTIVITYMAP_CT_DICOM_DIR}|g" \
  -e "s|{{ACTIVITYMAP_DICOM_DIR}}|${ACTIVITYMAP_NM_DICOM_DIR}|g" \
  "${TEMPLATE_FILE}" > "${PARAM_FILE}"

# Smoke runs often use heavily restricted voxel windows for speed.
# Disable strict parent-grid matching in this script mode to avoid
# false fatals from intentionally coarse/restricted smoke setups.
sed -i.bak \
  -e 's/b:So\/MapSource\/ModeParams\/RequireParentGridMatch[[:space:]]*=.*/b:So\/MapSource\/ModeParams\/RequireParentGridMatch = "False"/' \
  "${PARAM_FILE}"
rm -f "${PARAM_FILE}.bak"

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
echo "PASS: activity_map (${MODE})"
