//
// Created by A. Bertolet on 7/7/23.
//

#ifndef TsRadioactiveTimeGenerator_hh
#define TsRadioactiveTimeGenerator_hh

#include "TsParameterManager.hh"
#include "TsVGenerator.hh"

#include "TsRadioactiveTimeSource.hh"
#include "TsNormalizationContext.hh"
#include "TsModeFactory.hh"
#include "ITimeDecayKernel.hh"
#include "IResultReporter.hh"
#include "IIsotopicAbundanceReporter.hh"
#include "IModeSummaryReporter.hh"
#include "ITsPositionSampler.hh"
#include "TsSamplingAtVolumes.hh"

#include "G4DecayTable.hh"
#include "G4VDecayChannel.hh"
#include "G4Radioactivation.hh"
#include "G4VPhysicalVolume.hh"
#include "G4ParticleDefinition.hh"
#include "G4DynamicParticle.hh"
#include "G4Navigator.hh"

class G4PhotonEvaporation;
class G4ITDecay;

#include <memory>
#include <set>

class G4Ions;

class TsRadioactiveTimeGenerator : public TsVGenerator
{
public:
    TsRadioactiveTimeGenerator(TsParameterManager* pM, TsGeometryManager* gM, TsGeneratorManager* pgM, G4String sourceName);
    ~TsRadioactiveTimeGenerator();

protected:
    // Two-phase construction. Virtual calls made from a constructor reach only the class being
    // constructed, so a subclass passes deferInitialization = true and calls Initialize() at the
    // end of its own constructor; the public constructor calls it directly.
    TsRadioactiveTimeGenerator(TsParameterManager* pM, TsGeometryManager* gM, TsGeneratorManager* pgM,
                               G4String sourceName, G4bool deferInitialization);
    // Reads the parameters (virtual ResolveParameters), sets up the timeline, the isotopic
    // abundances, the atom count and the first step, calls FinishInitialization(), and writes the
    // first run metadata. Runs once, after the whole object is constructed.
    void Initialize();
    // Hook for subclass set-up that needs the atom count and the first step (both known here).
    virtual void FinishInitialization() {}
    // Stops with a clear message if Initialize() never ran (a subclass that deferred it and did
    // not call it), instead of failing later on an empty timeline or an unset source.
    void RequireInitialized(const char* caller) const;

public:

    void ResolveParameters();
    void GeneratePrimaries(G4Event* );
    void UpdateForNewRun(G4bool force);

    // Reimplement for other sources
    virtual void SetNewPositionForHistory();

    G4ParticleDefinition* PickNextRadionuclide();
    void AddThisParticle(G4Event* anEvent, G4String name, G4double energy, G4double charge, G4ThreeVector momentum, G4bool newHistory);
    void AddRecursivelyToIsotopeListIfRadioactive(G4Event* anEvent, G4ParticleDefinition* particle);

    void IncreaseAbundance(G4ParticleDefinition* particle);
    void DecreaseAbundance(G4ParticleDefinition* particle);
    void ConsolidateAbundanceDifferences();
    void MergeAbundanceDifferencesAcrossThreads();

    void ReadIsotopicAbundanceFile();

    G4VDecayChannel* GetDecayChannelManually(G4DecayTable* decayTable);
    // DeexciteDaughters: cascade excited daughter ions to the ground state (or an isomer).
    // One decay of particle from decayTable (isomeric transitions through fITDecay before Geant4 11.3).
    G4DecayProducts* SampleDecayProducts(const G4ParticleDefinition* particle, G4DecayTable* decayTable);
    G4DecayProducts* DeexciteDaughterIons(G4DecayProducts* products);
    // True when DeexciteDaughters keeps this excited ion as an isomer: its mean life, read as
    // the isotopic abundances read it (MeanLifeOfNucleus), is above IsomerLifetimeThreshold.
    G4bool IsKeptIsomer(const G4Ions* ion) const;
    // FirstForbiddenBetaShape: decay table with first-forbidden beta branches made allowed.
    G4DecayTable* GetDecayTable(const G4ParticleDefinition* particle, const char* where);

