// Scorer for DoseInVitroRPT
//
// ********************************************************************
// *                                                                  *
// * This file is part of the TOPAS-nBio extensions to the            *
// *   TOPAS Simulation Toolkit.                                      *
// * The TOPAS-nBio extensions are freely available under the license *
// *   agreement set forth at: https://topas-nbio.readthedocs.io/     *
// *                                                                  *
// ********************************************************************
//

#include "TsScorerDoseInVitroRPT.hh"
#include "G4RunManager.hh"

TsScorerDoseInVitroRPT::TsScorerDoseInVitroRPT(TsParameterManager* pM, TsMaterialManager* mM, TsGeometryManager* gM, TsScoringManager* scM, TsExtensionManager* eM,
                                 G4String scorerName, G4String quantity, G4String outFileName, G4bool isSubScorer)
: TsVNtupleScorer(pM, mM, gM, scM, eM, scorerName, quantity, outFileName, isSubScorer), fGm(gM), fComponentName("")
{
    SetParameters();
    fNtuple->RegisterColumnI(&fCurrentSeqTime, "Sequential time");
    fNtuple->RegisterColumnI(&fNucleusNumber, "Nucleus number");
    fNtuple->RegisterColumnF(&fDose_hist, "Dose", "Gy");
    fNtuple->RegisterColumnF(&fDose_hist_alpha, "Dose alpha", "Gy");
    fNtuple->RegisterColumnF(&fDose_hist_elec, "Dose electron", "Gy");
    fNtuple->RegisterColumnF(&fDose_hist_gamma, "Dose gamma", "Gy");
    fNtuple->RegisterColumnF(&fDose_hist_isotope, "Dose isotope", "Gy");
    fNtuple->RegisterColumnI(&fAlphaNumber, "Alpha number");
    fNtuple->RegisterColumnF(&fDoseAtCulture, "Dose at culture", "Gy");
    fAlphaNumber = 0;
    fIsFirstAlpha = true;
    fIsNewHist = true;
    fTrackID = -5;
    fDoseAtCulture = 0;
    fCurrentSeqTime = 0;


}

TsScorerDoseInVitroRPT::~TsScorerDoseInVitroRPT() {}

void TsScorerDoseInVitroRPT::SetParameters()
{

    if (fPm->ParameterExists(GetFullParmName("Component")))
        fComponentName  = fPm->GetStringParameter(GetFullParmName("Component"));
    
    if (fComponentName != "") {fComponent = fGm->GetComponent(fComponentName);}
    else {
        G4cerr << "Topas is exiting due to a serious error in scoring." << G4endl;
        G4cerr << "The scorer DoseInVitroRPT was designed to be executed with a TsCellMonolayer geometry component." << G4endl;
    }

    // This line is getting all physical volume of the Cell Monolayer
    // fPhysicalVolumes[0] is the envelope, i.e. the medium culture
    // fPhysicalVolumes[1] is the cell, without the nucleus
    // fPhysicalVolumes[2] is the nucleus
    fPhysicalVolumes = fComponent->GetAllPhysicalVolumes();

    fDosePerTrack = false;
    if (fPm->ParameterExists(GetFullParmName("DosePerParticle")))
        fDosePerTrack = fPm->GetBooleanParameter(GetFullParmName("DosePerParticle"));

    fDosePerHit = false;
    if (fPm->ParameterExists(GetFullParmName("DosePerInteraction")))
        fDosePerHit = fPm->GetBooleanParameter(GetFullParmName("DosePerInteraction"));

    //Read if dose per track as output
    if (fDosePerTrack) {
        SetHeaderDosePerTrack();
        //Open file and clean it
        G4String fileDosPerTrack = fOutFileName + "_DosePerTrack.phsp";
        std::remove( fileDosPerTrack.c_str());
        fDosePerTrackFile.open(fileDosPerTrack, std::ios::out);
    }

    if(fDosePerHit){
        SetHeaderDosePerHit();
        //Open file and clean it
        G4String fileDosPerHit = fOutFileName + "_DosePerHit.phsp";
        std::remove( fileDosPerHit.c_str());
        fDosePerHitFile.open(fileDosPerHit, std::ios::out);
    }


}

