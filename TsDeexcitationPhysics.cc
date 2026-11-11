// Physics Module for tsrts_deexcitation
//

#include "TsDeexcitationPhysics.hh"

#include "TsAtomicDeexcitation.hh"

#include "G4DeexPrecoParameters.hh"
#include "G4EmParameters.hh"
#include "G4Exception.hh"
#include "G4LossTableManager.hh"
#include "G4NuclearLevelData.hh"
#include "G4NuclideTable.hh"
#include "G4UAtomicDeexcitation.hh"

#include <typeinfo>

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

    // The EM module has already installed G4UAtomicDeexcitation (G4EmBuilder); replace it
    // with TsAtomicDeexcitation, which leaves no stale vacancy behind for the next decay
    // (TsAtomicDeexcitation.hh). The models fetch the pointer when the run initialises,
    // after this. Any other atomic de-excitation a module installed is kept.
    G4LossTableManager* man = G4LossTableManager::Instance();
    G4VAtomDeexcitation* ad = man->AtomDeexcitation();
    if (ad == nullptr || typeid(*ad) == typeid(G4UAtomicDeexcitation)) {
        man->SetAtomDeexcitation(new TsAtomicDeexcitation());
        man->ResetParameters();
    } else {
        G4ExceptionDescription msg;
        msg << "tsrts_deexcitation keeps the atomic de-excitation another module installed ("
            << typeid(*ad).name() << "). With Geant4 11.2 and earlier it can leave a stale "
            << "vacancy that adds spurious daughter K x-rays to the next decay.";
        G4Exception("TsDeexcitationPhysics::ConstructProcess()", "TsRADIOACTIVE024", JustWarning, msg);
    }
}