    void GetNumberOfIndependentDecaysDuringThisStep();
    void EvolveAbundancesAnalytically(G4double stepDuration);

    void setPositionForHistory(G4ThreeVector NewPosition);
    virtual void GetAdaptedSource();
    void SetUpIsotopicAbundanceFile();
    G4double ComputePrimaryWeight() const;
    G4double ComputeAbundanceWeight() const;
    void AppendCurrentRunMetadata();
    void SaveIsotopicAbundanceSnapshot(G4bool initializeFile) const;
    TsTimeDecayState BuildTimeDecayState() const;
    TsNormalizationContext BuildNormalizationContext() const;
    virtual G4String GetModeMetadataJson() const;
    void ConfigureActivityMapMode();
    void ConfigureTIABinaryMode();
    void WriteActivityMapSummary() const;
    void ConfigureDiffusionMode();
    void ConfigureBioDistMode();
    void SetNewPositionForHistoryDiffusion();
    G4ThreeVector GetInitialPositionForDiffusion();
    G4double GetDiffusionCoefficientForEmitter(const G4String& emitterBaseName) const;
    G4double GetDesorptionProbabilityForEmitter(const G4String& emitterBaseName) const;
    G4double GetBiologicalClearanceProbabilityForEmitter(const G4String& emitterBaseName) const;
    void SaveRunMetadata() const;

    void UpdateCounts(G4String particleName);
    void PrintCounts();

protected:
    std::map<G4String, G4double> fIsotopicAbundance;
    std::map<G4String, G4double> fIsotopicAbundanceDifferencePerStep;
    std::vector<G4String> fOrderedIsotopes;
    G4bool fFirstTimeGettingSource;
    G4bool fInitialized = false;
    G4bool fWriteIsotopicAbundance;
    G4String fWriteIsotopicAbundanceFileName;

    G4double fExcitationEnergy;

    G4double fCurrentTime;
    G4double fInitialTime;
    G4double fFinalTime;
    G4double fNextTime;
    std::vector<G4double> fTimeList;
    G4double fIntraStepTime;

    G4double fInitialActivity;
    G4double fInitialNAtoms;
    G4double fCurrentNAtoms;
    G4double fOriginalHistoriesPerStep;
    G4double fHistoriesPerStep;
    G4double fNumberOfIndependentDecaysDuringFirstStep;
    G4double fNumberOfIndependentDecaysDuringThisStep;
    std::vector<G4double> fFractionsOfActivityAtTime0;
    // FractionOfDecaysAtTime0 read from the abundance file on a restart, for the notice only.
    G4double fFractionOfDecaysFromFile = -1.;

    G4String fEmitterParticleName;

protected:
    TsRadioactiveTimeSource* fSource;

    G4int fRunID;

    // True for the one generator of this source (one exists per worker thread) that writes
    // the shared output files.
    G4bool fIsOutputWriter;

    G4bool fCorrectByNumberOfHistories;

    G4bool fTreatAdditionalDecaysAsNewHistories;
    // AbundanceUpdate = "Analytic": the isotopic abundances follow the Bateman solution of the
    // decay chain each step instead of the decays sampled by the histories.
    G4bool fAnalyticAbundanceUpdate = false;
    G4bool fIncludeWholeDecayChain;
    G4bool fDeexciteDaughters;
    G4double fIsomerLifetimeThreshold;
    G4bool fAllowedShapeForFirstForbidden;
    std::map<const G4ParticleDefinition*, G4DecayTable*> fPatchedDecayTables;

    G4bool fReadIsotopicAbundanceFromFile;
    G4String fReadIsotopicAbundanceFileName;
    G4double fTimeToLoadIsotopicAbundance;
    G4bool fUseFractionOfActivityFromFile;

    G4ThreeVector fPositionForHistory;