G4bool TsScorerDoseInVitroRPT::ProcessHits(G4Step* aStep, G4TouchableHistory*)
{
    if (!fIsActive) {
        fSkippedWhileInactive++;
        return false;
    }

    G4int trackID = aStep->GetTrack()->GetTrackID();

    if (trackID != fTrackID) {
        fIsFirstAlpha = true;

        fTrackID = trackID;
    }

    // ResolveSolid(aStep);
    // This line is commented out due to an error occurring when:
    // 1. fComponent->IsParameterized() is true (this scorer works with TsCellMonolayer, which is parameterized).
    // 2. physVol->GetParameterisation() returns an invalid pointer (medium and nucleus volume are not parameterizations).
    //
    // This error can be suppressed by setting:
    //    i:Ts/ParameterizationErrorMaxReports = 1
    // However, the ErrorCount variable is of type G4int, with a maximum value of 2147483647.
    // In simulations with many events, this limit is easily reached, causing the error to reappear
    // in every step after that.
    // Resolving the solid is not necessary for this scorer, so it is commented out.
    //
    // See TsVScorer.cc and TsSequenceManager.cc for details.


    //Getting the energy deposited considering the weight of the particle
    G4double edep = aStep->GetTotalEnergyDeposit();
    edep *= aStep->GetPreStepPoint()->GetWeight();
    // Adding the dose to the culture medium
    fDoseAtCulture += edep/(fPhysicalVolumes[0]->GetLogicalVolume()->GetMass());

    if (aStep->GetPreStepPoint()->GetPhysicalVolume() == fPhysicalVolumes[2] && edep != 0) {
        G4int currentCopyNo = aStep->GetPreStepPoint()->GetTouchable()->GetVolume(1)->GetCopyNo(); // Getting the copy number of the nucleus
        G4double dose = edep/(fPhysicalVolumes[2]->GetLogicalVolume()->GetMass());  // Dose to the nucleus
        G4int type = aStep->GetTrack()->GetDefinition()->GetPDGEncoding(); // To clasify the particle

        // Scoring the dose per track
        if (fDosePerTrack) {
            //Write in file
            std::tuple<G4int, G4int, G4int, G4int> key = {fCurrentSeqTime, GetEventID(), currentCopyNo, type};


            if (fDosePerTrackMap.find(key) == fDosePerTrackMap.end()) {
                fDosePerTrackMap[key] = {aStep->GetPreStepPoint()->GetKineticEnergy()/MeV, edep/MeV, dose/gray};
            } else {
                fDosePerTrackMap[key][1] += edep/MeV;
                fDosePerTrackMap[key][2] += dose/gray;
            }

        }

        if (fDosePerHit){
            fDosePerHitFile
                << std::setw(5) << std::left << fCurrentSeqTime << "\t"  // Left-align, width 5
                << std::setw(5) << std::left << GetEventID() << "\t"  // Left-align, width 5         
                << std::setw(5) << std::left << trackID << "\t"          // Left-align, width 5
                << std::setw(5) << std::left << currentCopyNo << "\t"    // Left-align, width 5
                << std::setw(15) << std::left << aStep->GetPreStepPoint()->GetKineticEnergy()/MeV << "\t"  // Left-align, width 15 for floating-point numbers
                << std::setw(15) << std::left << edep/MeV << "\t"             // Left-align, width 15 for floating-point numbers
                << std::setw(15) << std::left << dose/gray << "\t"             // Left-align, width 15 for floating-point numbers
                << std::setw(5) << std::left << type << G4endl;              // Left-align, width 5 for integer or categorical data
        }

        if (fDoseMap.find(currentCopyNo) == fDoseMap.end()) {
            fDoseMap[currentCopyNo] = dose;
            fDoseAlphaMap[currentCopyNo] = 0;
            fDoseElecMap[currentCopyNo] = 0;
            fDoseGammaMap[currentCopyNo] = 0;
            fDoseIsotopeMap[currentCopyNo] = 0;
            fAlphasMap[currentCopyNo] = 0;
            //Accumulate to the corresponding map
            if (abs(type) == 11) fDoseElecMap[currentCopyNo] = dose;
            else if (abs(type) == 22) fDoseGammaMap[currentCopyNo] = dose;
            else if (abs(type)==1000020040) fDoseAlphaMap[currentCopyNo] = dose;
            else if (abs(type)> 1000020040) fDoseIsotopeMap[currentCopyNo] = dose;
            else {
                //G4cout << "Type: " << type << " dose: " << dose << G4endl;
            }

            if (type == 1000020040) {
                //Update alpha in function of the weight
                G4double weight = aStep->GetPreStepPoint()->GetWeight();
                G4int nAlphas = floor(weight);
                //Random number
                G4double r_alpha = G4UniformRand();
                if(r_alpha < weight - nAlphas) nAlphas++; //If r_alpha < weight, add one alpha to the floor value
                fAlphasMap[currentCopyNo] = nAlphas;
            }

        } else {
            fDoseMap[currentCopyNo] += dose;
            //Accumulate to the corresponding map
            if (abs(type) == 11) fDoseElecMap[currentCopyNo] += dose;
            else if (abs(type) == 22) fDoseGammaMap[currentCopyNo] += dose;
            else if (abs(type)==1000020040) fDoseAlphaMap[currentCopyNo] += dose;
            else if (abs(type)> 1000020040) fDoseIsotopeMap[currentCopyNo] += dose;
            else {
                //G4cout << "Type: " << type << " dose: " << dose << G4endl;
            }
            if (type == 1000020040) {
                //Update alpha in function of the weight
                G4double weight = aStep->GetPreStepPoint()->GetWeight();
                G4int nAlphas = floor(weight);
                //Random number
                G4double r_alpha = G4UniformRand();
                if(r_alpha < weight - nAlphas) nAlphas++; //If r_alpha < weight, add one alpha to the floor value
                fAlphasMap[currentCopyNo] += nAlphas;
            }
        }
    }
    
    return true;
}

