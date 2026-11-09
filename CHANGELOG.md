# Changelog

All notable changes to `topas-rpt` are documented here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and
[Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- `tests/regtests/validate_output_files.sh`: default abundance file, times of
  1e6 s and more written and read back, uptake-file id check.

### Fixed
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
