# Changelog

All notable changes to `topas-rpt` are documented here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and
[Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- `tests/regtests/validate_multithread_chain_bookkeeping.sh`: the decay chain
  must not depend on the thread count or on `CorrectByNumberOfHistories`.

### Fixed
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
