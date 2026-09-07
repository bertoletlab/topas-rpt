#!/usr/bin/env bash
set -euo pipefail

# Scientific showcase for invitro_bind mode: 2-compartment vs 4-compartment kinetics.
# Usage: scripts/run_invitro_scientific_showcase.sh /path/to/topas

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

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
P2="${ROOT}/AuxFiles/regtests/runs/bind_2comp_cell/bind_2comp_cell.txt"
P4="${ROOT}/AuxFiles/regtests/runs/bind_4comp/bind_4comp.txt"
OUT_ROOT="${EXAMPLE_OUT_ROOT:-/tmp/tsrts_examples_runs}"
RUN_DIR="${OUT_ROOT}/$(date +%Y%m%d_%H%M%S)_invitro_scientific_$$"
D2="${RUN_DIR}/model_2comp"
D4="${RUN_DIR}/model_4comp"
mkdir -p "${D2}" "${D4}"

cp "${P2}" "${D2}/run.txt"
cp "${P4}" "${D4}/run.txt"

run_case_or_skip() {
  local case_dir="$1"
  local rc
  (
    cd "${case_dir}" && "${TOPAS_BIN}" run.txt > run.log 2>&1
  )
  rc=$?

  if [[ ${rc} -ne 0 ]]; then
    if grep -Eiq 'unsupported Component Type: TsCellMonolayer|unsupported Component Type: TsCell' "${case_dir}/run.log"; then
      return 10
    fi
    return "${rc}"
  fi
  return 0
}

set +e
run_case_or_skip "${D2}"
rc=$?
set -e
if [[ ${rc} -eq 10 ]]; then
  echo "SKIP: invitro showcase requires optional component TsCellMonolayer in this TOPAS build."
  exit 0
elif [[ ${rc} -ne 0 ]]; then
  exit "${rc}"
fi

set +e
run_case_or_skip "${D4}"
rc=$?
set -e
if [[ ${rc} -eq 10 ]]; then
  echo "SKIP: invitro showcase requires optional component TsCellMonolayer in this TOPAS build."
  exit 0
elif [[ ${rc} -ne 0 ]]; then
  exit "${rc}"
fi

for d in "${D2}" "${D4}"; do
  if grep -Eiq 'TOPAS is exiting due to a serious error|FATAL ERROR|G4Exception: Aborting execution' "${d}/run.log"; then
    echo "ERROR: invitro showcase failed in ${d}"
    exit 1
  fi
done

PROB2="${D2}/prob_2comp_cell.txt"
PROB4="${D4}/prob_4comp.txt"
[[ -f "${PROB2}" ]] || { echo "ERROR: missing ${PROB2}"; exit 1; }
[[ -f "${PROB4}" ]] || { echo "ERROR: missing ${PROB4}"; exit 1; }

FINAL2="${RUN_DIR}/final_2comp.tsv"
FINAL4="${RUN_DIR}/final_4comp.tsv"
awk -F'\t' 'NR==1{for(i=1;i<=NF;i++) h[i]=$i; next} {for(i=1;i<=NF;i++) v[i]=$i} END{for(i=1;i<=NF;i++) printf "%s\t%s\n", h[i], v[i]}' "${PROB2}" > "${FINAL2}"
awk -F'\t' 'NR==1{for(i=1;i<=NF;i++) h[i]=$i; next} {for(i=1;i<=NF;i++) v[i]=$i} END{for(i=1;i<=NF;i++) printf "%s\t%s\n", h[i], v[i]}' "${PROB4}" > "${FINAL4}"

REPORT="${RUN_DIR}/invitro_scientific_report.md"
{
  echo "# InVitro Bind Scientific Showcase"
  echo
  echo "Comparison: 2-compartment-cell vs 4-compartment kinetics"
  echo
  echo "## Final state from probability traces"
  echo
  echo "| Quantity | 2-compartment | 4-compartment |"
  echo "|---|---:|---:|"
  awk -F'\t' '
    NR==FNR {a[$1]=$2; next}
    {b[$1]=$2}
    END {
      keys["Time"]=1; keys["E"]=1; keys["M"]=1; keys["I"]=1; keys["N"]=1; keys["D"]=1; keys["FracRemaining"]=1;
      for (k in keys) {
        av = (k in a) ? a[k] : "NA";
        bv = (k in b) ? b[k] : "NA";
        printf "| %s | %s | %s |\n", k, av, bv;
      }
    }
  ' "${FINAL2}" "${FINAL4}"
  echo
  echo "Interpretation: differences in compartment populations reflect kinetic-model structure, even with identical source isotope/geometry settings."
} > "${REPORT}"

echo "TOPAS_BIN=${TOPAS_BIN}"
echo "RUN_DIR=${RUN_DIR}"
echo "REPORT_FILE=${REPORT}"
