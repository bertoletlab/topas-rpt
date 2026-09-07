# topas-rpt

**topas-rpt** extends [OpenTOPAS](https://opentopas.readthedocs.io/) with the two things
standard Monte Carlo dosimetry approaches generally get wrong for radiopharmaceutical
therapy: they treat each decay as an instantaneous, independent event, when a decaying
nuclide's daughters carry their own radiation signature delivered at a different place and
time than the parent's, and dose to a target actually builds up over the whole treatment as
the radioligand distributes, binds, and clears.

topas-rpt extends OpenTOPAS with explicit control over both - sampling decays across a full
branching chain with the correct time structure, and threading radioligand binding
kinetics through the source term over the course of a simulated exposure.

The core decay-chain and binding-kinetics machinery is described in Onecha et al.,
*"Space- and Time-Defined Monte Carlo Dosimetry Explains Ovarian Cancer Cell Viability in
Targeted &alpha;-Particle Therapy With Astatine 211-ParaThanatrace,"* Int. J. Radiat. Oncol.
Biol. Phys. 123(3), 2025.

## Modes

| Mode | Use case |
|------|----------|
| `uniform` | Baseline: decays sampled uniformly within a component, full decay-chain tracking, time-binned normalization. |
| `invitro_bind` | Compartmental radioligand binding kinetics for in-vitro RPT - the mode behind the IJROBP validation above. |
| `activity_map` | Decay positions sampled from a voxelized DICOM activity map (calibrated Bq/mL or raw counts). |

## Who this guide is for

This manual assumes you can already build and run OpenTOPAS and are comfortable writing
OpenTOPAS parameter files. You do **not** need any C++ experience. If you have never run
OpenTOPAS before, work through the
[OpenTOPAS documentation](https://opentopas.readthedocs.io/) first, then come back here.

## How this manual is organized

| If you want to… | Read |
|---|---|
| Build the extension and confirm it works | [Installation](installation.md) |
| Run your first simulation | [Quickstart](quickstart.md) |
| Understand the modes, parameters, and output files | [User Guide](user_guide.md) |
| See what the regression suite checks | [Validation](validation.md) |
| Know what isn't supported yet | [Known Limitations](known_limitations.md) |
