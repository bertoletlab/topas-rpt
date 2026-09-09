# Known Limitations

## Geant4 version support

Full recursive daughter-chain sampling - the part of this extension that
walks a decay chain past the first generation, tracking excited-state
normalization and branching ratios as it goes - works correctly on Geant4
11.2.2 and on Geant4 11.3 and later. An earlier release of this extension
disabled the recursive path on Geant4 >= 11.3 out of caution about
excited-state ions crashing decay-channel selection; that concern turned out
to already be handled by ground-state normalization logic that runs earlier
in the chain-sampling routine, so the guard was removed once verified against
the full regression suite plus a targeted excited-state stress test.

Geant4 11.2.2 remains the version this build and its CI are pinned to and
tested against (see [Requirements](installation.md#requirements)). Promoting
a newer Geant4 line to canonical is a separate decision from this fix and has
not been made.

## Resolved: spurious immediate decay of isotopes Geant4 marks stable

Versions before this fix used a positive PDG lifetime as the sole test for
"this isotope needs a sampled decay time." A genuinely instant transition and
an isotope Geant4's stability flag already marks as stable - but whose
decay-table data file still lists leftover decay channels - both report an
identical non-positive lifetime sentinel, so that test conflated the two. The
extension now checks Geant4's stability flag directly and trusts it over any
leftover channel entries.

This shows up in practice as an isotope whose underlying nuclear data file
recorded its half-life with an unresolved placeholder instead of a real
measured value getting spuriously decayed during recursive chain sampling,
even though Geant4 itself treats it as stable. Bi-209 is one documented example
under the RadioactiveDecay5.6 data set bundled with Geant4 11.2.2 - a data set
this fix does not change, so anyone re-running an older simulation against an
unpatched build of this extension may want to check whether their chain of
interest passes through an isotope carrying a comparable placeholder value.
