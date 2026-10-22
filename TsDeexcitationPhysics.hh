// Physics Module for tsrts_deexcitation
//
// Nuclear de-excitation settings for So/<src>/DeexciteDaughters, without registering
// G4Radioactivation: the settings G4RadioactiveDecayPhysics makes (internal-conversion
// shell data, isomer production, Auger electrons, de-excitation below cuts), so that the
// generator's own G4ITDecay emits conversion electrons, x-rays and Auger electrons.
// List it after the EM module: sv:Ph/Default/Modules = 2 "g4em-livermore" "tsrts_deexcitation"
//

#ifndef TsDeexcitationPhysics_hh
#define TsDeexcitationPhysics_hh

#include "G4VPhysicsConstructor.hh"

class TsParameterManager;

class TsDeexcitationPhysics : public G4VPhysicsConstructor
{
public:
    TsDeexcitationPhysics(TsParameterManager* pM);
    ~TsDeexcitationPhysics() override = default;

    void ConstructParticle() override {}
    void ConstructProcess() override;
};

#endif
