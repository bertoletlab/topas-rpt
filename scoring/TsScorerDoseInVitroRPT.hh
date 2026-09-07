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

#ifndef TsScorerDoseInVitroRPT_hh
#define TsScorerDoseInVitroRPT_hh

#include "TsScorerDoseInVitroRPT.hh"
#include "TsVNtupleScorer.hh"
#include "TsGeometryManager.hh"
#include "G4LogicalVolumeStore.hh"
#include "G4PhysicalVolumeStore.hh"

#include "G4VProcess.hh"


class TsScorerDoseInVitroRPT : public TsVNtupleScorer
{
public:
    TsScorerDoseInVitroRPT(TsParameterManager* pM, TsMaterialManager* mM, TsGeometryManager* gM, TsScoringManager* scM, TsExtensionManager* eM,
                G4String scorerName, G4String quantity, G4String outFileName, G4bool isSubScorer);
    virtual ~TsScorerDoseInVitroRPT();

    G4bool ProcessHits(G4Step*,G4TouchableHistory*);
    void AbsorbResultsFromWorkerScorer(TsVScorer*);
    void UserHookForEndOfRun();
    void SetParameters();

    void SetHeaderDosePerTrack();
    void SetHeaderDosePerHit();

protected:
    // Output variables
    G4float fDose_hist;
    G4float fDose_hist_alpha;
    G4float fDose_hist_elec;
    G4float fDose_hist_gamma;
    G4float fDose_hist_isotope;
    G4int fNucleusNumber;
    G4int fAlphaNumber;
    G4bool fIsFirstAlpha;
    G4bool fIsNewHist;
    G4int fTrackID;
    G4float fDoseAtCulture;
    G4bool fDosePerTrack;
    G4bool fDosePerHit;
    G4double fAccumDosePerTrack;
    G4double fAccumEdepPerTrack;
    std::ofstream fDosePerTrackFile;
    std::ofstream fDosePerHitFile;
    std::ofstream fDosePerTrackHeader;
    std::ofstream fDosePerHitHeader;

private:
    std::map<G4int, G4double> fDoseMap;
    std::map<G4int, G4double> fDoseAlphaMap;
    std::map<G4int, G4double> fDoseElecMap;
    std::map<G4int, G4double> fDoseGammaMap;
    std::map<G4int, G4double> fDoseIsotopeMap;
    std::map<G4int, G4int> fAlphasMap;
    std::map<std::tuple<G4int, G4int, G4int, G4int>, std::vector<G4double>> fDosePerTrackMap;


    TsGeometryManager* fGm;
    G4String fComponentName;
    std::vector<G4VPhysicalVolume*> fPhysicalVolumes;
    G4int fCurrentSeqTime;
    

};
#endif