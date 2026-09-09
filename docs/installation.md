# Installation

## Requirements

- OpenTOPAS 4.2.3
- Geant4 11.2.2, with a one-line patch (see [`patches/`](https://github.com/bertoletlab/topas-rpt/tree/main/patches)) - the version this build and its CI are pinned to and tested against (see [Known Limitations](known_limitations.md))
- GDCM 2.6.8, for DICOM-driven activity maps - see [Building GDCM](building_gdcm.md) if your system CMake is too recent to configure it directly
- CMake (Ninja recommended)

## Building

This extension needs one small patch applied to OpenTOPAS before building - GDCM's
registration of `TsDicomActivityMap` had a buffer-allocation bug triggered by a
divided-patient geometry. See
[`patches/README.md`](https://github.com/bertoletlab/topas-rpt/tree/main/patches) for the
patch and how to apply it (a couple of minutes, no rebuild of anything else required).

```bash
git clone https://github.com/OpenTOPAS/OpenTOPAS --branch v4.2.3
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

`-DTOPAS_USE_QT=ON` matters even for a batch build with no viewer open: OpenTOPAS's
sequence manager references its Qt-backed session class unconditionally, so leaving this
off fails at *link* time, after everything else has already compiled.

GDCM 2.6.8 needs an older CMake to configure (its exported-target metadata predates a
policy since tightened by CMake) - see [Building GDCM](building_gdcm.md) if `cmake` on
your system is recent enough to hit this.