void TsScorerDoseInVitroRPT::UserHookForEndOfRun()
{

    // Fill the ntuple with the accumulated dose
    for (const auto& entry : fDoseMap) {
        auto nucleusNumber = entry.first;
        auto depositedDose = entry.second;
        fNucleusNumber = nucleusNumber;
        fDose_hist = depositedDose;
        fDose_hist_alpha = fDoseAlphaMap[nucleusNumber];
        fDose_hist_elec = fDoseElecMap[nucleusNumber];
        fDose_hist_gamma = fDoseGammaMap[nucleusNumber];
        fDose_hist_isotope = fDoseIsotopeMap[nucleusNumber];
        fAlphaNumber = fAlphasMap[nucleusNumber];
        fNtuple->Fill();
    }

    // Fill the DosePerTrack map
    if (fDosePerTrack) {
        for (const auto& entry : fDosePerTrackMap) {
            auto key = entry.first;
            auto values = entry.second;
            fDosePerTrackFile
                    << std::setw(3) << std::left << std::get<0>(key) << "\t"  // Left-align the first column 
                    << std::setw(3) << std::left << std::get<1>(key) << "\t"  // Left-align the second column 
                    << std::setw(5) << std::left << std::get<2>(key) << "\t"  // Left-align the third column
                    << std::setw(15) << std::left << std::setprecision(6) << values[0] << "\t"  // Left-align,  with 6 decimals
                    << std::setw(15) << std::left << std::setprecision(6) << values[1] << "\t"  // Left-align, with 6 decimals
                    << std::setw(15) << std::left << std::setprecision(6) << values[2] << "\t"
                    << std::setw(3) << std::left << std::get<3>(key) << "\t" // Left-align
                    << G4endl;
        }
    }

    //Ressiting the variables for new run
    fDoseMap.clear();
    fAlphasMap.clear();
    fDoseAlphaMap.clear();
    fDoseElecMap.clear();
    fDoseGammaMap.clear();
    fDoseIsotopeMap.clear();

    fDoseAtCulture = 0;
    fCurrentSeqTime++;

    fDosePerTrackMap.clear();

}

    

