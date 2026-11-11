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

A washout replaces the medium with fresh, ligand-free medium: it empties the
medium compartment (and the degraded one) at that time, while bound and
internalized ligand stay. Set one washout with `ModeParams/WashoutTime`, or
several with `ModeParams/WashoutTimes`, a time vector such as
`dv:So/<Source>/ModeParams/WashoutTimes = 3 18 42 66 h` (not both). Each
washout must be later than `Tf/TimelineStart`. It is applied at exactly its
time, also when that time falls inside a time step or on a step boundary.

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

With `CorrectByNumberOfHistories = "True"` (the usual setting), the number of
histories in a step is `NumberOfHistoriesInRun` times the step's decays divided
by the first step's decays, truncated to an integer, so late steps of a
decaying source run fewer histories at the same weight. A step that still has
decays never drops below one history: that history carries all of the step's
decays as its weight, so dose stays unbiased, although a step with very few
histories is noisy. Raise `NumberOfHistoriesInRun` when the late steps matter.

## What to check after a run

- `run.log` - no fatal warnings or exceptions.
- `run_metadata.json` - `schema_version` is `tsrts.run_metadata.v1`,
  `normalization_spec` is `tsrts.norm.v1`, and the mode/time-bin fields match
  what you configured.
- The isotopic abundance CSV, if you turned it on - columns are `Time`,
  per-isotope fractions, and `FractionOfDecaysAtTime0`. Use it to confirm the
  chain evolves the way you expect.

### Restarting from an abundance file

The abundance CSV is written by default (`WriteIsotopicAbundance`), one row per
step start. Each isotope column is the number of atoms of that isotope as a
fraction of the atoms present at the original time 0, so the decay since then
is already included. `FractionOfDecaysAtTime0` is the step's decays divided by
the run's first-step decays; with `CorrectByNumberOfHistories = "False"`, where
every step uses the first step's weight, multiplying a step's scored values by
it gives the physical result.

A later run can continue the chain from such a file:

```
b:So/<Source>/ReadIsotopicAbundanceFromFile = "True"
s:So/<Source>/ReadIsotopicAbundanceFileName = "isotopic_abundance.csv"
d:So/<Source>/TimeToLoadIsotopicAbundance   = 36000 s   # omit to load the last row
```

Keep `InitialActivity` at the activity of the original time 0: the loaded
fractions already carry the decay. Between rows the fractions are interpolated
linearly, which is close to the exponential only when the rows are short against
the half-lives, so restart at a row time when you can. `FractionOfDecaysAtTime0`
of the new run starts again at 1, since it refers to that run's first step;
`UseFractionOfActivityFromFile` only reports the value from the file.
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
