#!/usr/bin/env bash
set -euo pipefail

# Decay-chain bookkeeping must not depend on the thread count or on CorrectByNumberOfHistories.
#
# Runs a Pb-212 uniform source (4 steps of 10 h) three times:
#   1 thread, CorrectByNumberOfHistories True
#   4 threads, CorrectByNumberOfHistories True
#   1 thread, CorrectByNumberOfHistories False
# and checks that
#   - the 1- and 4-thread abundance files agree (each worker used to deplete the parent only
#     with its own share of the histories, so 4 threads decayed Pb-212 about 4x too slowly),
#   - the 4-thread run wrote one abundance file with one row per step and no temp.csv,
#   - Pb-212 follows its physical decay within 6% in all three runs (with False, the abundances
#     used the normalised weight and Pb-212 dropped to 0 after the second step). The margin covers
#     the binomial noise from the histories that go to Bi-212 rather than Pb-212; the defects
#     above are 60% to 85% off.
#
# Usage:
#   tests/regtests/validate_multithread_chain_bookkeeping.sh /path/to/topas

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

RUN_DIR="$(mktemp -d "${TMPDIR:-/tmp}/tsrts_mt_chain.XXXXXX")"

write_deck() {
  local threads="$1"
  local correct="$2"
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
b:So/ChainSource/CorrectByNumberOfHistories           = "${correct}"
b:So/ChainSource/IncludeWholeDecayChain               = "True"
b:So/ChainSource/WriteIsotopicAbundance               = "True"
s:So/ChainSource/WriteIsotopicAbundanceFileName       = "iso_chain.csv"
s:So/ChainSource/RunMetadataFileName                  = "run_metadata_chain.json"
u:So/ChainSource/InitialActivity                      = 1000000
i:So/ChainSource/NumberOfHistoriesInRun               = 2000
d:Tf/TimelineStart           = 0 s
d:Tf/TimelineEnd             = 144000 s
i:Tf/NumberOfSequentialTimes = 4
i:Ts/NumberOfThreads = ${threads}
i:Ts/Seed            = 11
DECK
}

for cfg in "t1:1:True" "t4:4:True" "f1:1:False"; do
  IFS=: read -r name threads correct <<<"${cfg}"
  mkdir -p "${RUN_DIR}/${name}"
  write_deck "${threads}" "${correct}" > "${RUN_DIR}/${name}/deck.txt"
  (cd "${RUN_DIR}/${name}" && "${TOPAS_BIN}" deck.txt > run.log 2>&1) || {
    echo "ERROR: TOPAS run failed (${name}); see ${RUN_DIR}/${name}/run.log"
    exit 1
  }
  [[ -f "${RUN_DIR}/${name}/iso_chain.csv" ]] || { echo "ERROR: missing ${RUN_DIR}/${name}/iso_chain.csv"; exit 1; }
done

fail=0

# One header and one row per step, written once.
rows="$(wc -l < "${RUN_DIR}/t4/iso_chain.csv" | tr -d ' ')"
if [[ "${rows}" != "5" ]]; then
  echo "FAIL: 4-thread abundance file has ${rows} lines, expected 5 (header + 4 steps)."
  fail=1
fi
if [[ -e "${RUN_DIR}/t4/temp.csv" ]]; then
  echo "FAIL: 4-thread run left temp.csv behind."
  fail=1
fi

# Same chain with 1 and 4 threads.
if ! paste -d, "${RUN_DIR}/t1/iso_chain.csv" "${RUN_DIR}/t4/iso_chain.csv" | awk -F, '
    NR == 1 { n = NF / 2; next }
    { for (i = 1; i <= n; i++) { d = $i - $(i + n); if (d < 0) d = -d; if (d > 1e-4) { printf("FAIL: 1 vs 4 threads, row %d column %d: %s vs %s\n", NR, i, $i, $(i + n)); bad = 1 } } }
    END { exit bad }'; then
  fail=1
fi

# Pb-212 follows its physical decay (half-life 10.64 h) within 6%.
for name in t1 t4 f1; do
  if ! awk -F, -v run="${name}" '
      NR == 1 { for (i = 1; i <= NF; i++) if ($i == "Pb212") col = i; next }
      {
        expected = exp(-log(2) * $1 / (10.64 * 3600))
        rel = ($col - expected) / expected; if (rel < 0) rel = -rel
        if (rel > 0.06) { printf("FAIL: %s: Pb212 at t=%s s is %s, expected %.4f\n", run, $1, $col, expected); bad = 1 }
      }
      END { if (!col) { print "FAIL: no Pb212 column"; bad = 1 }; exit bad }' "${RUN_DIR}/${name}/iso_chain.csv"; then
    fail=1
  fi
done

if [[ "${fail}" -ne 0 ]]; then
  echo "Multithread chain bookkeeping check failed (${RUN_DIR})."
  exit 1
fi
echo "PASS: decay-chain bookkeeping is independent of thread count and CorrectByNumberOfHistories (${RUN_DIR})"
