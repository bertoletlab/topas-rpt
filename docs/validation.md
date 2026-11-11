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
- **Activity consistency** (`validate_activity_consistency.sh`) -
  `invitro_bind` warns when `InitialActivity` does not match
  `ModeParams/Activity_permL` times the medium volume (x1000, /1000, x8 slips),
  stops with `"Error"`, stays silent with `"Off"`, and leaves the dose
  unchanged.
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

`.github/workflows/build.yml` builds Geant4 11.2.2 and OpenTOPAS 4.2.3 from
source (with the two patches in `patches/` applied), links this extension
against them, and runs the suite above on every push and pull request.
