# Changelog

All notable changes to `topas-rpt` are documented here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and
[Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- `So/<Source>/DeexciteDaughters` (default False) and `So/<Source>/IsomerLifetimeThreshold`
  (default 1 min, mean life): the excited daughter nucleus of every sampled decay is
  cascaded through `G4ITDecay` (photon evaporation, internal conversion, atomic
  relaxation) to its ground state or to a longer-lived isomer, and every gamma,
  conversion electron, x-ray and Auger electron is emitted as a primary. Applied to the
  first decay and to every recursive chain decay.
- `tsrts_deexcitation` physics module (`TsDeexcitationPhysics`): the nuclear
  de-excitation settings of `G4RadioactiveDecayPhysics` (internal-conversion shell data,
  isomer production, Auger, de-excitation below cuts) without `G4Radioactivation`.
- `So/<Source>/FirstForbiddenBetaShape` (`"geant4"`, default, or `"allowed"`): with
  `"allowed"`, every beta branch the decay data tag `firstForbidden` is rebuilt as an
  allowed transition with its branching ratio, endpoint and daughter level. Geant4 11.2
  (`G4BetaDecayCorrections::ShapeFactor`) shapes those branches with a fit to 210Bi data
  that its own comment marks as not valid for other nuclides; it puts the mean beta energy
  of 161Tb at 0.91 and of 169Er at 0.94 of ICRP 107, which treats non-unique
  first-forbidden transitions as allowed. With `"allowed"` they come to 1.010 and 1.018.
  Nuclides without such a branch are unchanged.
- `tests/regtests/validate_deexcite_daughters.sh`, `tests/regtests/validate_first_forbidden_shape.sh`,
  `tests/regtests/validate_no_stale_vacancy.sh`.
- `tests/regtests/validate_deexcitation_energy.sh`: energy per decay in a 0.5 mm
  water sphere and in total against `g4radioactivedecay`, for Lu-177 and for Pb-212
  with its chain; Lu-177 without `DeexciteDaughters` must fall short. The four
  de-excitation tests run in the suite (`validate_all_features_vs_legacy.sh`), and
  the user guide has a "Daughter de-excitation" section.
- `tests/regtests/validate_deexcitation_recoil.sh` (Am-241 recoil with and without
  `DeexciteDaughters`) and `tests/regtests/validate_deexcitation_isomer_chain.sh`
  (Mo-99 to Tc-99m in the chain).
- `tests/regtests/validate_zero_history_steps.sh`: Pb-212 over 12 one-day
  steps with 200 histories; every step with decays runs a history and Pb-212
  keeps decaying.
- `tests/regtests/validate_abundance_restart.sh`.
- `invitro_bind`: `InitialActivity` (total Bq; sets the decays and the dose)
  is now compared at start-up with `ModeParams/Activity_permL` times the
  medium volume (Bq/mL; until now used only for the binding kinetics). Nothing
  tied the two together, so a unit slip in either one (uL for mL, kBq for Bq,
  a half-length entered as a length) changed the dose by that factor without
  any sign. A value outside the range from the liquid volume (medium minus
  cells) to the whole medium envelope, widened by 5%, gives warning
  `TsRADIOACTIVEBIND001` with both values and their ratio.
  `ModeParams/ActivityConsistencyCheck` = `"Warn"` (default), `"Error"` or
  `"Off"`; `ModeParams/ActivityConsistencyTolerance` sets the 5%. The run
  metadata records `implied_initial_activity_bq`,
  `implied_initial_activity_liquid_bq` and `activity_consistency_check`. The
  check changes no result. It flags the `invitro_bind` regression decks, whose activities are
  arbitrary test values.
- `s:So/<Source>/AbundanceUpdate = "Analytic"` (default `"MonteCarlo"`, the
  behaviour so far): the isotopic abundances advance each step with the Bateman
  solution of the decay chain, using the half-lives and branching ratios of the
  Geant4 decay data, instead of the decays sampled by the histories. The
  sampled bookkeeping gets noisy when a step has few histories (late steps of
  a `CorrectByNumberOfHistories` run, or a low `NumberOfHistoriesInRun`); the
  analytic update has no noise and does not depend on the seed, the thread
  count or the number of histories. It follows the rules of the sampled
  bookkeeping: excited daughters as their ground state, nuclides without a
  lifetime (or with a meanlife below 1e-6 of the step) passed straight to
  their daughters, daughters only with `IncludeWholeDecayChain`. Not available
  in `diffusion` mode, whose clearance and desorption are outside the decay
  chain. Doses still come from the histories; only the abundances, and so the
  decay count of each step, change.
- `tests/regtests/validate_analytic_abundance.sh`.
- `tests/regtests/validate_single_step_metadata.sh`.

### Changed
- CI and the documented requirements move to OpenTOPAS 4.3.0 and Geant4
  11.3.2, the pair the regression baselines were already generated with. CI
  runs on both Ubuntu 24.04 (GCC 13) and 26.04 (GCC 15). Geant4 dataset paths
  are read from `geant4-config --datasets` instead of being listed by version.
- `examples/canonical/invitro_bind_minimal.txt`: `InitialActivity` 5 -> 1.6 Bq,
  the activity of its 0.08 uL medium at 20 kBq/mL. The binding kinetics are
  unchanged (same concentration); doses scale by 0.32.
- `tests/regtests/validate_activity_consistency.sh`.
- `TsRadioactiveTimeGenerator` is now set up in two phases. Its constructor
  only sets defaults; `Initialize()`, run once the whole object exists, reads
  the parameters through the virtual `ResolveParameters()`, sets up the
  timeline, abundances, atom count and first step, calls the new
  `FinishInitialization()` hook, and writes the first run metadata. Before,
  all of this ran inside the base constructor, where virtual calls reach only
  the base class: `TsRadioactiveTimeGeneratorBind` read its parameters only
  afterwards, its `GetAdaptedSource()` never ran, and the first
  `run_metadata.json` had an empty `mode_settings`. Subclasses now pass
  `deferInitialization = true` to the base constructor and call `Initialize()`
  at the end of their own (`TsRadioactiveTimeGeneratorBind` offers the same
  protected constructor to its own subclasses), and `Initialize()` stops with an
  error if it runs twice, or if it never ran when the first run starts.
  Parameter files and results are unchanged, apart from the fix below; in
  `run.log`, the `invitro_bind` parameter summary now prints before the
  first-step summary.

### Removed
- `patches/0002-geant4-fix-columns-icc-typo.patch` and
  `patches/0003-opentopas-zero-ke-momentum.patch`: Geant4 11.3.2 and OpenTOPAS
  4.3.0 already contain both fixes. `0001` (DICOM activity map) still applies
  to 4.3.0 and is kept.
- `TsRadioactiveTimeGeneratorBind::GetAdaptedSource()` and its own `fSource`
  member, which hid the base class's `fSource` and were never used: the base
  constructor called the base version, which also sets up the source of the
  `invitro_bind` mode.

### Fixed
- Without `g4radioactivedecay`, sources emitted the daughter ion still excited and nothing
  de-excited it, so gammas, conversion electrons and x-rays were missing (177Lu: beta
  energy alone, 0.91 of the ICRP 107 non-penetrating energy, no photons). Adding
  `g4radioactivedecay` instead also decays radioactive ground-state daughters during
  tracking, which turns first-decay sources into chain sources (211At 1.85, 213Bi 9.2,
  212Pb 23 times the first-decay energy). `DeexciteDaughters` gives the first decay
  alone: within 2% of ICRP 107 for all 25 nuclides checked (161Tb and 169Er with
  `FirstForbiddenBetaShape = "allowed"`; 6-7% low without it), and within 1% of
  `g4radioactivedecay` where the daughter is stable. Opt-in, so existing decks are
  unchanged.
- `DeexciteDaughters`: the de-excitation products were created at rest, so a daughter
  left excited by an alpha decay lost its recoil (Am-241: 92.6 keV per decay without the
  flag, 0.35 keV with it). They now move with the recoiling nucleus, as in
  `G4VRadioactiveDecay`.
- `DeexciteDaughters` with `IncludeWholeDecayChain`: an isomer kept by the de-excitation
  (Tc-99m from Mo-99) was replaced by its ground state in the chain, so it never decayed
  (0.04 gammas of 140.5 keV per Mo-99 decay instead of 0.83). It now decays with its own
  lifetime and carries over between steps as its own abundance entry. The isomer cut
  reads the lifetime as the isotopic abundances do.
- `FirstForbiddenBetaShape = "allowed"` finds the decay data as Geant4 does
  (`G4FindDataDir`), matches the parent level within 10 eV and on its floating level,
  keys branches by daughter floating level, and warns (`TsRADIOACTIVE023`) when the file
  cannot be read instead of keeping the Geant4 shapes silently.
- `DeexciteDaughters` without the `tsrts_deexcitation` module now stops with
  `TsRADIOACTIVE020` on Geant4 11.3 as well. The check looked only at the
  internal-conversion data flag, which Geant4 11.3 turns on by default, so it never
  fired there, and runs went ahead without the module's isomer, Auger and
  stale-vacancy settings. It now also checks that the module is in
  `Ph/<ListName>/Modules`.
- With `tsrts_deexcitation`, a source in a finite phantom containing hydrogen emitted spurious
  daughter K x-rays and K Auger electrons. With the Auger cascade active,
  `G4UAtomicDeexcitation::GenerateParticles` (Geant4 11.2 and earlier) stores the vacancy before
  it returns for Z < 6, and nothing clears it; with `DeexcitationIgnoreCut`, Livermore Compton
  scattering on hydrogen makes that call. When the photon then escapes, the event ends and the
  next decay's atomic relaxation (`G4ITDecay`, `G4ECDecay`) processes the stale vacancy as a
  daughter K vacancy. 99mTc in a soft-tissue cube of 197.6 mm half-side emitted 0.117 Tc K x-rays per
  decay against 0.062 in vacuum, and 3.4% more electron energy per decay than in a 2 m medium;
  graphite, without hydrogen, showed nothing. The module now installs `TsAtomicDeexcitation`, a
  `G4UAtomicDeexcitation` that returns before storing the vacancy, in place of the one the EM
  module installed. The same stale vacancy also added a spurious K cascade of the next atom
  relaxed within the same event; that goes too. Geant4 11.3.0 clears the vacancy list at the
  start of every call, so the fix matters only with Geant4 11.2 (99mTc in water, 11.2.2: 0.178
  Tc K x-rays per decay without it, 0.073 with it and in vacuum; 11.3.2: 0.073 either way).
- `CorrectByNumberOfHistories = "True"`: once a step's scaled history count
  (the first step's count times its share of decays, truncated) reached 0, no
  history updated the isotopic abundances, so the decay chain froze and every
  later row of the abundance file repeated the same values. A step with decays
  now always runs at least one history, which carries the step's decays as its
  weight. Who should check their results: runs whose late steps had fewer
  decays than the first step divided by `NumberOfHistoriesInRun`, for their
  abundance files. Dose in those steps was 0 before (its share of the total
  is at most 1/`NumberOfHistoriesInRun` per step).
- Restart from an isotopic-abundance file (`ReadIsotopicAbundanceFromFile`):
  the `FractionOfDecaysAtTime0` column written by the restarted run was
  shifted by one row and ended in 0 without `UseFractionOfActivityFromFile`,
  and started from the file's value with it. Since the column undoes the
  normalisation of the scored quantities to the run's first step
  (`CorrectByNumberOfHistories = "False"`), it now starts at 1 in every run.
  With the flag, `InitialActivity` was also multiplied by the file's value and
  reported as "the actual activity used", although the decays never used it:
  the abundances in the file are fractions of the atoms at the original time 0
  and already carry the decay since then. `InitialActivity` now stays as set,
  and the notice reports the file's value and says that it changes neither the
  decays nor the column. Decay counts and doses are unchanged.
- `invitro_bind`: a single-step run wrote `run_metadata.json` with an empty
  `mode_settings` (no binding model, activities, rates or washout times).

## [1.1.0] - 2026-11-10

### Added
- `invitro_bind`: several washouts per run with
  `ModeParams/WashoutTimes` (a time vector), for repeated medium changes.
  Each one empties the medium and the degraded compartment at exactly its
  time. `WashoutTime` still sets a single washout; setting both is an error.
  The run metadata lists them as `washout_times_s`. Regression deck
  `bind_washout_multiple` (three washouts, two on step boundaries) is covered
  by `validate_washout_trace_consistency.sh`.
- Regression deck `bind_washout_boundary` and
  `tests/regtests/validate_washout_trace_consistency.sh` (trace on/off parity,
  washout on a step boundary, post-wash sampling).
- `krec` is printed in the binding-model summary for `3compartments`.
- `tests/regtests/validate_manual_model.sh`: the Manual model keeps its split
  every step, reads `ModeParams/BindingModel`, and applies a washout.
- `tests/regtests/validate_decay_chain_timing.sh`: Bi-212 from Pb-212 against
  the Bateman solution, and one position per decay with the chain off.
- `tests/regtests/validate_multithread_chain_bookkeeping.sh`: the decay chain
  must not depend on the thread count or on `CorrectByNumberOfHistories`.
- `tests/regtests/validate_output_files.sh`: default abundance file, times of
  1e6 s and more written and read back, uptake-file id check.

### Fixed
- `invitro_bind`: the washout was never applied during the run when
  `SaveProbabilityDistribution = "True"`. Writing the trace integrated the
  whole timeline first and left a "washout done" flag set, so every step kept
  the full medium activity, decay count and medium probability after
  `WashoutTime`, while the trace file itself showed the wash. The washout is
  now applied without stored state, at exactly `WashoutTime`, in both the
  trace and the run. Who should check their results: any `invitro_bind` run
  with a `WashoutTime` and `SaveProbabilityDistribution = "True"`; post-wash
  doses from those runs are not valid.
- `invitro_bind`: a washout that falls exactly on a time-step boundary now
  applies to the step that starts there; it was deferred by one step. Who
  should check: washout runs whose `WashoutTime` is a multiple of the step
  length (common with hour-aligned washouts); the first post-wash step was
  simulated with the pre-wash medium activity.
- `invitro_bind`: a `WashoutTime` at or before `Tf/TimelineStart` is now
  rejected with an error.
- `3compartments`: free receptors used the cytoplasm amount (`Ri - I`),
  unbinding released `koff * I` instead of `koff * M`, recycling had the wrong
  sign and was missing from the membrane, and an unset `krec` entered the
  equations as -1 1/s. The model now uses the 4-compartment terms, and an
  unset `krec` means no recycling. Effluxed activity, which used to leave the
  simulation, now stays in the well as non-binding medium activity (the
  degraded compartment) until a washout. Who should check: every
  `3compartments` result (internalised fractions were about 200x too low).
- `4compartments`: degraded ligand counted against the free receptors
  (`Ri - M - I - D`), so catabolite accumulated until it filled the receptor
  pool and binding stopped (cell-bound activity 3x too low after 2 h and
  almost zero after 24 h on the regression deck). Degraded ligand is outside
  the cell, so free receptors are now `Ri - M - I`. Who should check: every
  `4compartments` result, including rate constants fitted with this model.
- `invitro_bind` Manual model: only the first time step used
  `ManualBindingProbabilities`; every later step placed all decays in the
  medium, and a washout removed the whole activity. The model now starts from
  the manual split and keeps it, and a washout removes only the medium and
  degraded shares. Who should check: every Manual run with more than one time
  step.
- `invitro_bind`: `ModeParams/BindingModel = "Manual"` exited with "The
  following parameters are required" (koff, kon, ReceptorsPerCell); only the
  legacy `BindingModel` key was accepted for Manual. Both keys now work.
- Decay timing: the primary of every history decayed at the start of its time
  step, so its daughters had the whole step to decay in and too few were left
  for the next step (Pb-212 with 2 h steps: Bi-212 at 0.030-0.035 of the
  initial Pb-212 against 0.066-0.074 from the Bateman solution). The primary
  decay time is now drawn within the step from the emitter's decay law. Who
  should check: runs with short-lived daughters (Pb-212, Ac-225, Ra-223 and
  similar chains) and steps that are long against the daughter half-lives;
  decays were placed in too early a step, and step-resolved doses shift.
- Cytoplasm sampling (`invitro_bind`, internalised activity) was not uniform:
  decays were pushed away from the nucleus (sphere cell: 0.5% of decays within
  2 um of the nucleus instead of 7.4%; cylinder cell: caps above and below the
  nucleus sampled the wrong radius and half their height). Who should check:
  nucleus doses from cytoplasm activity, which were biased low.
- `IncludeWholeDecayChain = "False"`: each product of a decay got its own
  random position; they now share the decay position.
- The chain is followed through products left out by
  `FilterParticlesToSimulate`, and alpha products of zero-lifetime daughters
  are emitted (both were dropped before).
- `invitro_bind`: a history suppressed by the compartment sampler (only when
  the probabilities round to less than 1) is now dropped instead of being
  emitted at the component centre.
- Diffusion mode: `TimeGenerationStart` is compared with the timeline time,
  not with the time within the step.
- Regression baselines regenerated for the decay-timing change (decay counts
  of the second step of the Pb-212 decks are about 10% higher).
- Multithreaded runs: each worker thread depleted the parent only with the
  decays of its own histories, so with N threads the chain decayed about N
  times too slowly and later steps had too many decays (Pb-212, 4 threads:
  0.85 left at 10 h instead of 0.52). The worker threads now pool their
  abundance changes at the end of each step, and the shared output files
  (`run_metadata.json`, the abundance CSV, the binding-probability trace) are
  written by one thread instead of all of them. Who should check: any run
  with `Ts/NumberOfThreads` above 1 and more than one time step. Doses per
  step were too high in later steps, by an amount that grows with the thread
  count and the decay over the timeline. Single-thread runs are unchanged.
- `CorrectByNumberOfHistories = "False"`: the abundance bookkeeping used the
  rescaled primary weight, so the parent was depleted too fast (Pb-212, 10 h
  steps: 0.04 left at 20 h instead of 0.27). The abundances now always follow
  the physical number of decays. Who should check: runs with this setting and
  more than one time step.
- The isotopic abundance file was not written unless `WriteIsotopicAbundance`
  was set explicitly (it is on by default, but the default file name was
  missing), and a `temp.csv` was left in the run folder. It is now written as
  `isotopic_abundance.csv` by default, and the temporary file sits next to it
  (`<name>.tmp`).
- Abundance files: times of 1e6 s (11.6 days) and more were written as
  `1.2096e+06` and read back as 1, so `TimeToLoadIsotopicAbundance` loaded the
  wrong row; and an exact time match took `FractionOfDecaysAtTime0` from the
  last row. Who should check: runs that used `ReadIsotopicAbundanceFromFile`
  with files covering more than 11 days.
- `invitro_bind`: an `UptakePerCellFilename` entry with a cell id outside
  0..NumOfCells-1 wrote outside memory; it is now rejected at setup.
- `DoseInVitroRPT`: "Alpha number" counted an alpha on every step inside a
  nucleus, so it was inflated when steps are short (fine cuts or step limits);
  it now counts each alpha once per nucleus it enters. Doses are unchanged.

## [1.0.1] - 2026-09-09

### Fixed
- Recursive daughter-chain sampling (`So/<Source>/IncludeWholeDecayChain =
  "True"`) now works on Geant4 11.3 and later. A guard added out of caution
  about excited-state ions crashing decay-channel selection had disabled the
  recursive path there entirely, silently truncating chains to their first
  generation; the ground-state normalization the guard's own comment called
  for was already present lower in that routine, so removing the guard was
  sufficient. Geant4 11.2.2 stays the version this build and its CI are
  pinned to (see [Known Limitations](docs/known_limitations.md)).
- Fixed spurious immediate decay of isotopes Geant4's data marks stable but
  whose decay-table still lists leftover channel entries (found via Bi-209
  under the RadioactiveDecay5.6 data set bundled with Geant4 11.2.2 - the
  version this fix targets, unrelated to the Geant4 11.3 fix above). The
  chain sampler now checks Geant4's stability flag directly instead of
  inferring stability from lifetime sign alone. See
  [Known Limitations](docs/known_limitations.md) for detail and who should
  check their results against it.

## [1.0.0] - 2026-09-07

First public release, the reference version behind the targeted
alpha-particle therapy validation with Astatine 211-ParaThanatrace published
in Onecha et al., Int. J. Radiat. Oncol. Biol. Phys. 123(3), 2025.

### Added
- Three documented, tested source modes: `uniform`, `invitro_bind`,
  `activity_map`.
- `patches/` - the two small upstream fixes (OpenTOPAS, Geant4) this build
  needs, neither released yet.
- CI (`.github/workflows/build.yml`): builds Geant4 11.2.2 and OpenTOPAS 4.2.3
  from source with the two patches applied, links this extension against
  them, and runs the full regression suite on every push and pull request.
- Regression suite covering legacy parity, determinism, chain-control
  contracts, metadata schema, and the `activity_map` DICOM contract.
