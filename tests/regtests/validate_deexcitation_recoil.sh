#!/usr/bin/env bash
set -euo pipefail

# Validates that DeexciteDaughters keeps the recoil of the daughter nucleus. Am-241 decays
# by alpha emission mostly to excited levels of Np-237 (85% to the 59.5 keV level), and the
# daughter recoils with about 90 keV. A point source in vacuum is run with the flag off (the
# excited Np-237 ion leaves with the recoil) and on (the ion is de-excited, and its products
# must keep moving with it); the mean kinetic energy of Np-237 ions crossing a 1 mm shell,
# per decay, must agree within 2%. Without the boost of the de-excitation products, the
# ions of the de-excited decays left at rest and the mean fell by about 85%.
#
# Usage:
#   tests/regtests/validate_deexcitation_recoil.sh /path/to/topas

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

WORK="$(mktemp -d /tmp/tsrts_deex_recoil.XXXX)"
trap 'rm -rf "${WORK}"' EXIT
cd "${WORK}"

HISTORIES=20000

cat > base.txt <<DECK
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
s:So/Src/BeamParticle = "GenericIon(95,241)"
u:So/Src/InitialActivity = 1000000
b:So/Src/WriteIsotopicAbundance = "False"
b:So/Src/IncludeWholeDecayChain = "False"
i:So/Src/NumberOfHistoriesInRun = ${HISTORIES}
d:Tf/TimelineStart = 0.0 s
d:Tf/TimelineEnd = 86400 s
i:Tf/NumberOfSequentialTimes = 1
s:Sc/PS/Quantity = "PhaseSpace"
s:Sc/PS/Surface = "Shell/OuterCurvedSurface"
s:Sc/PS/OutputType = "ASCII"
s:Sc/PS/IfOutputFileAlreadyExists = "Overwrite"
s:Sc/PS/OnlyIncludeParticlesGoing = "Out"
i:Ts/Seed = 241
DECK

printf 'includeFile = base.txt\nb:So/Src/DeexciteDaughters = "False"\ns:Sc/PS/OutputFile = "off"\n' > off.txt
printf 'includeFile = base.txt\nb:So/Src/DeexciteDaughters = "True"\ns:Sc/PS/OutputFile = "on"\n' > on.txt

"${TOPAS_BIN}" off.txt > off.log 2>&1 || { echo "FAIL: run without DeexciteDaughters exited non-zero"; tail -20 off.log; exit 1; }
"${TOPAS_BIN}" on.txt > on.log 2>&1 || { echo "FAIL: run with DeexciteDaughters exited non-zero"; tail -20 on.log; exit 1; }

# Np-237 ions (PDG 100093237x), kinetic energy in column 6 (MeV): keV per decay
recoil() {
  awk -v n="${HISTORIES}" 'int($8 / 10) == 100093237 { e += $6 } END { printf "%.4f\n", 1000 * e / n }' "$1"
}
off="$(recoil off.phsp)"
on="$(recoil on.phsp)"
echo "Np-237 recoil kinetic energy per decay: off ${off} keV, on ${on} keV"

awk -v a="${off}" 'BEGIN { exit !(a > 80 && a < 100) }' \
  || { echo "FAIL: recoil without DeexciteDaughters outside 80-100 keV per decay"; exit 1; }
awk -v a="${off}" -v b="${on}" 'BEGIN { r = b / a - 1; exit !(r > -0.02 && r < 0.02) }' \
  || { echo "FAIL: DeexciteDaughters changed the recoil energy of the daughter by more than 2%"; exit 1; }
echo "PASS: DeexciteDaughters keeps the recoil energy of the daughter nucleus"
