<p align="center"><b>OpenTOPAS extension for time-resolved radiopharmaceutical therapy dosimetry.</b></p>

<p align="center">
  <a href="https://github.com/bertoletlab/topas-rpt/actions/workflows/build.yml"><img src="https://github.com/bertoletlab/topas-rpt/actions/workflows/build.yml/badge.svg" alt="build"></a>
  <a href="LICENSE"><img src="https://img.shields.io/badge/License-MIT-blue.svg" alt="License: MIT"></a>
  <a href="https://topas-rpt.readthedocs.io/"><img src="https://img.shields.io/badge/docs-Read%20the%20Docs-8CA1AF?logo=readthedocs&logoColor=white" alt="Documentation"></a>
  <a href="https://github.com/bertoletlab/topas-rpt/releases"><img src="https://img.shields.io/github/v/release/bertoletlab/topas-rpt?label=release&color=1f2a56" alt="Release"></a>
  <a href="https://doi.org/10.5281/zenodo.22651399"><img src="https://zenodo.org/badge/DOI/10.5281/zenodo.22651399.svg" alt="DOI"></a>
  <img src="https://img.shields.io/badge/OpenTOPAS-4.3.0-1f2a56" alt="OpenTOPAS 4.3.0">
</p>

Standard Monte Carlo dosimetry approaches generally treat each decay as an
instantaneous event and each isotope independently. For radiopharmaceutical
therapy, that misses two things that matter: a decaying nuclide's daughters
carry their own radiation signature, delivered at a different place and time
than the parent's; and dose to a target builds up over the whole treatment as
the radioligand distributes, binds, and clears. `topas-rpt` extends OpenTOPAS
with explicit control over both - sampling decays across a full branching
chain with the correct time structure, and threading radioligand binding
kinetics through the source term over the course of a simulated exposure.

The core decay-chain and binding-kinetics machinery is described in Onecha et
al., *"Space- and Time-Defined Monte Carlo Dosimetry Explains Ovarian Cancer
Cell Viability in Targeted &alpha;-Particle Therapy With Astatine
211-ParaThanatrace,"* Int. J. Radiat. Oncol. Biol. Phys. 123(3), 2025.

## Requirements

- OpenTOPAS 4.3.0
- Geant4 11.3.2 - the version this build and its CI are pinned to and tested
  against (see [Known Limitations](docs/known_limitations.md))
- GDCM 2.6.8, for DICOM-driven activity maps
- CMake (Ninja recommended)

## Building

This extension needs one small patch applied to OpenTOPAS before building -
GDCM's registration of `TsDicomActivityMap` had a buffer-allocation bug
triggered by a divided-patient geometry. See [`patches/README.md`](patches/README.md)
for the patch and how to apply it (a couple of minutes, no rebuild of anything
else required).

```bash
git clone https://github.com/OpenTOPAS/OpenTOPAS --branch v4.3.0
cd OpenTOPAS
git apply /path/to/topas-rpt/patches/0001-opentopas-harden-dicom-activity-map.patch

cmake -S . -B build \
  -DTOPAS_EXTENSIONS_DIR=/path/to/topas-rpt \
  -DTOPAS_USE_QT=ON \
  -DGeant4_DIR=/path/to/geant4-install/lib/cmake/Geant4 \
  -DGDCM_DIR=/path/to/gdcm-install/lib/gdcm-2.6 \
  -DCMAKE_INSTALL_PREFIX=/path/to/install
cmake --build build --target install
```

Set `-DTOPAS_USE_QT` to match your Geant4 build: `ON` when Geant4 was built
with Qt (as above, adding `-DTOPAS_USE_QT6=ON` for a Qt6 build), `OFF` when it
was not. A mismatch fails, even for a batch build with no viewer open: `ON`
against a Geant4 without Qt does not compile (`qobject.h` not found), and a
Geant4 with Qt that OpenTOPAS does not recognise on its own (some Qt6 builds)
fails at *link* time on `TsQt5` symbols. `geant4-config --has-feature qt` tells
you whether your Geant4 has Qt. `scripts/build_topas_with_extension.sh` reads
both settings from Geant4's CMake package (`TOPAS_USE_QT=auto`, the default;
`ON` or `OFF` override it, although `OFF` cannot remove Qt from a Geant4 that
OpenTOPAS detects as built with it).

GDCM 2.6.8 needs an older CMake to configure (its exported-target metadata
predates a policy since tightened by CMake) - see
[`docs/building_gdcm.md`](docs/building_gdcm.md) if `cmake` on your system is
recent enough to hit this.

## Quick Start

```bash
topas examples/canonical/uniform_minimal.txt
```

Three modes ship supported and documented:

| Mode | Use case |
|------|----------|
| `uniform` | Baseline: decays sampled uniformly within a component, full decay-chain tracking, time-binned normalization. |
| `invitro_bind` | Compartmental radioligand binding kinetics for in-vitro RPT - the mode behind the IJROBP validation above. |
| `activity_map` | Decay positions sampled from a voxelized DICOM activity map (calibrated Bq/mL or raw counts). |

More in [`examples/canonical/`](examples/canonical/) and the
[user guide](docs/user_guide.md).

## Validation

```bash
tests/regtests/validate_all_features_vs_legacy.sh /path/to/topas
```

Needs `ripgrep` on `PATH`. See [`docs/validation.md`](docs/validation.md) for
what each of the regression steps actually checks.

## Documentation

Full documentation is at **[topas-rpt.readthedocs.io](https://topas-rpt.readthedocs.io/)**.
Individual pages, viewable directly on GitHub:

- [User Guide](docs/user_guide.md) - modes, parameters, output files
- [Validation](docs/validation.md) - the regression suite
- [Known Limitations](docs/known_limitations.md)
- [`patches/`](patches/) - the two small upstream patches this build needs

## License

MIT - see [`LICENSE`](LICENSE). If you use this in published work, please cite
the paper above (see [`CITATION.cff`](CITATION.cff)).
