# Known Limitations

## Geant4 11.3+ decay-chain sampling

Full recursive daughter-chain sampling - the part of this extension that
walks a decay chain past the first generation, tracking excited-state
normalization and branching ratios as it goes - is disabled on Geant4 11.3
and later. `So/<Source>/IncludeWholeDecayChain = "True"` silently becomes a
no-op there: only the first-generation decay products are generated, and the
chain does not propagate further.

The reason is a change in how Geant4 11.3 exposes its radioactive-decay
sampling internals; some excited-state ions can crash decay-channel selection
under the old recursive approach on the newer API, so the extension declines
to run the recursive path at all on 11.3+ to avoid producing subtly wrong
chain physics, issuing a one-time console warning instead. Geant4 11.2.2
remains the canonical, fully-supported version for this reason.
