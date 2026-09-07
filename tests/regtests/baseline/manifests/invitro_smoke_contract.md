# In-Vitro Smoke Contract

This note defines the deterministic smoke artifacts checked by the `invitro_bind` determinism gate.

## Scope

- Mode: `invitro_bind`
- Runner: `scripts/run_examples_with_built_topas.sh ... smoke`
- Determinism gate: `tests/regtests/validate_invitro_smoke_determinism.sh`
- Environment: OpenTOPAS `4.2.3` with Geant4 `11.2.2`

## Checked artifacts

The contract compares SHA256 across two consecutive smoke runs for:

1. `run_metadata.json`
2. `targeted_PDAC_isotopic_abundance.csv`
3. `OutputFileName.header`

## Why these files

1. `run_metadata.json`
- Verifies temporal bookkeeping and event-weight normalization are bitwise-stable in smoke mode.
- Captures `entries[*]` with time window and effective activity/weight fields.

2. `targeted_PDAC_isotopic_abundance.csv`
- Verifies decay-chain abundance evolution and reporting path stability.

3. `OutputFileName.header`
- Verifies phase-space/scorer header-level run configuration metadata stability.

## Interpretation

- PASS means these core smoke outputs are reproducible for fixed seed and fixed smoke settings.
- FAIL indicates a regression in deterministic behavior (algorithmic ordering, time bookkeeping, or output formatting).
- This is a contract check, not a scientific equivalence proof for all observables.
