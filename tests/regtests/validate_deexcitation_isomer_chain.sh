#!/usr/bin/env bash
set -euo pipefail

# Validates that an isomer kept by DeexciteDaughters stays an isomer in the decay chain.
# Mo-99 feeds Tc99[142.684] (Tc-99m, mean life 8.7 h) in about 88% of its decays; the
# de-excitation stops there (IsomerLifetimeThreshold), and with IncludeWholeDecayChain the
# chain must then decay Tc-99m with its own lifetime, emitting its 140.5 keV gamma (89% per
# Tc-99m decay). A Mo-99 point source in vacuum over one 30-day step must give about 0.8
# gammas of 140.5 keV per Mo-99 decay; when the chain replaced Tc-99m by ground-state Tc-99
# (2e5 y), only the few Mo-99 decays through higher levels gave one (about 0.05).
#
# Usage:
#   tests/regtests/validate_deexcitation_isomer_chain.sh /path/to/topas

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

if ! command -v jq >/dev/null 2>&1; then
  echo "ERROR: jq is required"
  exit 2
fi

WORK="$(mktemp -d /tmp/tsrts_deex_isomer.XXXX)"
trap 'rm -rf "${WORK}"' EXIT
cd "${WORK}"

cat > deck.txt <<'DECK'
d:Ge/World/HLX = 50 mm
d:Ge/World/HLY = 50 mm
d:Ge/World/HLZ = 50 mm
s:Ge/World/Material = "G4_Galactic"
s:Ge/Shell/Type = "TsSphere"
s:Ge/Shell/Parent = "World"
s:Ge/Shell/Material = "G4_Galactic"
d:Ge/Shell/RMax = 1 mm
s:Ge/Point/Parent = "World"
s:Ge/Point/Type = "Group"
sv:Ph/Default/Modules = 2 "g4em-livermore" "tsrts_deexcitation"
b:Ph/Default/Fluorescence = "True"
b:Ph/Default/Auger = "True"
b:Ph/Default/DeexcitationIgnoreCut = "True"
s:So/Src/Type = "RadioactiveTimeSource"
s:So/Src/Mode = "uniform"
s:So/Src/Component = "Point"
s:So/Src/BeamParticle = "GenericIon(42,99)"
u:So/Src/InitialActivity = 1000000
b:So/Src/WriteIsotopicAbundance = "False"
b:So/Src/WriteRunMetadata = "True"
s:So/Src/RunMetadataFileName = "meta.json"
b:So/Src/IncludeWholeDecayChain = "True"
b:So/Src/TreatAdditionalDecaysAsNewHistories = "False"
b:So/Src/CorrectByNumberOfHistories = "True"
b:So/Src/DeexciteDaughters = "True"
i:So/Src/NumberOfHistoriesInRun = 10000
d:Tf/TimelineStart = 0.0 s
d:Tf/TimelineEnd = 2592000 s
i:Tf/NumberOfSequentialTimes = 1
s:Sc/PS/Quantity = "PhaseSpace"
s:Sc/PS/Surface = "Shell/OuterCurvedSurface"
s:Sc/PS/OutputType = "ASCII"
s:Sc/PS/OutputFile = "ps"
s:Sc/PS/IfOutputFileAlreadyExists = "Overwrite"
s:Sc/PS/OnlyIncludeParticlesGoing = "Out"
i:Ts/Seed = 99
DECK

"${TOPAS_BIN}" deck.txt > run.log 2>&1 || { echo "FAIL: run exited non-zero"; tail -20 run.log; exit 1; }

# 140.5 keV gammas per history (one Mo-99 decay each)
g140="$(awk -v n=10000 '$8 == 22 && $6 > 0.1400 && $6 < 0.1410 { g++ } END { printf "%.4f\n", g / n }' ps.phsp)"
echo "140.5 keV gammas per Mo-99 decay: ${g140}"
awk -v g="${g140}" 'BEGIN { exit !(g > 0.70 && g < 0.90) }' \
  || { echo "FAIL: 140.5 keV yield outside 0.70-0.90: Tc-99m was not decayed in the chain"; exit 1; }
echo "PASS: the chain decays the Tc-99m kept by DeexciteDaughters"
