#!/usr/bin/env bash
set -euo pipefail

# With CorrectByNumberOfHistories = True, a step's history count is the first step's count
# scaled by its share of decays and truncated to an integer. Once that rounded to 0, no history
# updated the isotopic abundances any more, so the chain froze and every later row of the
# abundance file repeated the same values.
#
# Runs Pb-212 (half-life 10.64 h) with 200 histories over 12 one-day steps, so the count reaches
# zero after about four days, and checks that
#   - every step with decays ran at least one history (run metadata),
#   - Pb-212 keeps falling after the first one-history step instead of staying constant.
#
# Usage:
#   tests/regtests/validate_zero_history_steps.sh /path/to/topas

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

RUN_DIR="$(mktemp -d "${TMPDIR:-/tmp}/tsrts_zero_hist.XXXXXX")"

cat > "${RUN_DIR}/deck.txt" <<'DECK'
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
s:So/ChainSource/WriteIsotopicAbundanceFileName       = "iso_chain.csv"
b:So/ChainSource/WriteRunMetadata                     = "True"
s:So/ChainSource/RunMetadataFileName                  = "run_metadata_chain.json"
u:So/ChainSource/InitialActivity                      = 1000000
i:So/ChainSource/NumberOfHistoriesInRun               = 200
d:Tf/TimelineStart           = 0 s
d:Tf/TimelineEnd             = 1036800 s
i:Tf/NumberOfSequentialTimes = 12
i:Ts/NumberOfThreads = 1
i:Ts/Seed            = 11
DECK

(cd "${RUN_DIR}" && "${TOPAS_BIN}" deck.txt > run.log 2>&1) || {
  echo "ERROR: TOPAS run failed; see ${RUN_DIR}/run.log"
  tail -n 40 "${RUN_DIR}/run.log" || true
  exit 1
}
for f in iso_chain.csv run_metadata_chain.json; do
  [[ -f "${RUN_DIR}/${f}" ]] || { echo "ERROR: missing ${RUN_DIR}/${f}"; exit 1; }
done

fail=0

# Every step with decays ran at least one history.
zero="$(jq '[.entries[] | select(.n_decays_step > 0 and .n_histories_step < 1)] | length' "${RUN_DIR}/run_metadata_chain.json")"
ones="$(jq '[.entries[] | select(.n_histories_step == 1)] | length' "${RUN_DIR}/run_metadata_chain.json")"
if [[ "${zero}" != "0" ]]; then
  echo "FAIL: ${zero} step(s) with decays ran no history."
  fail=1
fi
if [[ "${ones}" -lt 5 ]]; then
  echo "FAIL: only ${ones} one-history step(s); the deck no longer reaches the rounding limit."
  fail=1
fi

# Pb-212 keeps changing after the history count reached its floor. With one history per step,
# a step can leave Pb-212 unchanged when that history is a daughter decay, so the check is that
# the last value is below the value at the first one-history step, not that every row changes.
first_one_t="$(jq '[.entries[] | select(.n_histories_step == 1)][0].time_start_s' "${RUN_DIR}/run_metadata_chain.json")"
if ! awk -F, -v t1="${first_one_t}" '
    NR == 1 { for (i = 1; i <= NF; i++) if ($i == "Pb212") col = i; next }
    $1 + 0 == t1 + 0 { at1 = $col }
    { last = $col }
    END {
      if (!col) { print "FAIL: no Pb212 column"; exit 1 }
      if (at1 == "") { printf("FAIL: no abundance row at t=%s s\n", t1); exit 1 }
      if (last + 0 >= at1 + 0) { printf("FAIL: Pb212 froze at %s from t=%s s\n", at1, t1); exit 1 }
    }' "${RUN_DIR}/iso_chain.csv"; then
  fail=1
fi

if [[ "${fail}" -ne 0 ]]; then
  echo "Zero-history step check failed (${RUN_DIR})."
  exit 1
fi
echo "PASS: steps keep at least one history and the chain keeps decaying (${RUN_DIR})"
