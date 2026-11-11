#!/usr/bin/env bash
set -euo pipefail

# Energy per decay against Geant4's own radioactive decay. A source in a 0.5 mm water sphere
# at the centre of a 60 cm water box is run twice: with RadioactiveTimeSource and
# DeexciteDaughters (the tsrts_deexcitation module), and as a plain Volumetric source of the
# same ion with g4radioactivedecay, which decays the ion and its whole chain during tracking.
# The energy deposited per decay, in the sphere and in sphere plus box, must agree:
#   Lu-177 (conversion electrons, Auger electrons, 113 and 208 keV gammas),
#   Pb-212 with its chain (Bi-212, Po-212, Tl-208; 2.6 MeV gamma).
# Lu-177 is also run with DeexciteDaughters off, which must fall well short of Geant4 (the
# de-excitation products were missing before 1.2.0: 14% in the sphere, 26% in total).
#
# Usage:
#   tests/regtests/validate_deexcitation_energy.sh /path/to/topas

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

WORK="$(mktemp -d /tmp/tsrts_deex_energy.XXXX)"
trap 'rm -rf "${WORK}"' EXIT
cd "${WORK}"

HISTORIES=4000

cat > geometry.txt <<'DECK'
s:Ge/World/Material = "G4_Galactic"
d:Ge/World/HLX = 40 cm
d:Ge/World/HLY = 40 cm
d:Ge/World/HLZ = 40 cm
s:Ge/Big/Type = "TsBox"
s:Ge/Big/Parent = "World"
s:Ge/Big/Material = "G4_WATER"
d:Ge/Big/HLX = 30 cm
d:Ge/Big/HLY = 30 cm
d:Ge/Big/HLZ = 30 cm
s:Ge/Local/Type = "TsSphere"
s:Ge/Local/Parent = "Big"
s:Ge/Local/Material = "G4_WATER"
d:Ge/Local/RMax = 0.5 mm
s:Sc/ELocal/Quantity = "EnergyDeposit"
s:Sc/ELocal/Component = "Local"
s:Sc/ELocal/OutputType = "csv"
s:Sc/ELocal/OutputFile = "ELocal"
s:Sc/ELocal/IfOutputFileAlreadyExists = "Overwrite"
s:Sc/EBig/Quantity = "EnergyDeposit"
s:Sc/EBig/Component = "Big"
s:Sc/EBig/OutputType = "csv"
s:Sc/EBig/OutputFile = "EBig"
s:Sc/EBig/IfOutputFileAlreadyExists = "Overwrite"
i:Ts/NumberOfThreads = 2
i:Ts/Seed = 5
b:Ts/PauseBeforeQuit = "False"
DECK

# $1 run name, $2 ion, $3 DeexciteDaughters (True/False)
write_rpt_deck() {
  cat > "$1.txt" <<DECK
includeFile = ../geometry.txt
sv:Ph/Default/Modules = 2 "g4em-standard_opt4" "tsrts_deexcitation"
b:Ph/Default/Fluorescence = "True"
b:Ph/Default/Auger = "True"
b:Ph/Default/DeexcitationIgnoreCut = "True"
s:So/Src/Type = "RadioactiveTimeSource"
s:So/Src/Mode = "uniform"
s:So/Src/Component = "Local"
s:So/Src/BeamParticle = "$2"
b:So/Src/CorrectByNumberOfHistories = "True"
b:So/Src/IncludeWholeDecayChain = "True"
b:So/Src/TreatAdditionalDecaysAsNewHistories = "False"
b:So/Src/WriteIsotopicAbundance = "False"
b:So/Src/WriteRunMetadata = "True"
s:So/Src/RunMetadataFileName = "meta.json"
b:So/Src/DeexciteDaughters = "$3"
u:So/Src/InitialActivity = 1000000
i:So/Src/NumberOfHistoriesInRun = ${HISTORIES}
d:Tf/TimelineStart = 0 s
d:Tf/TimelineEnd = 2592000 s
i:Tf/NumberOfSequentialTimes = 1
DECK
}

