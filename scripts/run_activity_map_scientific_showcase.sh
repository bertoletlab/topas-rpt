#!/usr/bin/env bash
set -euo pipefail

# Scientific showcase for activity_map mode: calibrated OFF vs ON.
# Usage: scripts/run_activity_map_scientific_showcase.sh /path/to/topas

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

if [[ -z "${ACTIVITYMAP_CT_DICOM_DIR:-}" || -z "${ACTIVITYMAP_NM_DICOM_DIR:-}" ]]; then
  echo "SKIP: activity_map scientific showcase requires ACTIVITYMAP_CT_DICOM_DIR and ACTIVITYMAP_NM_DICOM_DIR."
  exit 0
fi

DEFAULT_HU_INCLUDE="/Applications/TOPAS/OpenTOPAS/examples/Patient/HUtoMaterialSchneider.txt"
ACTIVITYMAP_HU_INCLUDE="${ACTIVITYMAP_HU_INCLUDE:-${DEFAULT_HU_INCLUDE}}"
if [[ ! -f "${ACTIVITYMAP_HU_INCLUDE}" ]]; then
  echo "ERROR: HU include file not found."
  exit 2
fi

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TEMPLATE="${ROOT}/examples/exampleActivityMap.template.txt"
OUT_ROOT="${EXAMPLE_OUT_ROOT:-/tmp/tsrts_examples_runs}"
RUN_DIR="${OUT_ROOT}/$(date +%Y%m%d_%H%M%S)_activity_map_scientific_$$"
OFF_DIR="${RUN_DIR}/uncalibrated"
ON_DIR="${RUN_DIR}/calibrated"
mkdir -p "${OFF_DIR}" "${ON_DIR}"

make_param() {
  local out="$1"
  sed \
    -e "s|{{HU_TO_MATERIAL_INCLUDE}}|${ACTIVITYMAP_HU_INCLUDE}|g" \
    -e "s|{{CT_DICOM_DIR}}|${ACTIVITYMAP_CT_DICOM_DIR}|g" \
    -e "s|{{ACTIVITYMAP_DICOM_DIR}}|${ACTIVITYMAP_NM_DICOM_DIR}|g" \
    "${TEMPLATE}" > "${out}"

  sed -i.bak \
    -e 's/b:So\/MapSource\/ModeParams\/RequireParentGridMatch[[:space:]]*=.*/b:So\/MapSource\/ModeParams\/RequireParentGridMatch = "False"/' \
    -e 's/i:So\/MapSource\/NumberOfHistoriesInRun[[:space:]]*=.*/i:So\/MapSource\/NumberOfHistoriesInRun = 40/' \
    "${out}"
  rm -f "${out}.bak"
}

make_param "${OFF_DIR}/run.txt"
make_param "${ON_DIR}/run.txt"

sed -i.bak \
  -e 's/b:So\/MapSource\/ModeParams\/UseCalibratedCounts[[:space:]]*=.*/b:So\/MapSource\/ModeParams\/UseCalibratedCounts = "False"/' \
  -e 's/s:So\/MapSource\/RunMetadataFileName[[:space:]]*=.*/s:So\/MapSource\/RunMetadataFileName = "run_metadata_uncalibrated.json"/' \
  -e 's/s:So\/MapSource\/ModeParams\/ActivityMapSummaryFileName[[:space:]]*=.*/s:So\/MapSource\/ModeParams\/ActivityMapSummaryFileName = "activity_map_summary_uncalibrated.json"/' \
  "${OFF_DIR}/run.txt"
rm -f "${OFF_DIR}/run.txt.bak"

sed -i.bak \
  -e 's/b:So\/MapSource\/ModeParams\/UseCalibratedCounts[[:space:]]*=.*/b:So\/MapSource\/ModeParams\/UseCalibratedCounts = "True"/' \
  -e 's/s:So\/MapSource\/ModeParams\/CalibratedCountUnits[[:space:]]*=.*/s:So\/MapSource\/ModeParams\/CalibratedCountUnits = "BqPerMl"/' \
  -e 's/u:So\/MapSource\/ModeParams\/CalibrationScaleBqPerCount[[:space:]]*=.*/u:So\/MapSource\/ModeParams\/CalibrationScaleBqPerCount = 1.0/' \
  -e 's/s:So\/MapSource\/RunMetadataFileName[[:space:]]*=.*/s:So\/MapSource\/RunMetadataFileName = "run_metadata_calibrated.json"/' \
  -e 's/s:So\/MapSource\/ModeParams\/ActivityMapSummaryFileName[[:space:]]*=.*/s:So\/MapSource\/ModeParams\/ActivityMapSummaryFileName = "activity_map_summary_calibrated.json"/' \
  "${ON_DIR}/run.txt"
