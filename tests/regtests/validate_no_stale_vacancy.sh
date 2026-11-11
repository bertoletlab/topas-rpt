#!/usr/bin/env bash
set -euo pipefail

# Validates that tsrts_deexcitation leaves no stale atomic vacancy for the next decay.
# A Tc-99m point source sits in a 1 um vacuum bubble at the centre of a water cube of 100 mm
# half-side; Tc K x-rays leaving the bubble per decay must match the same source in vacuum.
# G4UAtomicDeexcitation (Geant4 11.2 and earlier) keeps the vacancy of a Compton scatter on
# hydrogen, and when that photon escapes the next decay emits it as a Tc K cascade:
# without TsAtomicDeexcitation the water run gives 2.4 times the vacuum yield (0.181 against 0.074).
# Geant4 11.3 clears the vacancy list itself, so there this test passes with or without the
# class; it guards builds against Geant4 11.2.
#
# Usage:
#   tests/regtests/validate_no_stale_vacancy.sh /path/to/topas

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
  fi
fi

WORK="$(mktemp -d /tmp/tsrts_vacancy.XXXX)"
trap 'rm -rf "${WORK}"' EXIT
cd "${WORK}"

N=200000
cat > base.txt <<DECK
d:Ge/World/HLX = 101 mm
d:Ge/World/HLY = 101 mm
d:Ge/World/HLZ = 101 mm
s:Ge/World/Material = "G4_Galactic"
s:Ge/Medium/Type = "TsBox"
s:Ge/Medium/Parent = "World"
d:Ge/Medium/HLX = 100 mm
d:Ge/Medium/HLY = 100 mm
d:Ge/Medium/HLZ = 100 mm
s:Ge/Bubble/Type = "TsSphere"
s:Ge/Bubble/Parent = "Medium"
s:Ge/Bubble/Material = "G4_Galactic"
d:Ge/Bubble/RMax = 0.001 mm
s:Ge/Point/Parent = "Bubble"
s:Ge/Point/Type = "Group"
sv:Ph/Default/Modules = 2 "g4em-livermore" "tsrts_deexcitation"
b:Ph/Default/Fluorescence = "True"
b:Ph/Default/Auger = "True"
b:Ph/Default/DeexcitationIgnoreCut = "True"
s:So/Src/Type = "RadioactiveTimeSource"
s:So/Src/Mode = "uniform"
s:So/Src/Component = "Point"
s:So/Src/BeamParticle = "GenericIon(43,99)"
d:So/Src/ExcitationEnergy = 142.6832 keV
b:So/Src/DeexciteDaughters = "True"
u:So/Src/InitialActivity = 1000000
b:So/Src/WriteIsotopicAbundance = "False"
b:So/Src/IncludeWholeDecayChain = "False"
i:So/Src/NumberOfHistoriesInRun = ${N}
d:Tf/TimelineStart = 0.0 s
d:Tf/TimelineEnd = 8640000 s
i:Tf/NumberOfSequentialTimes = 1
s:Sc/PS/Quantity = "PhaseSpace"
s:Sc/PS/Surface = "Bubble/OuterCurvedSurface"
s:Sc/PS/OutputType = "ASCII"
s:Sc/PS/IfOutputFileAlreadyExists = "Overwrite"
s:Sc/PS/OnlyIncludeParticlesGoing = "Out"
i:Ts/NumberOfThreads = 4
i:Ts/Seed = 99
b:Ts/PauseBeforeQuit = "False"
DECK

printf 'includeFile = base.txt\ns:Ge/Medium/Material = "G4_WATER"\ns:Sc/PS/OutputFile = "water"\n' > water.txt
printf 'includeFile = base.txt\ns:Ge/Medium/Material = "G4_Galactic"\ns:Sc/PS/OutputFile = "vacuum"\n' > vacuum.txt

"${TOPAS_BIN}" water.txt > water.log 2>&1
"${TOPAS_BIN}" vacuum.txt > vacuum.log 2>&1

# Tc K x-rays (K-alpha 18.25-18.37 keV, K-beta 20.6-21.0 keV) per decay
kx() {
  awk -v n="${N}" '$8 == 22 && $6 > 0.0180 && $6 < 0.0212 { k++ } END { printf "%.4f\n", k / n }' "$1"
}
x_water="$(kx water.phsp)"
x_vacuum="$(kx vacuum.phsp)"
echo "Tc K x-rays per decay: water ${x_water}, vacuum ${x_vacuum}"

# 200000 decays give about 12000 x-rays per run, 1% statistics; the stale vacancy gave 2.4x
awk -v w="${x_water}" -v v="${x_vacuum}" 'BEGIN { exit !(v > 0.060 && v < 0.090 && w / v > 0.96 && w / v < 1.04) }' \
  || { echo "FAIL: Tc K x-ray yield in water differs from vacuum (stale atomic vacancy)"; exit 1; }

echo "PASS: no stale atomic vacancy reaches the next decay"
