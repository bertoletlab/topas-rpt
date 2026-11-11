# Known Limitations

## Known issue: daughter de-excitation is not simulated (1.1.0 and earlier)

TOPAS-RPT samples each decay itself from the Geant4 decay tables, so the daughter nucleus can be left in an excited state. With electromagnetic physics alone, no process de-excites that nucleus, so releases up to and including 1.1.0 emitted no γ-rays, conversion electrons, X-rays or Auger electrons from daughters. Absorbed doses from nuclides with a sizeable share of these emissions were therefore underestimated. Compared with plain TOPAS using `g4radioactivedecay`, for a source in a 0.5 mm water sphere inside a 60 cm water box, 1.1.0 deposited 14% less energy within 0.5 mm and 26% less in total for 177Lu, 36% and 39% less for 161Tb, and about 98% less for 99mTc, whereas 211At and 225Ac agreed within 1%. Adding `g4radioactivedecay` to the physics list does not repair this, since it then decays the chain a second time. Who should check their results: any run with 177Lu, 161Tb, an isomeric source such as 99mTc, or a γ-emitting chain. A fix is planned for 1.2.0.

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

Geant4 11.3.2 with OpenTOPAS 4.3.0 is the pair this build and its CI are
pinned to and tested against (see [Requirements](installation.md#requirements)),
on Ubuntu 24.04 and 26.04. The regression baselines were generated with that
pair on macOS arm64, so CI compares Linux x86_64 results against them within
the tolerances in `tests/regtests/baseline/tolerances.env`. Geant4 11.2.2 with OpenTOPAS 4.2.3 also builds, but needs the two patches
dropped in the move (see `patches/README.md` and the git history), and its
results are compared against baselines made with the newer data sets.

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