rm -f "${ON_DIR}/run.txt.bak"

set +e
(
  cd "${OFF_DIR}" && "${TOPAS_BIN}" run.txt > run.log 2>&1
)
RC_OFF=$?
(
  cd "${ON_DIR}" && "${TOPAS_BIN}" run.txt > run.log 2>&1
)
RC_ON=$?
set -e

if [[ ${RC_OFF} -ne 0 || ${RC_ON} -ne 0 ]]; then
  if [[ -f "${OFF_DIR}/run.log" ]] && rg -q 'unsupported Component Type: TsDicomActivityMap' "${OFF_DIR}/run.log"; then
    echo "SKIP: TOPAS build does not register TsDicomActivityMap in geometry type registry."
    exit 0
  fi
  if [[ -f "${OFF_DIR}/run.log" ]] && rg -q 'Attempt to instantiate child:' "${OFF_DIR}/run.log"; then
    echo "SKIP: activity_map requires TOPAS-side geometry fix for TsDicomActivityMap under divided TsDicomPatient."
    exit 0
  fi
  if [[ -f "${ON_DIR}/run.log" ]] && rg -q 'unsupported Component Type: TsDicomActivityMap' "${ON_DIR}/run.log"; then
    echo "SKIP: TOPAS build does not register TsDicomActivityMap in geometry type registry."
    exit 0
  fi
  if [[ -f "${ON_DIR}/run.log" ]] && rg -q 'Attempt to instantiate child:' "${ON_DIR}/run.log"; then
    echo "SKIP: activity_map requires TOPAS-side geometry fix for TsDicomActivityMap under divided TsDicomPatient."
    exit 0
  fi
  echo "ERROR: activity_map showcase run failed."
  exit 1
fi

for d in "${OFF_DIR}" "${ON_DIR}"; do
  if grep -Eiq 'TOPAS is exiting due to a serious error|FATAL ERROR|G4Exception: Aborting execution' "${d}/run.log"; then
    echo "ERROR: activity_map showcase failed in ${d}"
    exit 1
  fi
done

OFF_META="${OFF_DIR}/run_metadata_uncalibrated.json"
ON_META="${ON_DIR}/run_metadata_calibrated.json"

if [[ ! -f "${OFF_META}" || ! -f "${ON_META}" ]]; then
  echo "SKIP: activity_map metadata outputs not produced by this TOPAS build/configuration."
  exit 0
fi

read_json_num() {
  local file="$1"
  local expr="$2"
  if command -v jq >/dev/null 2>&1; then
    jq -r "${expr}" "${file}"
  else
    echo "NA"
  fi
}

OFF_A="$(read_json_num "${OFF_META}" '.entries[0].initial_activity_bq')"
ON_A="$(read_json_num "${ON_META}" '.entries[0].initial_activity_bq')"
OFF_W="$(read_json_num "${OFF_META}" '.entries[0].primary_weight')"
ON_W="$(read_json_num "${ON_META}" '.entries[0].primary_weight')"

REPORT="${RUN_DIR}/activity_map_scientific_report.md"
{
  echo "# Activity Map Scientific Showcase"
  echo
  echo "Comparison: UseCalibratedCounts OFF vs ON"
  echo
  echo "| Metric | Uncalibrated | Calibrated |"
  echo "|---|---:|---:|"
  echo "| initial_activity_bq (step 0) | ${OFF_A} | ${ON_A} |"
  echo "| primary_weight (step 0) | ${OFF_W} | ${ON_W} |"
  echo
  echo "Interpretation: calibration mode changes effective source normalization by deriving initial activity from map values and voxel volume."
} > "${REPORT}"

echo "TOPAS_BIN=${TOPAS_BIN}"
echo "RUN_DIR=${RUN_DIR}"
echo "REPORT_FILE=${REPORT}"
