# User Guide

## Source modes

Set the mode with `s:So/<Source>/Mode = "uniform" | "invitro_bind" | "activity_map"`.

**`uniform`** - the baseline mode. `topas-rpt` samples how many decays occur
in each timeline step from the radioactive exponential decay law, governed
by each isotope's half-life. With `IncludeWholeDecayChain = "True"`, it walks
each daughter's own decay recursively
(`AddRecursivelyToIsotopeListIfRadioactive`,
`TsRadioactiveTimeGenerator.cc:1056`), so a daughter produced mid-timeline
gets its own correctly-timed emission instead of the parent's instantaneous
one. Decays are sampled uniformly within a geometry component, with
time-binned normalization and the chain controls above
(`IncludeWholeDecayChain`, `TreatAdditionalDecaysAsNewHistories`). Use this
mode when you need controlled time bins without a biological binding model
layered on top.

**`invitro_bind`** - compartmental in-vitro kinetics. A dose of radioligand
starts in the surrounding medium and, over time, binds cell-surface
receptors, gets internalized into the cytoplasm, and either reaches the
nucleus or gets degraded and cleared, each transfer governed by its own
binding and release rate. `TsDynamicBindModel` numerically integrates this
compartment by compartment (`TsDynamicBindModel.cc`) and feeds the resulting
occupancy fractions back into the decay-position sampler, so where a decay
occurs on the simulated timeline tracks the radioligand's real position as it
moves through the cell over time. This is the mode behind the published
Astatine 211-ParaThanatrace validation (Onecha et al., IJROBP 2025), which
matched simulated dose-response curves to real ovarian cancer cell viability
data.

**`activity_map`** - decay positions sampled from a voxelized DICOM activity
map. Each voxel's activity value becomes a sampling weight:
`topas-rpt` builds a cumulative probability distribution over all non-zero
voxels (`TsActivityMapPositionSampler`) and draws each decay's position from
it, so denser voxels produce proportionally more decays, matching how a real
radiotracer distributes non-uniformly through a patient. Supports calibrated
units (`ModeParams/CalibratedCountUnits = "BqPerMl"`, when map values are
activity concentration) or raw counts (`"Counts"`, with
`ModeParams/CalibrationScaleBqPerCount` to convert). Writes an
`activity_map_summary.json` QA report alongside the usual outputs - voxel
statistics, calibration settings, and a sanity check that the map and its
parent patient geometry actually agree on extent.

## How the output weighting works

Every run writes normalization metadata to `run_metadata.json`. The rule that
ties a simulated history back to real activity is:

```
primary_weight = n_decays_step / n_histories_step
```

Each simulated history stands in for many physical decays in that time step,
so the energy-deposition scoring is weighted accordingly, and reported dose
already reflects the activity and timeline you configured. When comparing two
runs, compare all four numbers together: dose, `n_decays_step`,
`n_histories_step`, and `primary_weight`.

## What to check after a run

- `run.log` - no fatal warnings or exceptions.
- `run_metadata.json` - `schema_version` is `tsrts.run_metadata.v1`,
  `normalization_spec` is `tsrts.norm.v1`, and the mode/time-bin fields match
  what you configured.
- The isotopic abundance CSV, if you turned it on - columns are `Time`,
  per-isotope fractions, and `FractionOfDecaysAtTime0`. Use it to confirm the
  chain evolves the way you expect.
- `activity_map_summary.json`, for `activity_map` mode specifically.

## Running the examples

```bash
scripts/run_examples_with_built_topas.sh /path/to/topas smoke      # invitro_bind
scripts/run_time_feature_decay_chain_test.sh /path/to/topas        # uniform, chain control
scripts/run_uniform_scientific_showcase.sh /path/to/topas
scripts/run_invitro_scientific_showcase.sh /path/to/topas
```

For `activity_map` (needs real DICOM CT + NM/PET data):

```bash
export ACTIVITYMAP_CT_DICOM_DIR=/path/to/scan/ct
export ACTIVITYMAP_NM_DICOM_DIR=/path/to/scan/spect
scripts/run_activity_map_example_with_built_topas.sh /path/to/topas smoke
scripts/run_activity_map_scientific_showcase.sh /path/to/topas
```

## Before trusting results

1. Run the regression suite and confirm every step passes
   (see [Validation](validation.md)).
2. Confirm the run's `run_metadata.json` matches the intended protocol - time
   bins, activity, mode.
3. For anything using the full decay chain, look at the isotopic abundance
   CSV and confirm the parent-daughter timing matches what you expect.

See [`examples/canonical/README.md`](https://github.com/bertoletlab/topas-rpt/tree/main/examples/canonical)
for a minimal parameter file per mode to start a new study from.