void TsScorerDoseInVitroRPT::AbsorbResultsFromWorkerScorer(TsVScorer* workerScorer)
{    
    TsVNtupleScorer::AbsorbResultsFromWorkerScorer(workerScorer);
    TsScorerDoseInVitroRPT* workerMTScorer = dynamic_cast<TsScorerDoseInVitroRPT*>(workerScorer);
    fDoseAtCulture += workerMTScorer->fDoseAtCulture; // Getting the dose at culture from the working thread
    for (const auto& entry : workerMTScorer->fDoseMap) {
        auto nucleusNumber = entry.first;
        auto depositedDose = entry.second;
        if (fDoseMap.find(nucleusNumber) == fDoseMap.end())
            fDoseMap[nucleusNumber] = depositedDose;
        else
            fDoseMap[nucleusNumber] += depositedDose;
    }
    for (const auto& entry : workerMTScorer->fDoseAlphaMap) {
        auto nucleusNumber = entry.first;
        auto depositedDose = entry.second;
        if (fDoseAlphaMap.find(nucleusNumber) == fDoseAlphaMap.end())
            fDoseAlphaMap[nucleusNumber] = depositedDose;
        else
            fDoseAlphaMap[nucleusNumber] += depositedDose;
    }
    for (const auto& entry : workerMTScorer->fDoseElecMap) {
        auto nucleusNumber = entry.first;
        auto depositedDose = entry.second;
        if (fDoseElecMap.find(nucleusNumber) == fDoseElecMap.end())
            fDoseElecMap[nucleusNumber] = depositedDose;
        else
            fDoseElecMap[nucleusNumber] += depositedDose;
    }
    for (const auto& entry : workerMTScorer->fDoseGammaMap) {
        auto nucleusNumber = entry.first;
        auto depositedDose = entry.second;
        if (fDoseGammaMap.find(nucleusNumber) == fDoseGammaMap.end())
            fDoseGammaMap[nucleusNumber] = depositedDose;
        else
            fDoseGammaMap[nucleusNumber] += depositedDose;
    }
    for (const auto& entry : workerMTScorer->fDoseIsotopeMap) {
        auto nucleusNumber = entry.first;
        auto depositedDose = entry.second;
        if (fDoseIsotopeMap.find(nucleusNumber) == fDoseIsotopeMap.end())
            fDoseIsotopeMap[nucleusNumber] = depositedDose;
        else
            fDoseIsotopeMap[nucleusNumber] += depositedDose;
    }
    for (const auto& entry : workerMTScorer->fAlphasMap) {
        auto nucleusNumber = entry.first;
        auto alphaNumber = entry.second;
        if (fAlphasMap.find(nucleusNumber) == fAlphasMap.end())
            fAlphasMap[nucleusNumber] = alphaNumber;
        else
            fAlphasMap[nucleusNumber] += alphaNumber;
    }

    // Fill the DosePerTrack map
    if (workerMTScorer->fDosePerTrack) {
        for (const auto& entry : workerMTScorer->fDosePerTrackMap) {
            auto key = entry.first;
            auto values = entry.second;

            workerMTScorer->fDosePerTrackFile
                    << std::setw(3) << std::left << std::get<0>(key) << "\t"  // Left-align the first column 
                    << std::setw(3) << std::left << std::get<1>(key) << "\t"  // Left-align the second column 
                    << std::setw(5) << std::left << std::get<2>(key) << "\t"  // Left-align the third column
                    << std::setw(15) << std::left << std::setprecision(6) << values[0] << "\t"  // Left-align,  with 6 decimals
                    << std::setw(15) << std::left << std::setprecision(6) << values[1] << "\t"  // Left-align, with 6 decimals
                    << std::setw(15) << std::left << std::setprecision(6) << values[2] << "\t"
                    << std::setw(3) << std::left << std::get<3>(key) << "\t" // Left-align
                    << G4endl; 

        }
    }

    workerMTScorer->fDoseMap.clear();
    workerMTScorer->fDoseAlphaMap.clear();
    workerMTScorer->fDoseElecMap.clear();
    workerMTScorer->fDoseGammaMap.clear();
    workerMTScorer->fDoseIsotopeMap.clear();
    workerMTScorer->fAlphasMap.clear();
    workerMTScorer->fDoseAtCulture = 0;
    workerMTScorer->fCurrentSeqTime++;

    workerMTScorer->fDosePerTrackMap.clear();
}

void TsScorerDoseInVitroRPT::SetHeaderDosePerTrack()
{
    //Open file and clean it
    G4String fileDosPerTrack = fOutFileName + "_DosePerTrack.header";
    std::remove( fileDosPerTrack.c_str());
    fDosePerTrackHeader.open(fileDosPerTrack, std::ios::out);

    fDosePerTrackHeader << "Columns of data are as follows:" << G4endl;
    fDosePerTrackHeader << "1: Sequential time" << G4endl;
    fDosePerTrackHeader << "2: Event ID (DecayID)" << G4endl;
    fDosePerTrackHeader << "3: Nucleus Number" << G4endl;
    fDosePerTrackHeader << "4: Kinetic Energy [MeV]" << G4endl;
    fDosePerTrackHeader << "5: Energy Deposited [MeV]" << G4endl;
    fDosePerTrackHeader << "6: Dose [Gy]" << G4endl;
    fDosePerTrackHeader << "7: Particle Type (11 -> e-, 1000020040 -> alpha)" << G4endl;

    fDosePerTrackHeader.close();


}

void TsScorerDoseInVitroRPT::SetHeaderDosePerHit()
{
    //Open file and clean it
    G4String fileDosPerHit = fOutFileName + "_DosePerHit.header";
    std::remove( fileDosPerHit.c_str());
    fDosePerHitHeader.open(fileDosPerHit, std::ios::out);

    fDosePerHitHeader << "Columns of data are as follows:" << G4endl;
    fDosePerHitHeader << "1: Sequential time" << G4endl;
    fDosePerHitHeader << "2: Event ID (DecayID)" << G4endl;
    fDosePerHitHeader << "3: Track ID" << G4endl;
    fDosePerHitHeader << "4: Nucleus Number" << G4endl;
    fDosePerHitHeader << "5: Kinetic Energy [MeV]" << G4endl;
    fDosePerHitHeader << "6: Energy Deposited [MeV]" << G4endl;
    fDosePerHitHeader << "7: Dose [Gy]" << G4endl;
    fDosePerHitHeader << "8: Particle Type (11 -> e-, 1000020040 -> alpha)" << G4endl;

}
