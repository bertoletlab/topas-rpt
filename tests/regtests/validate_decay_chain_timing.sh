#!/usr/bin/env bash
set -euo pipefail

# Decay timing and decay positions:
# - Pb-212 (4 steps of 2 h, chain on): Bi-212 at each step boundary must follow the Bateman
#   solution within 10%. With the primary decay at the start of every step, the daughters got the
#   whole step to decay in and Bi-212 came out at less than half of it.
# - Lu-177 in vitro with IncludeWholeDecayChain False: the products of one decay must share one
#   position (each product used to get its own random position).
#
# Usage:
#   tests/regtests/validate_decay_chain_timing.sh /path/to/topas

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

OUT="$(mktemp -d "${TMPDIR:-/tmp}/tsrts_chain_timing.XXXXXX")"
fail=0

deck() {
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
s:So/Src/Mode         = "uniform"
s:So/Src/Type         = "RadioactiveTimeSource"
s:So/Src/Component    = "Seed"
u:So/Src/InitialActivity = 1000000
b:So/Src/WriteIsotopicAbundance = "True"
s:So/Src/WriteIsotopicAbundanceFileName = "iso.csv"
i:Ts/NumberOfThreads = 1
i:Ts/Seed            = 11
DECK
}

# Bi-212 against Bateman.
mkdir -p "${OUT}/bateman"
{ deck; cat <<'DECK'
s:So/Src/BeamParticle = "GenericIon(82, 212)"
b:So/Src/IncludeWholeDecayChain = "True"
i:So/Src/NumberOfHistoriesInRun = 4000
d:Tf/TimelineStart           = 0 s
d:Tf/TimelineEnd             = 28800 s
i:Tf/NumberOfSequentialTimes = 4
DECK
} > "${OUT}/bateman/deck.txt"
( cd "${OUT}/bateman" && "${TOPAS_BIN}" deck.txt > run.log 2>&1 ) || { echo "FAIL: Pb-212 run failed; see ${OUT}/bateman/run.log"; exit 1; }
if ! awk -F, '
    NR == 1 { for (i = 1; i <= NF; i++) { if ($i == "Pb212") p = i; if ($i == "Bi212") b = i }; next }
    $1 > 0 {
      lp = log(2) / (10.64 * 3600); lb = log(2) / (60.55 * 60)
      expected = lp / (lb - lp) * (exp(-lp * $1) - exp(-lb * $1))
      rel = ($b - expected) / expected; if (rel < 0) rel = -rel
      if (rel > 0.10) { printf("FAIL: Bi212 at t=%s s is %s, Bateman %.4f\n", $1, $b, expected); bad = 1 }
      n++
    }
    END { if (!p || !b || n != 3) { print "FAIL: iso.csv lacks the Pb212/Bi212 columns or rows"; bad = 1 }; exit bad }' "${OUT}/bateman/iso.csv"; then
  fail=1
fi

# One position per decay with the chain off.
mkdir -p "${OUT}/chain_off"
cat > "${OUT}/chain_off/deck.txt" <<'DECK'
s:Ge/World/Type = "TsBox"
d:Ge/World/HLX  = 0.5 mm
d:Ge/World/HLY  = 0.5 mm
d:Ge/World/HLZ  = 0.2 mm
s:Ge/World/Material = "G4_Galactic"
s:Ge/Cells/Type            = "TsCellMonolayer"
s:Ge/Cells/Parent          = "World"
s:Ge/Cells/CellGeometry    = "Cylinder"
i:Ge/Cells/NumOfCells      = 100
d:Ge/Cells/RCell           = 10 um
d:Ge/Cells/HCell           = 12 um
d:Ge/Cells/RNucleus        = 7 um
d:Ge/Cells/HNucleus        = 9 um
d:Ge/Cells/MediumHLX       = 0.4 mm
d:Ge/Cells/MediumHLY       = 0.4 mm
d:Ge/Cells/MediumHLZ       = 0.15 mm
s:Ge/Cells/Material        = "G4_WATER"
s:Ge/Cells/Cell/Material   = "G4_WATER"
s:Ge/Cells/Nucleus/Material= "G4_WATER"
s:So/Bind/Type      = "RadioactiveTimeSourceBind"
s:So/Bind/Component = "Cells"
s:So/Bind/BeamParticle = "GenericIon(71, 177)"
b:So/Bind/IncludeWholeDecayChain = "False"
u:So/Bind/InitialActivity = 1000000
i:So/Bind/NumberOfHistoriesInRun = 20
i:So/Bind/Verbosity = 2
u:So/Bind/Activity_permL = 20000
u:So/Bind/SpecificActivity_pernmol = 25000000
s:So/Bind/BindingModel = "Manual"
uv:So/Bind/ManualBindingProbabilities = 5 10 70 10 5 5
d:Tf/TimelineStart = 0 s
d:Tf/TimelineEnd   = 3600 s
i:Tf/NumberOfSequentialTimes = 1
i:Ts/NumberOfThreads = 1
i:Ts/Seed = 1
DECK
( cd "${OUT}/chain_off" && "${TOPAS_BIN}" deck.txt > run.log 2>&1 ) || { echo "FAIL: Lu-177 run failed; see ${OUT}/chain_off/run.log"; exit 1; }
# Each history logs its sampled position, then the reset to (0,0,0).
sampled="$(grep "New position for history" "${OUT}/chain_off/run.log" | grep -vc "(0,0,0)" || true)"
if [[ "${sampled}" != "20" ]]; then
  echo "FAIL: chain off: ${sampled} positions sampled for 20 decays (one per decay expected)"
  fail=1
fi

if [[ "${fail}" -ne 0 ]]; then
  echo "Decay timing/position check failed (${OUT})."
  exit 1
fi
echo "PASS: Bi-212 follows Bateman; one position per decay with the chain off (${OUT})"
