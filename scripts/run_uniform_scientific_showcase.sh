#!/usr/bin/env bash
set -euo pipefail

# Scientific showcase for uniform (parent) mode: chain ON vs chain OFF.
# Usage: scripts/run_uniform_scientific_showcase.sh /path/to/topas

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
BASE_EXAMPLE="${ROOT}/examples/exampleTimeFeatureDecayChainControl.txt"
OUT_ROOT="${EXAMPLE_OUT_ROOT:-/tmp/tsrts_examples_runs}"
RUN_DIR="${OUT_ROOT}/$(date +%Y%m%d_%H%M%S)_uniform_scientific_$$"
ON_DIR="${RUN_DIR}/chain_on"
OFF_DIR="${RUN_DIR}/chain_off"
mkdir -p "${ON_DIR}" "${OFF_DIR}"

cp "${BASE_EXAMPLE}" "${ON_DIR}/run.txt"
cp "${BASE_EXAMPLE}" "${OFF_DIR}/run.txt"

sed -i.bak \
  -e 's/b:So\/ChainSource\/IncludeWholeDecayChain[[:space:]]*=.*/b:So\/ChainSource\/IncludeWholeDecayChain = "False"/' \
  -e 's/s:So\/ChainSource\/WriteIsotopicAbundanceFileName[[:space:]]*=.*/s:So\/ChainSource\/WriteIsotopicAbundanceFileName = "iso_chain_off.csv"/' \
  -e 's/s:So\/ChainSource\/RunMetadataFileName[[:space:]]*=.*/s:So\/ChainSource\/RunMetadataFileName = "run_metadata_chain_off.json"/' \
  "${OFF_DIR}/run.txt"
rm -f "${OFF_DIR}/run.txt.bak"

(
  cd "${ON_DIR}" && "${TOPAS_BIN}" run.txt > run.log 2>&1
)
(
  cd "${OFF_DIR}" && "${TOPAS_BIN}" run.txt > run.log 2>&1
)

for d in "${ON_DIR}" "${OFF_DIR}"; do
  if grep -Eiq 'TOPAS is exiting due to a serious error|FATAL ERROR|G4Exception: Aborting execution' "${d}/run.log"; then
    echo "ERROR: uniform showcase failed in ${d}"
    exit 1
  fi
done

ON_ISO="${ON_DIR}/iso_chain.csv"
OFF_ISO="${OFF_DIR}/iso_chain_off.csv"
[[ -f "${ON_ISO}" ]] || { echo "ERROR: missing ${ON_ISO}"; exit 1; }
[[ -f "${OFF_ISO}" ]] || { echo "ERROR: missing ${OFF_ISO}"; exit 1; }

DIFF_TSV="${RUN_DIR}/uniform_chain_diff.tsv"
awk -F',' '
  function process_file(file, arr,   line, n, i) {
    while ((getline line < file) > 0) {
      if (line ~ /^Time,/ ) {
        n = split(line, h, ",");
        for (i = 2; i <= n - 1; i++) names[i] = h[i];
        continue;
      }
      last = line;
    }
    close(file);
    n = split(last, v, ",");
    for (i = 2; i <= n - 1; i++) arr[names[i]] = v[i] + 0;
  }
  BEGIN {
    process_file(ARGV[1], on);
    process_file(ARGV[2], off);
    delete ARGV[1]; delete ARGV[2];
    maxd = 0;
    for (k in on) keys[k] = 1;
    for (k in off) keys[k] = 1;
    for (k in keys) {
      a = (k in on) ? on[k] : 0;
      b = (k in off) ? off[k] : 0;
      d = a - b;
      ad = d < 0 ? -d : d;
      if (ad > maxd) maxd = ad;
      printf "%s\t%.12g\t%.12g\t%.12g\n", k, a, b, ad;
    }
    printf "MAX_ABS_DIFF\t%.12g\n", maxd;
  }
' "${ON_ISO}" "${OFF_ISO}" > "${DIFF_TSV}"

MAX_DIFF="$(awk -F'\t' '$1=="MAX_ABS_DIFF"{print $2}' "${DIFF_TSV}")"
: "${MAX_DIFF:=0}"

ON_META="${ON_DIR}/run_metadata_chain.json"
OFF_META="${OFF_DIR}/run_metadata_chain_off.json"
ON_W="$(jq -r '.entries[0].primary_weight' "${ON_META}" 2>/dev/null || echo "NA")"
OFF_W="$(jq -r '.entries[0].primary_weight' "${OFF_META}" 2>/dev/null || echo "NA")"

REPORT="${RUN_DIR}/uniform_scientific_report.md"
{
  echo "# Uniform Mode Scientific Showcase"
  echo
  echo "Comparison: IncludeWholeDecayChain ON vs OFF"
  echo
  echo "- Primary weight (first step), chain ON: ${ON_W}"
  echo "- Primary weight (first step), chain OFF: ${OFF_W}"
  echo "- Max abs final isotope-fraction difference: ${MAX_DIFF}"
  echo
  echo "| Isotope | Chain ON | Chain OFF | Abs diff |"
  echo "|---|---:|---:|---:|"
  awk -F'\t' '$1!="MAX_ABS_DIFF"{printf("| %s | %s | %s | %s |\n",$1,$2,$3,$4)}' "${DIFF_TSV}"
} > "${REPORT}"

echo "TOPAS_BIN=${TOPAS_BIN}"
echo "RUN_DIR=${RUN_DIR}"
echo "REPORT_FILE=${REPORT}"
echo "MAX_ABS_DIFF=${MAX_DIFF}"
