# Patches

Two small, independent one-line-scale fixes this build needs, neither of
which has made it into an official release yet.

## `0001-opentopas-harden-dicom-activity-map.patch`

Applies to OpenTOPAS 4.2.3, file `geometry/TsDicomActivityMap.cc`.

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

## `0002-geant4-fix-columns-icc-typo.patch`

Applies to Geant4 11.2.2, file
`source/externals/g4tools/include/tools/wroot/columns.icc`.

A one-character typo (`m_barnch` for `m_branch`) in the copy constructor of
`std_vector_column_ref`, in Geant4's bundled `g4tools` third-party library.
Already fixed upstream as of Geant4 11.3, but 11.2.2 remains canonical for
this extension (see [Known Limitations](../docs/known_limitations.md)), so
the fix needs to be applied by hand until then. Only matters if that specific
copy constructor gets instantiated - most builds won't hit it, but it costs
nothing to apply.

```bash
git apply /path/to/topas-rpt/patches/0002-geant4-fix-columns-icc-typo.patch
```
