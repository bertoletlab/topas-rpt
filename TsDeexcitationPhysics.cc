// Physics Module for tsrts_deexcitation
//

#include "TsDeexcitationPhysics.hh"

#include "G4DeexPrecoParameters.hh"
#include "G4EmParameters.hh"
#include "G4LossTableManager.hh"
#include "G4NuclearLevelData.hh"
#include "G4NuclideTable.hh"
#include "G4UAtomicDeexcitation.hh"

#include <cmath>

TsDeexcitationPhysics::TsDeexcitationPhysics(TsParameterManager*)
    : G4VPhysicsConstructor("tsrts_deexcitation")
{
    // As G4RadioactiveDecayPhysics; these are locked after PreInit, which is why they
    // live in a physics constructor and not in the source.
    G4DeexPrecoParameters* deex = G4NuclearLevelData::GetInstance()->GetParameters();
    deex->SetStoreICLevelData(true);
    deex->SetInternalConversionFlag(true);
    deex->SetIsomerProduction(true);
    deex->SetMaxLifeTime(G4NuclideTable::GetInstance()->GetThresholdOfHalfLife() / std::log(2.));
}

void TsDeexcitationPhysics::ConstructProcess()
{
    G4EmParameters::Instance()->SetAuger(true);
    G4EmParameters::Instance()->SetDeexcitationIgnoreCut(true);

    G4LossTableManager* man = G4LossTableManager::Instance();
    if (man->AtomDeexcitation() == nullptr) {
        man->SetAtomDeexcitation(new G4UAtomicDeexcitation());
        man->ResetParameters();
    }
}
