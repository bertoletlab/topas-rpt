#!/usr/bin/env bash
set -euo pipefail

# Validates So/<Source>/FirstForbiddenBetaShape (1.2.0). Tb-161 decays almost entirely
# through branches its Geant4 data tag "firstForbidden"; Geant4 11.2 shapes them with a
# Bi-210 fit, which puts the mean beta energy near 140 keV, against 154.6 keV in ICRP 107.
# With "allowed" the mean must come within 3% of ICRP 107; Lu-177, which has no such
# branch, must not change at all; an unknown value must stop with TsRADIOACTIVE021.
#
# Usage:
#   tests/regtests/validate_first_forbidden_shape.sh /path/to/topas

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

WORK="$(mktemp -d /tmp/tsrts_ffshape.XXXX)"
trap 'rm -rf "${WORK}"' EXIT
cd "${WORK}"

deck() {  # name ion shape
  cat > "$1.txt" <<DECK
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
sv:Ph/Default/Modules = 1 "g4em-livermore"
s:So/Src/Type = "RadioactiveTimeSource"
s:So/Src/Mode = "uniform"
s:So/Src/Component = "Point"
s:So/Src/BeamParticle = "$2"
u:So/Src/InitialActivity = 1000000
b:So/Src/WriteIsotopicAbundance = "False"
b:So/Src/IncludeWholeDecayChain = "False"
s:So/Src/FirstForbiddenBetaShape = "$3"
i:So/Src/NumberOfHistoriesInRun = 20000
d:Tf/TimelineStart = 0.0 s
d:Tf/TimelineEnd = 8640000 s
i:Tf/NumberOfSequentialTimes = 1
s:Sc/PS/Quantity = "PhaseSpace"
s:Sc/PS/Surface = "Shell/OuterCurvedSurface"
s:Sc/PS/OutputType = "ASCII"
s:Sc/PS/IfOutputFileAlreadyExists = "Overwrite"
s:Sc/PS/OnlyIncludeParticlesGoing = "Out"
s:Sc/PS/OutputFile = "$1"
i:Ts/Seed = 161
DECK
}
mean_e() { awk '$8 == 11 { s += $6; n++ } END { printf "%.6f\n", 1000 * s / n }' "$1.phsp"; }

deck tb_geant4 "GenericIon(65,161)" geant4
deck tb_allowed "GenericIon(65,161)" allowed
deck lu_geant4 "GenericIon(71,177)" geant4
deck lu_allowed "GenericIon(71,177)" allowed
deck bad "GenericIon(65,161)" unique
for d in tb_geant4 tb_allowed lu_geant4 lu_allowed; do "${TOPAS_BIN}" $d.txt > $d.log 2>&1; done
if "${TOPAS_BIN}" bad.txt > bad.log 2>&1 || ! grep -q TsRADIOACTIVE021 bad.log; then
  echo "FAIL: an unknown FirstForbiddenBetaShape did not stop with TsRADIOACTIVE021"
  exit 1
fi

tb_g=$(mean_e tb_geant4); tb_a=$(mean_e tb_allowed); lu_g=$(mean_e lu_geant4); lu_a=$(mean_e lu_allowed)
echo "Tb-161 mean beta: geant4 ${tb_g} keV, allowed ${tb_a} keV (ICRP 107: 154.57)"
echo "Lu-177 mean beta: geant4 ${lu_g} keV, allowed ${lu_a} keV"
awk -v a="${tb_a}" -v g="${tb_g}" 'BEGIN { exit !(a > 150.0 && a < 159.2 && g < 145.0) }' \
  || { echo "FAIL: Tb-161 mean beta outside the expected range"; exit 1; }
[[ "${lu_g}" == "${lu_a}" ]] || { echo "FAIL: Lu-177 changed although it has no first-forbidden branch"; exit 1; }
echo "PASS: FirstForbiddenBetaShape allowed brings Tb-161 to ICRP 107 and leaves Lu-177 unchanged"
echo "PASS: an unknown FirstForbiddenBetaShape stops with TsRADIOACTIVE021"
