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

Two inputs set the activity, and they play different roles.
`InitialActivity` (total Bq in the geometry) sets the number of decays per
step, and so the dose; `ModeParams/Activity_permL` (Bq/mL), with
`ModeParams/SpecificActivity_pernmol`, sets the ligand concentration of the
binding kinetics. For a consistent deck, `InitialActivity` equals
`Activity_permL` times the medium volume in mL: for example, 1 MBq/mL in a
4 x 4 x 2.1 mm medium (33.6 uL) gives 33600 Bq. At start-up the source compares
the two and issues warning `TsRADIOACTIVEBIND001` when `InitialActivity` falls
outside the range from the liquid volume (medium minus cells) to the whole
medium envelope, widened by 5%. A unit slip in either input (uL for mL, kBq for
Bq, a half-length entered as a full length) therefore shows up as a ratio of
1000, 0.001 or 8 in that warning. `s:So/<Source>/ModeParams/ActivityConsistencyCheck`
takes `"Warn"` (default), `"Error"` (stop the run) or `"Off"`, and
`u:So/<Source>/ModeParams/ActivityConsistencyTolerance` changes the 5%. The
run metadata records both implied activities (`implied_initial_activity_bq`,
`implied_initial_activity_liquid_bq`).

**Isotopic abundances.** Every mode tracks the composition of the decay chain
between steps (written with `WriteIsotopicAbundance`). By default
(`s:So/<Source>/AbundanceUpdate = "MonteCarlo"`) each history moves its share of
the step's decays from the nuclide it sampled to the daughters it produced, so
the composition carries the sampling noise of the histories, which grows when a
step has few of them. `AbundanceUpdate = "Analytic"` advances the composition
with the Bateman solution of the chain (Geant4 half-lives and branching ratios)
instead: the result has no noise and does not depend on the seed, the thread
count or the number of histories. It is not available in `diffusion` mode.

Since the decay count of every later step comes from the abundances, the
choice also affects the dose normalisation. For a Pb-212 source over 72
one-hour steps (30 seeds), the sampled update gave these totals of decays
against the Bateman solution:

| `NumberOfHistoriesInRun` | sampled / Bateman | spread between runs |
|---|---|---|
| 20 | 1.016 | 1.4% |
| 200 | 1.003 | about 0.5% |
| 2000 | 1.0005 (no significant bias) | about 0.2% |

The small excess at low history counts comes from abundances that a single
history drives below zero being reset to zero. Daughters scatter much more
than the parent: with 20 histories, Bi-212 at 46 h ranged from 0 to 2.4 times
its Bateman value. The analytic update follows the same chain rules as the
sampled one: excited daughters are followed as their ground state (isomers
such as Tc-99m are not kept apart), and a nuclide with a meanlife below 1e-6
of the step passes straight to its daughters, so very short-lived nuclides
(At-217, Po-212) appear in the file only when the steps are short. It also
differs in one corner case: in `invitro_bind` with `"Manual"` probabilities
that sum below 1 (which prints `SUM OF PROBABILITIES IS NOT 1`), the sampled
update adds no daughters for the dropped histories, while the analytic update
keeps the whole chain.

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

## Daughter de-excitation

`topas-rpt` samples each decay itself, so the daughter nucleus can come out in
an excited state, and with electromagnetic physics alone nothing de-excites
it. Its gammas, conversion electrons, X-rays and Auger electrons are then
missing (see [Known Limitations](known_limitations.md)). To emit them, add the
`tsrts_deexcitation` physics module after the EM module and turn on
`DeexciteDaughters`:

```
sv:Ph/Default/Modules = 2 "g4em-livermore" "tsrts_deexcitation"
b:Ph/Default/Fluorescence = "True"
b:Ph/Default/Auger = "True"
b:Ph/Default/DeexcitationIgnoreCut = "True"
b:So/<Source>/DeexciteDaughters = "True"
```

The excited daughter of every sampled decay, first decay and chain decays
alike, is then taken to its ground state through Geant4's isomeric-transition
decay (photon evaporation, internal conversion, atomic relaxation), and every
product is emitted as a primary at the decay position, moving with the
recoil of the nucleus it came from. A daughter level with a mean life above
`d:So/<Source>/IsomerLifetimeThreshold` (default 1 min) is kept as an isomer.
With `IncludeWholeDecayChain`, the chain then decays it with its own lifetime
and decay table, and an isomer still present at the end of a step carries over
as its own entry of the abundance file (Mo-99 feeds `Tc99[142.684]`, whose
abundance followed the Bateman solution within 2% over 18 h in 6 h steps).
`AbundanceUpdate = "Analytic"` follows excited daughters as their ground state,
so it does not keep these isomers between steps (warning `TsRADIOACTIVE044`); a
chain through an isomer needs the default `"MonteCarlo"`. `DeexciteDaughters` without the
module stops the run (`TsRADIOACTIVE020`). Do not add `g4radioactivedecay`:
it decays the chain a second time during tracking.

`DeexciteDaughters` is off by default, so existing decks give the same results
as before. For a source in a 0.5 mm water sphere inside a 60 cm water box, it
brings the energy per decay to that of `g4radioactivedecay` within 2.5%
in the sphere and 1.5% in total (Lu-177, Tb-161, Pb-212 with its chain, At-211,
Ac-225), where it was 14% and 26% low for Lu-177 and 36% and 39% low for
Tb-161 without it. A metastable source such as Tc-99m is set as
`GenericIon(43,99)` with `d:So/<Source>/ExcitationEnergy = 142.6836 keV`; it
needs the flag, since without it almost all of its energy is lost.

`s:So/<Source>/FirstForbiddenBetaShape = "allowed"` (default `"geant4"`)
rebuilds the beta branches that the decay data tag as first-forbidden with the
allowed spectrum shape. The Geant4 shape factor for those branches is a fit to
Bi-210 data; it puts the mean beta energy of Tb-161 and Er-169 6 to 9% below
ICRP 107, and `"allowed"` brings them within 2%. Nuclides without such a
branch, including Lu-177, are unchanged. The endpoints are read from the
standard Geant4 decay data, so `"allowed"` should not be combined with decay
files given through `/grdm/setRadioactiveDecayFile`.

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
