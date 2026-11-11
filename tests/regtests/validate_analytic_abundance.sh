#!/usr/bin/env bash
set -euo pipefail

# AbundanceUpdate = "Analytic": the isotopic abundances follow the Bateman solution of the decay
# chain instead of the decays sampled by the histories.
#
# Runs Pb-212 with only 20 histories over 24 one-hour steps (the sampled bookkeeping is noisy
# there) and checks that
#   - Pb-212, Bi-212 and Tl-208 follow the Bateman solution (half-lives 10.622 h, 60.55 min,
#     3.053 min as in the Geant4 data; Bi-212 to Tl-208 branch 35.94%),
#   - the abundance file is identical for two seeds and for 1 and 4 threads,
#   - an unknown AbundanceUpdate value stops the run.
#
# Usage:
#   tests/regtests/validate_analytic_abundance.sh /path/to/topas

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

RUN_DIR="$(mktemp -d "${TMPDIR:-/tmp}/tsrts_analytic.XXXXXX")"

write_deck() {
  local seed="$1" threads="$2" update="$3"
  cat <<DECK
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
s:So/ChainSource/Mode                                 = "uniform"
s:So/ChainSource/Type                                 = "RadioactiveTimeSource"
s:So/ChainSource/Component                            = "Seed"
s:So/ChainSource/BeamParticle                         = "GenericIon(82, 212)"
b:So/ChainSource/CorrectByNumberOfHistories           = "True"
b:So/ChainSource/IncludeWholeDecayChain               = "True"
s:So/ChainSource/AbundanceUpdate                      = "${update}"
b:So/ChainSource/WriteIsotopicAbundance               = "True"
s:So/ChainSource/WriteIsotopicAbundanceFileName       = "iso.csv"
u:So/ChainSource/InitialActivity                      = 1000000
i:So/ChainSource/NumberOfHistoriesInRun               = 20
d:Tf/TimelineStart           = 0 s
d:Tf/TimelineEnd             = 86400 s
i:Tf/NumberOfSequentialTimes = 24
i:Ts/NumberOfThreads = ${threads}
i:Ts/Seed            = ${seed}
DECK
}

for cfg in "s11:11:1:Analytic" "s12:12:1:Analytic" "t4:11:4:Analytic" "bad:11:1:Exact"; do
  IFS=: read -r name seed threads update <<<"${cfg}"
  mkdir -p "${RUN_DIR}/${name}"
  write_deck "${seed}" "${threads}" "${update}" > "${RUN_DIR}/${name}/deck.txt"
  set +e
  (cd "${RUN_DIR}/${name}" && "${TOPAS_BIN}" deck.txt > run.log 2>&1)
  rc=$?
  set -e
  if [[ "${name}" == "bad" ]]; then
    [[ "${rc}" -ne 0 ]] || { echo "FAIL: AbundanceUpdate = \"Exact\" did not stop the run"; exit 1; }
  else
    [[ "${rc}" -eq 0 ]] || { echo "ERROR: TOPAS run failed (${name}); see ${RUN_DIR}/${name}/run.log"; exit 1; }
    [[ -f "${RUN_DIR}/${name}/iso.csv" ]] || { echo "ERROR: missing ${RUN_DIR}/${name}/iso.csv"; exit 1; }
  fi
done

fail=0
if ! cmp -s "${RUN_DIR}/s11/iso.csv" "${RUN_DIR}/s12/iso.csv"; then
  echo "FAIL: abundance file differs between seeds"; fail=1
fi
if ! cmp -s "${RUN_DIR}/s11/iso.csv" "${RUN_DIR}/t4/iso.csv"; then
  echo "FAIL: abundance file differs between 1 and 4 threads"; fail=1
fi

if ! awk -F, '
    NR == 1 { for (i = 1; i <= NF; i++) col[$i] = i; next }
    {
      t = $1; lp = log(2) / (10.622 * 3600); lb = log(2) / (60.55 * 60); lt = log(2) / (3.053 * 60); br = 0.3594
      pb = exp(-lp * t)
      bi = lp / (lb - lp) * (exp(-lp * t) - exp(-lb * t))
      tl = lp * lb * br * (exp(-lp * t) / ((lb - lp) * (lt - lp)) + exp(-lb * t) / ((lp - lb) * (lt - lb)) + exp(-lt * t) / ((lp - lt) * (lb - lt)))
      check("Pb212", pb, 2e-4); check("Bi212", bi, 2e-3); check("Tl208", tl, 5e-3)
    }
    function check(name, ref, tol,   v, r) {
      if (!(name in col)) { if (t > 0) { printf("FAIL: no %s column\n", name); bad = 1 }; return }
      v = $(col[name]); if (ref < 1e-12) return
      r = (v - ref) / ref; if (r < 0) r = -r
      if (r > tol) { printf("FAIL: %s at t=%s s is %s, Bateman %.6g (rel. %.2g > %.2g)\n", name, t, v, ref, r, tol); bad = 1 }
    }
    END { exit bad }' "${RUN_DIR}/s11/iso.csv"; then
  fail=1
fi

if [[ "${fail}" -ne 0 ]]; then
  echo "Analytic abundance check failed (${RUN_DIR})."
  exit 1
fi
echo "PASS: analytic abundances follow Bateman and do not depend on seed or threads (${RUN_DIR})"
