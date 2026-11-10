# Changelog

All notable changes to `topas-rpt` are documented here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and
[Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

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
