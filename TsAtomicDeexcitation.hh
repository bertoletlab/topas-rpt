// Atomic de-excitation for tsrts_deexcitation: G4UAtomicDeexcitation that ignores Z < 6
// and Z > 104 before touching its vacancy list.
//
// With the Auger cascade active, G4UAtomicDeexcitation::GenerateParticles (Geant4 11.2 and
// earlier) pushes the vacancy onto its member vacancyArray before returning for Z < 6,
// and nothing clears it. With DeexcitationIgnoreCut, Livermore Compton scattering
// on hydrogen makes that call with Z = 1, so the next call, on whatever atom, also
// relaxes this stale K vacancy. When the event ends right after (the photon escapes),
// the next call is the atomic relaxation of the next decay, and the source emits a
// spurious daughter K x-ray or K Auger cascade (99mTc: 0.117 K x-rays per decay in a
// 0.4 m water phantom against 0.062 in vacuum). Returning before the push, as the
// relaxation of an atom without data has to, removes it. Geant4 11.3.0 clears
// vacancyArray at the start of every call, so from 11.3 on this class changes nothing
// (99mTc in water: 0.178 Tc K x-rays per decay without it on 11.2.2, 0.073 with it and in
// vacuum; 0.073 with and without it on 11.3.2).
//

#ifndef TsAtomicDeexcitation_hh
#define TsAtomicDeexcitation_hh

#include "G4UAtomicDeexcitation.hh"

class TsAtomicDeexcitation : public G4UAtomicDeexcitation
{
public:
    TsAtomicDeexcitation() = default;
    ~TsAtomicDeexcitation() override = default;

    void GenerateParticles(std::vector<G4DynamicParticle*>* secVect, const G4AtomicShell* shell,
                           G4int Z, G4double gammaCut, G4double eCut) override;
};

#endif
