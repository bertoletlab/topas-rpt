#!/usr/bin/env bash
set -euo pipefail

# invitro_bind takes two activity inputs: InitialActivity (total Bq; sets the decays and the dose)
# and ModeParams/Activity_permL (Bq/mL; sets only the binding kinetics). The source compares
# InitialActivity with Activity_permL x medium volume at start-up (warning TsRADIOACTIVEBIND001).
#
# Medium 1 x 1 x 0.08 mm = 0.08 uL at 20 kBq/mL, so the implied InitialActivity is 1.6 Bq
# (about 1.59 Bq without the cell volume). Checks that
#   - 1.6 Bq and the liquid-volume value 1.55 Bq run without the warning,
#   - 1600 Bq (x1000), 0.0016 Bq (/1000) and 12.8 Bq (x8, a half-length slip) warn once each,
#   - "Error" stops the run, "Off" silences the warning,
#   - the warning appears once with 4 threads,
#   - the run metadata records the implied activity,
#   - the dose is unchanged by the check (Warn vs Off at the same InitialActivity).
#
# Usage:
#   tests/regtests/validate_activity_consistency.sh /path/to/topas

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

RUN_DIR="$(mktemp -d "${TMPDIR:-/tmp}/tsrts_activity_check.XXXXXX")"

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BASE_DECK="${ROOT}/examples/canonical/invitro_bind_minimal.txt"

# The canonical invitro_bind deck with InitialActivity replaced, fewer histories, metadata and a
# dose scorer, plus any extra lines.
write_deck() {
  local activity="$1" extra="$2"
  grep -vE '^[a-z]+:(So/BindSource/(InitialActivity|NumberOfHistoriesInRun|WriteRunMetadata|RunMetadataFileName)|Ts/NumberOfThreads) *=' "${BASE_DECK}"
  cat <<DECK
u:So/BindSource/InitialActivity          = ${activity}
i:So/BindSource/NumberOfHistoriesInRun   = 20
b:So/BindSource/WriteRunMetadata         = "True"
s:So/BindSource/RunMetadataFileName      = "meta.json"
s:Sc/Dose/Quantity                  = "DoseInVitroRPT"
s:Sc/Dose/Component                 = "Cells"
s:Sc/Dose/OutputFile                = "dose"
b:Sc/Dose/DosePerParticle           = "False"
b:Sc/Dose/DosePerInteraction        = "False"
s:Sc/Dose/OutputType                = "ASCII"
s:Sc/Dose/IfOutputFileAlreadyExists = "Overwrite"
b:Sc/Dose/OutputAfterRun            = "False"
${extra}
DECK
}

# name|InitialActivity|extra lines|expected exit (0 or nonzero)|expected warnings
cases=(
  "consistent|1.6||0|0"
  "liquid|1.55||0|0"
  "x1000|1600||0|1"
  "div1000|0.0016||0|1"
  "x8|12.8||0|1"
  "error|1600|s:So/BindSource/ModeParams/ActivityConsistencyCheck = \"Error\"|nonzero|1"
  "off|1600|s:So/BindSource/ModeParams/ActivityConsistencyCheck = \"Off\"|0|0"
  "threads4|1600|i:Ts/NumberOfThreads = 4|0|1"
)

fail=0
for c in "${cases[@]}"; do
  IFS='|' read -r name activity extra want_exit want_warn <<<"${c}"
  mkdir -p "${RUN_DIR}/${name}"
  extra_lines="${extra}"
  [[ "${extra}" == *NumberOfThreads* ]] || extra_lines="${extra}"$'\n'"i:Ts/NumberOfThreads = 1"
  write_deck "${activity}" "${extra_lines}" > "${RUN_DIR}/${name}/deck.txt"
  set +e
  (cd "${RUN_DIR}/${name}" && "${TOPAS_BIN}" deck.txt > run.log 2>&1)
  rc=$?
  set -e
  warnings="$(grep -c 'TsRADIOACTIVEBIND001' "${RUN_DIR}/${name}/run.log" || true)"
  if [[ "${want_exit}" == "0" && "${rc}" -ne 0 ]]; then
    echo "FAIL: ${name}: exit ${rc}, expected 0 (see ${RUN_DIR}/${name}/run.log)"; fail=1
  elif [[ "${want_exit}" == "nonzero" && "${rc}" -eq 0 ]]; then
    echo "FAIL: ${name}: run completed, expected the check to stop it"; fail=1
  fi
  if [[ "${warnings}" -lt "${want_warn}" || ( "${want_warn}" == "0" && "${warnings}" -ne 0 ) ]]; then
    echo "FAIL: ${name}: ${warnings} TsRADIOACTIVEBIND001 message(s), expected ${want_warn}"; fail=1
  fi
  if [[ "${want_warn}" == "1" && "${want_exit}" == "0" && "${warnings}" -ne 1 ]]; then
    echo "FAIL: ${name}: warning printed ${warnings} times, expected once"; fail=1
  fi
done

# Metadata records the implied activity (1.6 Bq for the envelope).
implied="$(jq '.mode_settings.implied_initial_activity_bq' "${RUN_DIR}/consistent/meta.json")"
if ! awk -v v="${implied}" 'BEGIN { exit !(v > 1.599 && v < 1.601) }'; then
  echo "FAIL: implied_initial_activity_bq is ${implied}, expected 1.6"; fail=1
fi

# The check does not change the dose: Warn and Off runs at 1600 Bq give identical dose files.
if ! cmp -s "${RUN_DIR}/x1000/dose.phsp" "${RUN_DIR}/off/dose.phsp"; then
  echo "FAIL: dose differs between Warn and Off at the same InitialActivity"; fail=1
fi

if [[ "${fail}" -ne 0 ]]; then
  echo "Activity consistency check failed (${RUN_DIR})."
  exit 1
fi
echo "PASS: InitialActivity is checked against Activity_permL x medium volume (${RUN_DIR})"
