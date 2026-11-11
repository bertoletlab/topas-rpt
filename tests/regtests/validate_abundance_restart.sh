#!/usr/bin/env bash
set -euo pipefail

# Restarting a decay chain from an isotopic-abundance file (ReadIsotopicAbundanceFromFile).
# The abundances in the file are fractions of the atoms at the original time 0, so the restart
# needs no change of InitialActivity, with or without UseFractionOfActivityFromFile. Runs Pb-212
# over 0-20 h (four 5 h steps), then restarts at 10 h from its abundance file with the flag off
# and on, and checks that
#   - the first-step decays of both restarts equal those of the uninterrupted run at 10 h,
#   - the restarted FractionOfDecaysAtTime0 column has one value per row: starting at 1 with the
#     flag off (it was shifted by one row, ending in 0), and continuing from the file's value at
#     10 h with the flag on (the later rows were relative to the restart instead),
#   - InitialActivity in the metadata stays 1 MBq with the flag on (it was multiplied by the
#     fraction, although the run did not use that value).
#
# Usage:
#   tests/regtests/validate_abundance_restart.sh /path/to/topas

if [[ $# -ne 1 ]]; then
  echo "Usage: $0 /path/to/topas"
  exit 2
fi

TOPAS_BIN="$1"
if ! command -v "${TOPAS_BIN}" >/dev/null 2>&1 && [[ ! -x "${TOPAS_BIN}" ]]; then
  echo "ERROR: TOPAS binary/command not found: ${TOPAS_BIN}"
  exit 2
fi
if [[ -x "${TOPAS_BIN}" ]]; then
  TOPAS_BIN="$(cd "$(dirname "${TOPAS_BIN}")" && pwd)/$(basename "${TOPAS_BIN}")"
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

RUN_DIR="$(mktemp -d "${TMPDIR:-/tmp}/tsrts_restart.XXXXXX")"

write_deck() {
  cat <<DECK
s:Ge/World/Type      = "TsBox"
d:Ge/World/HLX       = 20 mm
d:Ge/World/HLY       = 20 mm
d:Ge/World/HLZ       = 20 mm
s:Ge/World/Material  = "G4_WATER"
s:Ge/Seed/Type       = "TsSphere"
s:Ge/Seed/Parent     = "World"
s:Ge/Seed/Material   = "G4_WATER"
d:Ge/Seed/RMax       = 1 mm
sv:Ph/Default/Modules = 1 "g4em-standard_opt4"
s:So/ChainSource/Mode                                 = "uniform"
s:So/ChainSource/Type                                 = "RadioactiveTimeSource"
s:So/ChainSource/Component                            = "Seed"
s:So/ChainSource/BeamParticle                         = "GenericIon(82, 212)"
b:So/ChainSource/CorrectByNumberOfHistories           = "True"
b:So/ChainSource/IncludeWholeDecayChain               = "True"
b:So/ChainSource/WriteIsotopicAbundance               = "True"
s:So/ChainSource/WriteIsotopicAbundanceFileName       = "iso.csv"
b:So/ChainSource/WriteRunMetadata                     = "True"
s:So/ChainSource/RunMetadataFileName                  = "meta.json"
u:So/ChainSource/InitialActivity                      = 1000000
i:So/ChainSource/NumberOfHistoriesInRun               = 2000
i:Ts/NumberOfThreads = 1
i:Ts/Seed            = 11
$1
DECK
}

mkdir -p "${RUN_DIR}/full" "${RUN_DIR}/off" "${RUN_DIR}/on"
write_deck 'd:Tf/TimelineStart = 0 s
d:Tf/TimelineEnd = 72000 s
i:Tf/NumberOfSequentialTimes = 4' > "${RUN_DIR}/full/deck.txt"
for flag in off on; do
  value="False"; [[ "${flag}" == "on" ]] && value="True"
  write_deck "d:Tf/TimelineStart = 36000 s
d:Tf/TimelineEnd = 72000 s
i:Tf/NumberOfSequentialTimes = 2
b:So/ChainSource/ReadIsotopicAbundanceFromFile = \"True\"
s:So/ChainSource/ReadIsotopicAbundanceFileName = \"../full/iso.csv\"
d:So/ChainSource/TimeToLoadIsotopicAbundance = 36000 s
b:So/ChainSource/UseFractionOfActivityFromFile = \"${value}\"" > "${RUN_DIR}/${flag}/deck.txt"
done

for name in full off on; do
  (cd "${RUN_DIR}/${name}" && "${TOPAS_BIN}" deck.txt > run.log 2>&1) || {
    echo "ERROR: TOPAS run failed (${name}); see ${RUN_DIR}/${name}/run.log"; exit 1; }
  for f in iso.csv meta.json; do
    [[ -f "${RUN_DIR}/${name}/${f}" ]] || { echo "ERROR: missing ${RUN_DIR}/${name}/${f}"; exit 1; }
  done
done

fail=0
ref="$(jq '.entries[] | select(.time_start_s == 36000) | .n_decays_step' "${RUN_DIR}/full/meta.json")"
f0="$(awk -F, 'NR > 1 && $1 + 0 == 36000 { print $NF }' "${RUN_DIR}/full/iso.csv")"

for flag in off on; do
  d="$(jq '.entries[0].n_decays_step' "${RUN_DIR}/${flag}/meta.json")"
  if ! awk -v a="${d}" -v b="${ref}" 'BEGIN { r = (a - b) / b; if (r < 0) r = -r; exit !(r < 1e-6) }'; then
    echo "FAIL: ${flag}: first-step decays ${d}, uninterrupted run ${ref}"; fail=1
  fi
  # One value per row, no trailing 0.
  if ! awk -F, 'NR > 1 && $NF + 0 <= 0 { bad = 1 } END { exit bad }' "${RUN_DIR}/${flag}/iso.csv"; then
    echo "FAIL: ${flag}: FractionOfDecaysAtTime0 has a zero or missing value"; fail=1
  fi
done

first_off="$(awk -F, 'NR == 2 { print $NF }' "${RUN_DIR}/off/iso.csv")"
second_off="$(awk -F, 'NR == 3 { print $NF }' "${RUN_DIR}/off/iso.csv")"
first_on="$(awk -F, 'NR == 2 { print $NF }' "${RUN_DIR}/on/iso.csv")"
second_on="$(awk -F, 'NR == 3 { print $NF }' "${RUN_DIR}/on/iso.csv")"
if ! awk -v v="${first_off}" 'BEGIN { exit !(v > 0.999999 && v < 1.000001) }'; then
  echo "FAIL: off: first FractionOfDecaysAtTime0 is ${first_off}, expected 1"; fail=1
fi
if ! awk -v v="${first_on}" -v w="${f0}" 'BEGIN { d = v - w; if (d < 0) d = -d; exit !(d < 1e-6) }'; then
  echo "FAIL: on: first FractionOfDecaysAtTime0 is ${first_on}, expected ${f0} from the file"; fail=1
fi
if ! awk -v a="${second_on}" -v b="${second_off}" -v f="${f0}" 'BEGIN { d = a - b * f; if (d < 0) d = -d; exit !(d < 1e-5) }'; then
  echo "FAIL: on: second FractionOfDecaysAtTime0 ${second_on} is not ${f0} x ${second_off}"; fail=1
fi

a0="$(jq '.entries[0].initial_activity_bq' "${RUN_DIR}/on/meta.json")"
if ! awk -v v="${a0}" 'BEGIN { exit !(v == 1000000) }'; then
  echo "FAIL: on: initial_activity_bq is ${a0}, expected 1000000"; fail=1
fi

if [[ "${fail}" -ne 0 ]]; then
  echo "Abundance restart check failed (${RUN_DIR})."
  exit 1
fi
echo "PASS: restart from an abundance file keeps the decays, the fraction column and InitialActivity (${RUN_DIR})"
