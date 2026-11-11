#!/usr/bin/env bash
set -euo pipefail

# Output-file contract:
# A. With no WriteIsotopicAbundance parameter (written by default), the abundance file is written
#    as isotopic_abundance.csv, no temporary file is left, and times are written in full
#    (1209600, not 1.2096e+06).
# B. Reading that file back at an exact row time loads that row, including its
#    FractionOfDecaysAtTime0 (times of 1e6 s and more were read as 1, and an exact match took the
#    fraction of the last row).
# C. An uptake file with a cell id outside 0..NumOfCells-1 is rejected at setup.
#
# Usage:
#   tests/regtests/validate_output_files.sh /path/to/topas

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

OUT="$(mktemp -d "${TMPDIR:-/tmp}/tsrts_outputs.XXXXXX")"
fail=0

uniform_deck() {
  cat <<'DECK'
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
s:So/Src/BeamParticle = "GenericIon(71, 177)"
u:So/Src/InitialActivity = 1000000
i:Ts/NumberOfThreads = 1
i:Ts/Seed            = 3
DECK
}

# A. Default abundance file over 28 days (rows at 0, 7, 14 and 21 days).
mkdir -p "${OUT}/a"
{ uniform_deck; cat <<'DECK'
i:So/Src/NumberOfHistoriesInRun = 200
d:Tf/TimelineStart           = 0 s
d:Tf/TimelineEnd             = 2419200 s
i:Tf/NumberOfSequentialTimes = 4
DECK
} > "${OUT}/a/deck.txt"
( cd "${OUT}/a" && "${TOPAS_BIN}" deck.txt > run.log 2>&1 ) || { echo "FAIL: A: run failed; see ${OUT}/a/run.log"; exit 1; }
if [[ ! -f "${OUT}/a/isotopic_abundance.csv" ]]; then
  echo "FAIL: A: isotopic_abundance.csv was not written by default"
  fail=1
else
  times="$(tail -n +2 "${OUT}/a/isotopic_abundance.csv" | cut -d, -f1 | tr '\n' ' ')"
  if [[ "${times}" != "0 604800 1209600 1814400 " ]]; then
    echo "FAIL: A: time column is '${times}', expected '0 604800 1209600 1814400 '"
    fail=1
  fi
fi
leftovers="$(cd "${OUT}/a" && ls temp.csv ./*.tmp 2>/dev/null || true)"
if [[ -n "${leftovers}" ]]; then
  echo "FAIL: A: temporary file left behind: ${leftovers}"
  fail=1
fi

# B. Read the 14-day row back.
if [[ -f "${OUT}/a/isotopic_abundance.csv" ]]; then
  mkdir -p "${OUT}/b"
  { uniform_deck; cat <<DECK
i:So/Src/NumberOfHistoriesInRun = 20
b:So/Src/WriteIsotopicAbundance = "False"
b:So/Src/ReadIsotopicAbundanceFromFile = "True"
s:So/Src/ReadIsotopicAbundanceFileName = "${OUT}/a/isotopic_abundance.csv"
d:So/Src/TimeToLoadIsotopicAbundance = 1209600 s
b:So/Src/UseFractionOfActivityFromFile = "True"
d:Tf/TimelineStart           = 0 s
d:Tf/TimelineEnd             = 3600 s
i:Tf/NumberOfSequentialTimes = 1
DECK
  } > "${OUT}/b/deck.txt"
  ( cd "${OUT}/b" && "${TOPAS_BIN}" deck.txt > run.log 2>&1 ) || { echo "FAIL: B: run failed; see ${OUT}/b/run.log"; exit 1; }
  # The notice reports the fraction loaded from the file for that row.
  expected="$(awk -F, '$1 == 1209600 { printf("%.6g", $NF) }' "${OUT}/a/isotopic_abundance.csv")"
  got="$(sed -n 's/.*the file gives FractionOfDecaysAtTime0 = \([0-9.eE+-]*\) .*/\1/p' "${OUT}/b/run.log" | head -n 1)"
  if [[ -z "${got}" ]] || ! awk -v g="${got}" -v e="${expected}" 'BEGIN { d = g - e; if (d < 0) d = -d; exit !(e > 0 && d <= 1e-4 * e) }'; then
    echo "FAIL: B: fraction loaded for 1209600 s is '${got}', expected ${expected} (that row's FractionOfDecaysAtTime0)"
    fail=1
  fi
fi

# C. Uptake file with an id beyond NumOfCells.
mkdir -p "${OUT}/c"
printf '0 1\n150 2\n' > "${OUT}/c/uptake.txt"
cat > "${OUT}/c/deck.txt" <<'DECK'
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
u:So/Bind/InitialActivity = 1000000
i:So/Bind/NumberOfHistoriesInRun = 10
u:So/Bind/Activity_permL = 20000
u:So/Bind/SpecificActivity_pernmol = 25000000
s:So/Bind/BindingModel = "Manual"
uv:So/Bind/ManualBindingProbabilities = 5 10 70 10 5 5
s:So/Bind/UptakePerCellFilename = "uptake.txt"
d:Tf/TimelineStart = 0 s
d:Tf/TimelineEnd   = 3600 s
i:Tf/NumberOfSequentialTimes = 1
i:Ts/NumberOfThreads = 1
DECK
set +e
( cd "${OUT}/c" && "${TOPAS_BIN}" deck.txt > run.log 2>&1 )
rc=$?
set -e
if [[ ${rc} -eq 0 ]] || ! grep -q "ids must be from 0 to NumOfCells - 1" "${OUT}/c/run.log"; then
  echo "FAIL: C: uptake id 150 with 100 cells was not rejected (exit code ${rc}); see ${OUT}/c/run.log"
  fail=1
fi

if [[ "${fail}" -ne 0 ]]; then
  echo "Output-file check failed (${OUT})."
  exit 1
fi
echo "PASS: default abundance file, full-precision times, read-back at an exact time, uptake id check (${OUT})"
