#!/usr/bin/env bash
set -euo pipefail

# Validates So/<Source>/DeexciteDaughters (1.1.0): a Lu-177 point source in vacuum must emit
# the de-excitation products of Hf-177 (113 and 208 keV gammas, K-shell conversion
# electrons, Hf K x-rays) only when the flag and the tsrts_deexcitation module are set, and
# the flag without the module must stop with TsRADIOACTIVE020.
#
# Usage:
#   tests/regtests/validate_deexcite_daughters.sh /path/to/topas

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

WORK="$(mktemp -d /tmp/tsrts_deexcite.XXXX)"
trap 'rm -rf "${WORK}"' EXIT
cd "${WORK}"

cat > base.txt <<'DECK'
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
b:Ph/Default/Fluorescence = "True"
b:Ph/Default/Auger = "True"
b:Ph/Default/DeexcitationIgnoreCut = "True"
s:So/Src/Type = "RadioactiveTimeSource"
s:So/Src/Mode = "uniform"
s:So/Src/Component = "Point"
s:So/Src/BeamParticle = "GenericIon(71,177)"
u:So/Src/InitialActivity = 1000000
b:So/Src/WriteIsotopicAbundance = "False"
b:So/Src/IncludeWholeDecayChain = "False"
i:So/Src/NumberOfHistoriesInRun = 20000
d:Tf/TimelineStart = 0.0 s
d:Tf/TimelineEnd = 8640000 s
i:Tf/NumberOfSequentialTimes = 1
s:Sc/PS/Quantity = "PhaseSpace"
s:Sc/PS/Surface = "Shell/OuterCurvedSurface"
s:Sc/PS/OutputType = "ASCII"
s:Sc/PS/IfOutputFileAlreadyExists = "Overwrite"
s:Sc/PS/OnlyIncludeParticlesGoing = "Out"
i:Ts/Seed = 177
DECK

printf 'includeFile = base.txt\nsv:Ph/Default/Modules = 1 "g4em-livermore"\ns:Sc/PS/OutputFile = "off"\n' > off.txt
printf 'includeFile = base.txt\nsv:Ph/Default/Modules = 2 "g4em-livermore" "tsrts_deexcitation"\nb:So/Src/DeexciteDaughters = "True"\ns:Sc/PS/OutputFile = "on"\n' > on.txt
printf 'includeFile = base.txt\nsv:Ph/Default/Modules = 1 "g4em-livermore"\nb:So/Src/DeexciteDaughters = "True"\ns:Sc/PS/OutputFile = "nomodule"\n' > nomodule.txt

"${TOPAS_BIN}" off.txt > off.log 2>&1
"${TOPAS_BIN}" on.txt > on.log 2>&1
if "${TOPAS_BIN}" nomodule.txt > nomodule.log 2>&1 || ! grep -q TsRADIOACTIVE020 nomodule.log; then
  echo "FAIL: DeexciteDaughters without tsrts_deexcitation did not stop with TsRADIOACTIVE020"
  exit 1
fi

# per-decay yields: 208 keV gammas, 47.5 keV K conversion electrons of the 113 keV line,
# Hf K-alpha x-rays (54.6-56.0 keV)
yields() {
  awk -v n=20000 '
    $8 == 22 && $6 > 0.20787 && $6 < 0.20887 { g208++ }
    $8 == 11 && $6 > 0.04697 && $6 < 0.04797 { ce47++ }
    $8 == 22 && $6 > 0.0545 && $6 < 0.0561 { xk++ }
    END { printf "%.4f %.4f %.4f\n", g208 / n, ce47 / n, xk / n }' "$1"
}
read -r g_off ce_off x_off < <(yields off.phsp)
read -r g_on ce_on x_on < <(yields on.phsp)
echo "off: 208keV ${g_off} CE47 ${ce_off} HfKa ${x_off}"
echo "on:  208keV ${g_on} CE47 ${ce_on} HfKa ${x_on}"

# ICRP 107: 0.104 (208 keV), 0.052 (K CE of 113 keV), 0.045 (K-alpha)
awk -v g="${g_off}" -v x="${x_off}" 'BEGIN { exit !(g == 0 && x == 0) }' \
  || { echo "FAIL: de-excitation products without DeexciteDaughters"; exit 1; }
awk -v g="${g_on}" -v c="${ce_on}" -v x="${x_on}" \
  'BEGIN { exit !(g > 0.093 && g < 0.115 && c > 0.045 && c < 0.065 && x > 0.035 && x < 0.055) }' \
  || { echo "FAIL: DeexciteDaughters yields outside the expected range"; exit 1; }

echo "PASS: DeexciteDaughters emits the Hf-177 de-excitation products, only when asked"
echo "PASS: DeexciteDaughters without tsrts_deexcitation stops with TsRADIOACTIVE020"
