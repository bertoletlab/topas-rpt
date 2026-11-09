#!/usr/bin/env bash
set -euo pipefail

# Manual binding model contract for invitro_bind:
# - BindingModel and ManualBindingProbabilities are read from ModeParams (no legacy keys here).
# - Every step uses the manual split, not only the first one (the split was lost after step 1
#   and all later decays were placed in the medium).
# - A washout empties the medium and degraded fractions; the other fractions are renormalised
#   and the remaining activity drops by the washed-out share.
#
# Deck: Lu-177, split 0.10/0.70/0.10/0.05/0.05 (E/M/I/N/D), three 1 h steps, washout at 1.5 h.
# Each step uses the state at its start, so the washout shows from the third step on.
#
# Usage:
#   tests/regtests/validate_manual_model.sh /path/to/topas

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

OUT="$(mktemp -d "${TMPDIR:-/tmp}/tsrts_manual.XXXXXX")"
cat > "${OUT}/deck.txt" <<'DECK'
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
s:So/Bind/Mode      = "invitro_bind"
s:So/Bind/Component = "Cells"
s:So/Bind/BeamParticle = "GenericIon(71, 177)"
u:So/Bind/InitialActivity = 1000000
i:So/Bind/NumberOfHistoriesInRun = 100
b:So/Bind/WriteIsotopicAbundance = "True"
s:So/Bind/WriteIsotopicAbundanceFileName = "iso_manual_washout.csv"
u:So/Bind/ModeParams/Activity_permL = 20000
u:So/Bind/ModeParams/SpecificActivity_pernmol = 25000000
s:So/Bind/ModeParams/BindingModel = "Manual"
uv:So/Bind/ModeParams/ManualBindingProbabilities = 5 10 70 10 5 5
d:So/Bind/ModeParams/WashoutTime = 5400 s
d:Tf/TimelineStart = 0 s
d:Tf/TimelineEnd   = 10800 s
i:Tf/NumberOfSequentialTimes = 3
i:Ts/NumberOfThreads = 1
i:Ts/Seed = 1
DECK

( cd "${OUT}" && "${TOPAS_BIN}" deck.txt > run.log 2>&1 ) || {
  echo "FAIL: Manual model run with ModeParams/BindingModel exited with an error; see ${OUT}/run.log"
  exit 1
}

python3 - "${OUT}/run.log" <<'PY'
import re, sys
text = open(sys.argv[1]).read()
names = ["Medium", "Membrane", "Cytoplasm", "Nucleus", "Degraded"]
# Each new step prints "Timeinit: <h> h", the atom count and the five probabilities.
blocks = re.findall(r"Timeinit: ([0-9.eE+-]+) h.*?Number of atoms ([0-9.eE+-]+)(.*?)-{10,}", text, re.S)
steps = {}
for t, atoms, body in blocks:
    probs = [float(re.search(rf"{n} probability: ([0-9.eE+-]+)", body).group(1)) for n in names]
    steps[round(float(t), 6)] = (float(atoms), probs)
manual = [0.10, 0.70, 0.10, 0.05, 0.05]
washed = [0.0, 0.70 / 0.85, 0.10 / 0.85, 0.05 / 0.85, 0.0]
fail = []
for t, expected in ((1.0, manual), (2.0, washed)):
    if t not in steps:
        fail.append(f"no step starting at {t} h in the log")
        continue
    got = steps[t][1]
    for n, g, e in zip(names, got, expected):
        if abs(g - e) > 1e-6:
            fail.append(f"step at {t} h: {n} probability {g}, expected {e:.6f}")
if 1.0 in steps and 2.0 in steps:
    ratio = steps[2.0][0] / steps[1.0][0]
    # Lu-177 decay over 1 h is below 0.6%; the washout removes 15% of the activity.
    if not 0.84 < ratio < 0.86:
        fail.append(f"atom count ratio across the washout is {ratio:.4f}, expected about 0.845")
if fail:
    print("\n".join("FAIL: " + f for f in fail))
    sys.exit(1)
PY
echo "PASS: Manual model keeps its split every step and applies the washout (${OUT})"
