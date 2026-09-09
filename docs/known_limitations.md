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
tested against (see [Requirements](../README.md#requirements)). Promoting a
newer Geant4 line to canonical is a separate decision from this fix and has
not been made.
