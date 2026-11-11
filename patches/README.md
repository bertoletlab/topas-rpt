# Patches

Small, independent fixes this build needs on top of the pinned releases.

Patches `0002` (Geant4 11.2.2 `columns.icc` typo) and `0003` (OpenTOPAS 4.2.3
zero-kinetic-energy momentum) were dropped when the build moved to Geant4 11.3.2
and OpenTOPAS 4.3.0, which include both fixes. They remain in the git history
for anyone still building against the older pair.

## `0001-opentopas-harden-dicom-activity-map.patch`

Applies to OpenTOPAS 4.3.0 (and 4.2.3), file `geometry/TsDicomActivityMap.cc`.

`TsDicomActivityMap` assumes its parent component is always a
`TsDicomPatient` when computing the DICOM-frame translation, and assumes
certain DICOM tags (per-frame functional group sequences, slice thickness)
are always present. Neither assumption holds for every valid geometry or
every scanner's DICOM export: a divided/parallel-world patient geometry can
give it a parent that isn't a plain `TsDicomPatient`, and some NM exports omit
tags this code previously required outright. This patch adds a graceful
fallback (zero translation, a console warning) for the first case, and falls
back to the plain `ImagePositionPatient`/derived-spacing tags for the second,
instead of aborting.

Apply from your OpenTOPAS checkout:

```bash
git apply /path/to/topas-rpt/patches/0001-opentopas-harden-dicom-activity-map.patch
```