    // For volumetric sources
    G4bool fRecursivelyIncludeChildren;
    G4bool fNeedToCalculateExtent;
    G4Navigator* fNavigator;
    std::vector<G4VPhysicalVolume*> fVolumes;
    G4double fXMin, fXMax, fYMin, fYMax, fZMin, fZMax;

    G4int fVerbosity;
    G4Radioactivation* fDecay;
    G4PhotonEvaporation* fPhotonEvaporation;
    G4ITDecay* fITDecay;

    std::vector<G4String> fFilterParticlesToSimulate;
    std::set<G4String> fFilterParticlesToSimulateSet;
    G4String fMode;
    G4bool fWriteRunMetadata;
    G4String fRunMetadataFileName;
    G4bool fWriteModeSummary;
    G4String fModeSummaryFileName;
    std::vector<TsNormalizationContext> fNormalizationHistory;
    std::unique_ptr<ITimeDecayKernel> fTimeDecayKernel;
    std::unique_ptr<IResultReporter> fResultReporter;
    std::unique_ptr<IIsotopicAbundanceReporter> fIsotopicReporter;
    std::unique_ptr<IModeSummaryReporter> fModeSummaryReporter;
    std::unique_ptr<ITsPositionSampler> fModePositionSampler;

    G4bool fActivityMapUseCalibratedCounts;
    G4bool fActivityMapUsedLegacyCalibrationParam;
    G4String fActivityMapCalibratedCountUnits;
    G4double fActivityMapCalibrationScaleBqPerCount;
    G4bool fActivityMapRequireParentGridMatch;
    G4double fActivityMapGridMatchToleranceFraction;
    G4bool fActivityMapWriteSummary;
    G4String fActivityMapSummaryFileName;
    G4double fActivityMapVoxelSizeX;
    G4double fActivityMapVoxelSizeY;
    G4double fActivityMapVoxelSizeZ;
    G4double fActivityMapTotalActivity;
    G4double fActivityMapMinCount;
    G4double fActivityMapMaxCount;
    G4double fActivityMapRawCountSum;
    G4double fActivityMapObservedWidthX;
    G4double fActivityMapObservedWidthY;
    G4double fActivityMapObservedWidthZ;
    G4bool fActivityMapParentExtentAvailable;
    G4double fActivityMapParentWidthX;
    G4double fActivityMapParentWidthY;
    G4double fActivityMapParentWidthZ;
    std::vector<G4ThreeVector> fActivityMapPositions;
    std::vector<G4double> fActivityMapVoxelProbabilities;
    std::vector<G4double> fActivityMapAccumulatedCounts;
    std::vector<unsigned long long> fActivityMapSampledVoxelCounts;
    G4bool fBioDistBackendAvailable;
    G4bool fBioDistModeInvoked;

    G4double fDiffusionSeedRadius;
    G4double fDiffusionSeedHalfLength;
    G4double fDiffusionGenerationStartTime;
    G4double fDiffusionLastDecayTime;
    G4bool fDiffusionHasSeedHalfLength;
    G4bool fDiffusionIsDesorbed;
    G4ThreeVector fDiffusionCurrentPosition;
    std::vector<G4String> fDiffusionRadionuclidesToDiffuse;
    std::vector<G4double> fDiffusionCoefficients;
    std::vector<G4double> fDiffusionBiologicalClearanceRates;
    std::vector<G4double> fDiffusionDesorptionProbabilities;
    TsSamplingAtVolumes fDiffusionSampler;

    // Counts
    G4int fNumberOfElectronsGenerated;
    G4int fNumberOfAlphasGenerated;
    G4int fNumberOfGammasGenerated;
    G4int fNumberOfNeutronsGenerated;
    G4int fNumberOfProtonsGenerated;
    G4int fNumberOfPositronsGenerated;
    G4int fNumberOfOtherGenerated;
};

const G4ThreeVector NO_POSITION = G4ThreeVector(-1e30, -1e30, -1e30);

#endif
