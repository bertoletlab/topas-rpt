# Validation

```bash
tests/regtests/validate_all_features_vs_legacy.sh /path/to/topas
```

Requires `ripgrep` on `PATH` - without it, some steps that grep run output
fail closed with a misleading `FAIL` message that obscures the real cause, so
install it first if a step fails unexpectedly.

## What each step checks

- **Legacy parity** - a deterministic smoke run's numeric output matches a
  frozen baseline (SHA256-compared).
- **Determinism** - two runs with an identical seed produce identical output.
- **Chain-control contract** - `IncludeWholeDecayChain` and
  `TreatAdditionalDecaysAsNewHistories` actually change chain behavior the way
  their names imply.
- **Metadata schema** - every run's `run_metadata.json` matches the documented
  schema.
- **Zero-history steps** (`validate_zero_history_steps.sh`) - with
  `CorrectByNumberOfHistories`, every step that still has decays runs at least
  one history, so the decay chain keeps evolving once the scaled history count
  would round to zero.
- **`activity_map` contract** - optional, skips cleanly without
  `ACTIVITYMAP_CT_DICOM_DIR`/`ACTIVITYMAP_NM_DICOM_DIR` set to real DICOM
  data.

## Parameterized regression tests

```bash
scripts/run_regtests_with_built_topas.sh /path/to/topas
```

Runs a handful of hand-written parameter files covering the different
`invitro_bind` compartment configurations against a built binary, checking
exit codes and the absence of fatal errors.

## Continuous integration

`.github/workflows/build.yml` builds Geant4 11.3.2 and OpenTOPAS 4.3.0 from
source (with the DICOM activity-map patch in `patches/` applied), links this
extension against them, and runs the suite above on Ubuntu 24.04 and 26.04 on
every push and pull request.
