#!/usr/bin/env bash
set -euo pipefail

# Washout contract for invitro_bind:
# - Saving the probability trace (SaveProbabilityDistribution) must not change the run.
# - A washout on a step boundary must already be applied in the state of the step that starts there.
# - The per-step atom count must follow the trace's FracRemaining.
# - With several washouts (WashoutTimes), each one must empty the medium.
#
# Usage:
#   tests/regtests/validate_washout_trace_consistency.sh /path/to/topas

if [[ $# -ne 1 ]]; then
  echo "Usage: $0 /path/to/topas"
  exit 2
fi
TOPAS_BIN="$1"

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
OUT="$(mktemp -d /tmp/tsrts_washout_XXXXXX)"

if [[ -z "${TOPAS_G4_DATA_DIR:-}" && -z "${G4LEVELGAMMADATA:-}" ]]; then
  if [[ -f "/Applications/GEANT4/geant4-install/bin/geant4.sh" ]]; then
    # shellcheck source=/dev/null
    source "/Applications/GEANT4/geant4-install/bin/geant4.sh"
  elif [[ -d "/Applications/GEANT4/G4DATA" ]]; then
    export TOPAS_G4_DATA_DIR="/Applications/GEANT4/G4DATA"
  fi
fi

# deck name -> washout times in hours (as set in the deck)
DECKS=("bind_washout_boundary:1" "bind_washout_multiple:1,2.5,3")

for entry in "${DECKS[@]}"; do
  name="${entry%%:*}"
  for mode in on off; do
    mkdir -p "${OUT}/${name}/${mode}"
    flag="True"; [[ "${mode}" == "off" ]] && flag="False"
    sed "s/SaveProbabilityDistribution = \"True\"/SaveProbabilityDistribution = \"${flag}\"/" \
      "${ROOT}/AuxFiles/regtests/param/${name}.txt" > "${OUT}/${name}/${mode}/deck.txt"
    ( cd "${OUT}/${name}/${mode}" && "${TOPAS_BIN}" deck.txt > run.log 2>&1 )
  done
done

python3 - "${OUT}" "${DECKS[@]}" <<'PY'
import json, re, sys
out = sys.argv[1]
fail = []
for entry in sys.argv[2:]:
    name, hours = entry.split(":")
    washouts = [float(h) * 3600.0 for h in hours.split(",")]
    d = f"{out}/{name}"
    on = json.load(open(f"{d}/on/run_metadata.json"))["entries"]
    off = json.load(open(f"{d}/off/run_metadata.json"))["entries"]
    for a, b in zip(on, off):
        for k in ("n_decays_step", "n_histories_step", "primary_weight"):
            if abs(a[k] - b[k]) > 1e-9 * max(abs(a[k]), 1.0):
                fail.append(f"{name}: trace on/off differ in run {a['run_id']} {k}: {a[k]} vs {b[k]}")

    # Trace rows: time, E, ..., FracRemaining. Keep the last row written at each time.
    rows = {}
    order = []
    for line in open(f"{d}/on/prob_{name[len('bind_'):]}.txt").read().splitlines()[1:]:
        c = [float(x) for x in line.split()]
        t = round(c[0], 3)
        if t not in rows: order.append(t)
        rows[t] = c
    for w in washouts:
        if w not in rows:
            fail.append(f"{name}: no trace row at washout {w} s")
            continue
        if rows[w][1] != 0.0:
            fail.append(f"{name}: medium not emptied at washout {w} s (E = {rows[w][1]})")
        prev = order[order.index(w) - 1]
        if not rows[w][-1] < rows[prev][-1]:
            fail.append(f"{name}: FracRemaining did not drop at washout {w} s")

    # Every step after the first must start from the trace state at its start time.
    log = open(f"{d}/on/run.log").read()
    n0 = float(re.search(r"Initial number of atoms: ([0-9.eE+-]+)", log).group(1))
    atoms = {}
    for t, n in re.findall(r"Timeinit: ([0-9.eE+-]+) h\s*\n.*?TimeEnd:.*?\n.*?Number of atoms ([0-9.eE+-]+)", log):
        atoms.setdefault(round(float(t) * 3600.0, 3), float(n))
    for t, n in atoms.items():
        frac = rows.get(t, [None])[-1]
        if frac is None or abs(n / n0 - frac) > 1e-4 * frac:
            fail.append(f"{name}: atoms at step start {t} s ({n / n0}) != trace FracRemaining {frac}")

    # The step that starts at (or contains) the first washout keeps enough activity to be sampled.
    post = [e for e in on if e["time_end_s"] > washouts[0]]
    if not post or post[0]["n_histories_step"] <= 0:
        fail.append(f"{name}: no post-washout histories were sampled")

if fail:
    print("\n".join("FAIL: " + f for f in fail))
    sys.exit(1)
print(f"PASS: washout trace consistency ({out})")
PY
