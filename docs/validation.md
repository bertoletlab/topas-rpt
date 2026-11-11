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
- **Restart from an abundance file** (`validate_abundance_restart.sh`) - a
  run restarted with `ReadIsotopicAbundanceFromFile` gives the same first-step
  decays as the uninterrupted run, with or without
  `UseFractionOfActivityFromFile`, and writes a `FractionOfDecaysAtTime0` column
  that starts at 1.
- **Activity consistency** (`validate_activity_consistency.sh`) -
  `invitro_bind` warns when `InitialActivity` does not match
  `ModeParams/Activity_permL` times the medium volume (x1000, /1000, x8 slips),
  stops with `"Error"`, stays silent with `"Off"`, and leaves the dose
  unchanged.
- **Analytic abundances** (`validate_analytic_abundance.sh`) - with
  `AbundanceUpdate = "Analytic"`, Pb-212, Bi-212 and Tl-208 follow the Bateman
  solution, the abundance file is identical for different seeds and thread
  counts, and an unknown value stops the run.
- **Daughter de-excitation** (`validate_deexcite_daughters.sh`) - with
  `DeexciteDaughters` and the `tsrts_deexcitation` module, a Lu-177 source
  emits the Hf-177 208 keV gamma, K conversion electrons and K x-rays at their
  ICRP 107 yields; without the flag it emits none; the flag without the module
  stops with `TsRADIOACTIVE020`.
- **Energy per decay** (`validate_deexcitation_energy.sh`) - a source in a
  0.5 mm water sphere inside a 60 cm water box deposits, per decay, the same
  energy in the sphere (within 4%) and in total (within 3%) as Geant4's own
  radioactive decay (`g4radioactivedecay`), for Lu-177 and for Pb-212 with its
  chain; Lu-177 without `DeexciteDaughters` must fall at least 15% short in
  total (26% measured).
- **First-forbidden beta shape** (`validate_first_forbidden_shape.sh`) - with
  `FirstForbiddenBetaShape = "allowed"`, the Tb-161 mean beta energy comes to
  ICRP 107, Lu-177 is unchanged, and an unknown value stops the run.
- **No stale vacancy** (`validate_no_stale_vacancy.sh`) - a Tc-99m source in
  water emits the same Tc K x-ray yield as in vacuum.
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
