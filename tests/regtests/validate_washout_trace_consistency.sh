#!/usr/bin/env bash
set -euo pipefail

# Washout contract for invitro_bind:
# - Saving the probability trace (SaveProbabilityDistribution) must not change the run.
# - A washout on a step boundary must already be applied in the state of the step that starts there.
# - The per-step atom count must follow the trace's FracRemaining.
#
# Usage:
#   tests/regtests/validate_washout_trace_consistency.sh /path/to/topas

if [[ $# -ne 1 ]]; then
  echo "Usage: $0 /path/to/topas"
  exit 2
fi
TOPAS_BIN="$1"

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
DECK="${ROOT}/AuxFiles/regtests/param/bind_washout_boundary.txt"
OUT="$(mktemp -d /tmp/tsrts_washout_XXXXXX)"

if [[ -z "${TOPAS_G4_DATA_DIR:-}" && -z "${G4LEVELGAMMADATA:-}" ]]; then
  if [[ -f "/Applications/GEANT4/geant4-install/bin/geant4.sh" ]]; then
    # shellcheck source=/dev/null
    source "/Applications/GEANT4/geant4-install/bin/geant4.sh"
  elif [[ -d "/Applications/GEANT4/G4DATA" ]]; then
    export TOPAS_G4_DATA_DIR="/Applications/GEANT4/G4DATA"
  fi
fi

for mode in on off; do
  mkdir -p "${OUT}/${mode}"
  flag="True"; [[ "${mode}" == "off" ]] && flag="False"
  sed "s/SaveProbabilityDistribution = \"True\"/SaveProbabilityDistribution = \"${flag}\"/" "${DECK}" > "${OUT}/${mode}/deck.txt"
  ( cd "${OUT}/${mode}" && "${TOPAS_BIN}" deck.txt > run.log 2>&1 )
done

python3 - "${OUT}" <<'PY'
import json, re, sys
out = sys.argv[1]
fail = []
on = json.load(open(f"{out}/on/run_metadata.json"))["entries"]
off = json.load(open(f"{out}/off/run_metadata.json"))["entries"]
keys = ("n_decays_step", "n_histories_step", "primary_weight")
for a, b in zip(on, off):
    for k in keys:
        if abs(a[k] - b[k]) > 1e-9 * max(abs(a[k]), 1.0):
            fail.append(f"trace on/off differ in run {a['run_id']} {k}: {a[k]} vs {b[k]}")

log = open(f"{out}/on/run.log").read()
n0 = float(re.search(r"Initial number of atoms: ([0-9.eE+-]+)", log).group(1))
atoms = {}
for t, n in re.findall(r"Timeinit: ([0-9.eE+-]+) h\s*\n.*?TimeEnd:.*?\n.*?Number of atoms ([0-9.eE+-]+)", log):
    atoms.setdefault(float(t), float(n))
trace = {}
for line in open(f"{out}/on/prob_washout_boundary.txt").read().splitlines()[1:]:
    c = line.split()
    trace[round(float(c[0]), 3)] = float(c[-1])
washout_h = 1.0
if washout_h not in atoms:
    fail.append("no step starts at the washout time")
else:
    frac = trace.get(washout_h * 3600.0)
    if frac is None or abs(atoms[washout_h] / n0 - frac) > 1e-4 * frac:
        fail.append(f"atoms at the washout step {atoms[washout_h] / n0} != trace FracRemaining {frac}")
    if not frac or frac > 0.5:
        fail.append(f"washout not applied at the step boundary (FracRemaining {frac})")
post = [e for e in on if e["time_start_s"] >= washout_h * 3600.0]
if not post or any(e["n_histories_step"] <= 0 for e in post):
    fail.append("no post-washout histories were sampled")

if fail:
    print("\n".join("FAIL: " + f for f in fail))
    sys.exit(1)
print(f"PASS: washout trace consistency ({out})")
PY
