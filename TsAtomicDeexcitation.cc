// Extra Class for tsrts_deexcitation

#include "TsAtomicDeexcitation.hh"

void TsAtomicDeexcitation::GenerateParticles(std::vector<G4DynamicParticle*>* secVect,
                                             const G4AtomicShell* shell, G4int Z,
                                             G4double gammaCut, G4double eCut)
{
    // The bounds G4UAtomicDeexcitation checks only after storing the vacancy.
    if (Z < 6 || Z > 104)
        return;
    G4UAtomicDeexcitation::GenerateParticles(secVect, shell, Z, gammaCut, eCut);
}