# $1 run name, $2 ion
write_ref_deck() {
  cat > "$1.txt" <<DECK
includeFile = ../geometry.txt
sv:Ph/Default/Modules = 2 "g4em-standard_opt4" "g4radioactivedecay"
s:So/Src/Type = "Volumetric"
s:So/Src/Component = "Local"
s:So/Src/ActiveMaterial = "G4_WATER"
b:So/Src/RecursivelyIncludeChildren = "True"
s:So/Src/BeamParticle = "$2"
d:So/Src/BeamEnergy = 0 MeV
u:So/Src/BeamEnergySpread = 0
i:So/Src/NumberOfHistoriesInRun = ${HISTORIES}
DECK
}

run() {
  mkdir -p "$1"
  mv "$1.txt" "$1/deck.txt"
  (cd "$1" && "${TOPAS_BIN}" deck.txt > run.log 2>&1) \
    || { echo "FAIL: run $1 exited non-zero"; tail -20 "$1/run.log"; exit 1; }
}

sum() { grep -v '^#' "$1" | head -1 | tr -d ' '; }

# energy per decay in MeV: "<sphere> <sphere + box>" (the box scorer leaves out the sphere inside it)
per_decay_rpt() {
  local n
  n="$(jq '[.entries[].n_decays_step] | add' "$1/meta.json")"
  awk -v l="$(sum "$1/ELocal.csv")" -v t="$(sum "$1/EBig.csv")" -v n="${n}" \
    'BEGIN { printf "%.6g %.6g\n", l / n, (l + t) / n }'
}
per_decay_ref() {
  awk -v l="$(sum "$1/ELocal.csv")" -v t="$(sum "$1/EBig.csv")" -v n="${HISTORIES}" \
    'BEGIN { printf "%.6g %.6g\n", l / n, (l + t) / n }'
}

FAILED=0
# $1 label, $2 rpt run, $3 ref run, $4 max |rel diff| in the sphere, $5 in total
compare() {
  read -r rl rt < <(per_decay_rpt "$2")
  read -r gl gt < <(per_decay_ref "$3")
  read -r dl dt < <(awk -v rl="${rl}" -v rt="${rt}" -v gl="${gl}" -v gt="${gt}" \
    'BEGIN { printf "%.4f %.4f\n", rl / gl - 1, rt / gt - 1 }')
  echo "$1: sphere ${rl} vs ${gl} MeV/decay (${dl}), total ${rt} vs ${gt} MeV/decay (${dt})"
  if awk -v d="${dl}" -v e="${dt}" -v a="$4" -v b="$5" \
       'BEGIN { exit !((d < 0 ? -d : d) <= a && (e < 0 ? -e : e) <= b) }'; then
    echo "PASS: $1 within ${4} (sphere) and ${5} (total) of g4radioactivedecay"
  else
    echo "FAIL: $1 outside ${4} (sphere) or ${5} (total) of g4radioactivedecay"
    FAILED=1
  fi
}

write_rpt_deck lu_on "GenericIon(71, 177)" True;  run lu_on
write_rpt_deck lu_off "GenericIon(71, 177)" False; run lu_off
write_ref_deck lu_ref "GenericIon(71, 177)";       run lu_ref
write_rpt_deck pb_on "GenericIon(82, 212)" True;  run pb_on
write_ref_deck pb_ref "GenericIon(82, 212)";       run pb_ref

compare "Lu-177, DeexciteDaughters" lu_on lu_ref 0.04 0.03
compare "Pb-212 chain, DeexciteDaughters" pb_on pb_ref 0.04 0.03

# Without the flag the total must miss at least 15% (26% measured): the test can see the gap.
read -r _ ot < <(per_decay_rpt lu_off)
read -r _ gt < <(per_decay_ref lu_ref)
if awk -v o="${ot}" -v g="${gt}" 'BEGIN { exit !(o / g - 1 < -0.15) }'; then
  echo "PASS: Lu-177 without DeexciteDaughters falls short of g4radioactivedecay (${ot} vs ${gt} MeV/decay)"
else
  echo "FAIL: Lu-177 without DeexciteDaughters is not short of g4radioactivedecay (${ot} vs ${gt} MeV/decay)"
  FAILED=1
fi

exit "${FAILED}"
