// Particle Generator for RadioactiveTimeSource
//
// Created by A. Bertolet on 7/7/23.
//

#include "TsRadioactiveTimeGenerator.hh"
#include "TsRunMetadataWriter.hh"
#include "TsSourceModeResolver.hh"
#include "TsActivityMapPositionSampler.hh"
#include "TsCompatibilityTimeDecayKernel.hh"
#include "TsRunMetadataReporter.hh"
#include "TsIsotopicAbundanceCsvReporter.hh"
#include "TsModeSummaryReporter.hh"
#include "TsDicomActivityMap.hh"
#include "TsGeometryManager.hh"

#include "TsVGeometryComponent.hh"
#include "TsSource.hh"

#include "G4Radioactivation.hh"
#include "G4DecayTable.hh"
#include "G4DecayProducts.hh"
#include "G4SystemOfUnits.hh"
#include "G4ParticleTable.hh"
#include "G4IonTable.hh"
#include "G4Ions.hh"
#include "G4VPhysicalVolume.hh"
#include "G4VisExtent.hh"
#include "G4TransportationManager.hh"
#include "G4Version.hh"
#include "G4VSolid.hh"
#include "G4PhotonEvaporation.hh"
#include "G4ITDecay.hh"
#include "G4NuclearDecay.hh"

#include <cmath>
#include <cstring>
#include <fstream>
#include <cstdio>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <cctype>

namespace
{
class TsRadioactivationAccessor final : public G4Radioactivation
{
public:
    G4DecayProducts* SampleDecayProducts(const G4ParticleDefinition& particleDefinition, G4DecayTable* decayTable)
    {
        return DoDecay(particleDefinition, decayTable);
    }
};

const G4Ions* AsIonOrThrow(const G4ParticleDefinition* particle, const char* caller)
{
    const G4Ions* ion = dynamic_cast<const G4Ions*>(particle);
    if (!ion) {
        G4ExceptionDescription msg;
        msg << "Expected ion particle for radioactive decay table lookup, got '"
            << (particle ? particle->GetParticleName() : "null") << "'.";
        G4Exception(caller, "TsRADIOACTIVE007", FatalException, msg);
    }
    return ion;
}

G4ParticleDefinition* ResolveParticleDefinitionByName(const G4String& particleName)
{
    G4ParticleTable* particleTable = G4ParticleTable::GetParticleTable();
    if (!particleTable)
        return nullptr;

    G4ParticleDefinition* direct = particleTable->FindParticle(particleName);
    if (direct)
        return direct;

    const size_t openBracket = particleName.find('[');
    const size_t closeBracket = particleName.find(']');
    if (openBracket == std::string::npos || closeBracket == std::string::npos || closeBracket <= openBracket + 1)
        return nullptr;

    const G4String baseName = particleName.substr(0, openBracket);
    G4ParticleDefinition* baseParticle = particleTable->FindParticle(baseName);
    if (!baseParticle)
        return nullptr;

    G4double excitationEnergy = 0.0;
    try {
        excitationEnergy = std::stod(particleName.substr(openBracket + 1, closeBracket - openBracket - 1)) * keV;
    } catch (...) {
        return nullptr;
    }

    G4IonTable* ionTable = particleTable->GetIonTable();
    if (!ionTable)
        return nullptr;

    const G4int Z = baseParticle->GetAtomicNumber();
    const G4int A = baseParticle->GetAtomicMass();
    if (Z <= 0 || A <= 0)
        return nullptr;

    return ionTable->GetIon(Z, A, excitationEnergy);
}

G4String GetBaseEmitterName(const G4String& emitterName)
{
    size_t bracketPosition = emitterName.find("[");
    if (bracketPosition == std::string::npos)
        return emitterName;
    return emitterName.substr(0, bracketPosition);
}
}


TsRadioactiveTimeGenerator::TsRadioactiveTimeGenerator(TsParameterManager* pM, TsGeometryManager* gM, TsGeneratorManager* pgM, G4String sourceName)
:TsVGenerator(pM, gM, pgM, sourceName)
{
    // Time related parameters
    G4double hour = 3600;
    fInitialTime = 0.0;
    fFinalTime = 1000 * 365 * 24 * hour; // 1000 years
    fIntraStepTime = 0.0;
    fDecay = new TsRadioactivationAccessor();
    fPhotonEvaporation = new G4PhotonEvaporation();
    fPhotonEvaporation->RDMForced(true);
    fPhotonEvaporation->SetICM(true);
    fITDecay = new G4ITDecay(fPhotonEvaporation);
    fTimeDecayKernel.reset(new TsCompatibilityTimeDecayKernel());
    fResultReporter.reset(new TsRunMetadataReporter());
    fIsotopicReporter.reset(new TsIsotopicAbundanceCsvReporter());
    fModeSummaryReporter.reset(new TsModeSummaryReporter());

    ResolveParameters();

    fCurrentTime = fInitialTime;
    if (fTimeList.size() == 1){
        fNextTime = fFinalTime;
    }else
        fNextTime = fTimeList[1];

    // Set position to (0,0,0) by default
    fPositionForHistory = G4ThreeVector(0,0,0);

    G4String ionName = fParticleDefinition->GetParticleName();
    G4double meanlife = fParticleDefinition->GetPDGLifeTime()/second;
    if (fExcitationEnergy > 0.0) {
        G4int Z = fParticleDefinition->GetAtomicNumber();
        G4int A = fParticleDefinition->GetAtomicMass();
        meanlife = G4ParticleTable::GetParticleTable()->GetIonTable()->GetLifeTime(Z, A, fExcitationEnergy)/second;
        if (meanlife == -1001/second) {
            G4cout << "WARNING: No data for this excitation energy could be found. The ground state will be used instead." << G4endl;
            meanlife = fParticleDefinition->GetPDGLifeTime()/second;
        }
        else
            ionName = G4ParticleTable::GetParticleTable()->GetIonTable()->GetIonName(Z, A, fExcitationEnergy);
    }

    if (!fReadIsotopicAbundanceFromFile) {
        // Set 100% isotopic abundance for original isotope
        fIsotopicAbundance[ionName] = 1.0;
        fIsotopicAbundanceDifferencePerStep[ionName] = 0.0;
        fOrderedIsotopes.push_back(ionName);
        fFractionsOfActivityAtTime0.push_back(1.0);
    }
    else
        ReadIsotopicAbundanceFile();

    // Get initial number of atoms
    G4double lambda = 1 / meanlife;
    fInitialNAtoms = fInitialActivity / lambda;
    fCurrentNAtoms = fInitialNAtoms;
    if (fUseFractionOfActivityFromFile) {
        G4cout << " ---------- NOTICE --------" << G4endl;
        G4cout << "Even though the original activity was set to " << fInitialActivity << " Bq";
        fInitialActivity *= fFractionsOfActivityAtTime0[0];
        G4cout << ", an actual activity: " << fInitialActivity
               << " Bq will be used since UseFractionOfActivityFromFile was set to True." << G4endl;
    }
    // Get histories / activity weights
    fOriginalHistoriesPerStep = GetSource()->GetNumberOfHistoriesInRun();
    fHistoriesPerStep = fOriginalHistoriesPerStep;
    GetNumberOfIndependentDecaysDuringThisStep();
    fNumberOfIndependentDecaysDuringFirstStep = fNumberOfIndependentDecaysDuringThisStep;

    G4cout << "Histories per step: " << fHistoriesPerStep << G4endl;
    G4cout << "Number of independent decays during first step: " << fNumberOfIndependentDecaysDuringFirstStep
               << G4endl;
    G4cout << "Initial number of atoms: " << fInitialNAtoms << " lambda: " << lambda << G4endl;
    AppendCurrentRunMetadata();
    SaveRunMetadata();
    fFirstTimeGettingSource = true;
    GetAdaptedSource();

    if (fTimeList.size() == 1){
        fSource->SetIsFinalRun(true);
    }

    fNumberOfElectronsGenerated = 0;
    fNumberOfAlphasGenerated = 0;
    fNumberOfGammasGenerated = 0;
    fNumberOfNeutronsGenerated = 0;
    fNumberOfProtonsGenerated = 0;
    fNumberOfPositronsGenerated = 0;
    fNumberOfOtherGenerated = 0;
}

TsRadioactiveTimeGenerator::~TsRadioactiveTimeGenerator()
{
    delete fITDecay;
    delete fPhotonEvaporation;
}

void TsRadioactiveTimeGenerator::GetAdaptedSource() {
    // Get source
    fSource = (TsRadioactiveTimeSource*)GetSource();
    if (fFirstTimeGettingSource)
    {
        if (fWriteIsotopicAbundance) {
           SetUpIsotopicAbundanceFile();
        }
        // Set initial source isotopic abundance and FractionsOfActivityAtTime0
        fSource->SetIsotopicAbundanceNames(fOrderedIsotopes);
        fSource->SetFractionsOfActivityAtTime0(fFractionsOfActivityAtTime0);
    }
    fFirstTimeGettingSource = false;
}

void TsRadioactiveTimeGenerator::SetUpIsotopicAbundanceFile()
{
    SaveIsotopicAbundanceSnapshot(true);
}



void TsRadioactiveTimeGenerator::ResolveParameters()
{
    TsVGenerator::ResolveParameters();
    fRunID = 0;
    if (fPm->ParameterExists("Tf/TimelineStart"))
        fInitialTime = fPm->GetDoubleParameter("Tf/TimelineStart", "Time")/second;
    if (fPm->ParameterExists("Tf/TimelineEnd") && fPm->GetDoubleParameter("Tf/TimelineEnd", "Time") > 0)
        fFinalTime = fPm->GetDoubleParameter("Tf/TimelineEnd", "Time")/second;
    G4double numberOfSequentialTimes = 1;
    if (fPm->ParameterExists("Tf/NumberOfSequentialTimes"))
        numberOfSequentialTimes = fPm->GetIntegerParameter("Tf/NumberOfSequentialTimes");
    G4double timeStep = (fFinalTime - fInitialTime) / numberOfSequentialTimes;
    for (G4int i = 0; i < numberOfSequentialTimes; i++)
        fTimeList.push_back(fInitialTime + i * timeStep);
    fExcitationEnergy = 0;
    if (fPm->ParameterExists(GetFullParmName("ExcitationEnergy")))
        fExcitationEnergy = fPm->GetDoubleParameter(GetFullParmName("ExcitationEnergy"), "Energy");
    fInitialActivity = 1e6;
    if (fPm->ParameterExists(GetFullParmName("InitialActivity")))
        fInitialActivity = fPm->GetUnitlessParameter(GetFullParmName("InitialActivity"));
    fCorrectByNumberOfHistories = true;
    if (fPm->ParameterExists(GetFullParmName("CorrectByNumberOfHistories")))
        fCorrectByNumberOfHistories = fPm->GetBooleanParameter(GetFullParmName("CorrectByNumberOfHistories"));
    fWriteIsotopicAbundance = true;
    if (fPm->ParameterExists(GetFullParmName("WriteIsotopicAbundance"))) {
        fWriteIsotopicAbundance = fPm->GetBooleanParameter(GetFullParmName("WriteIsotopicAbundance"));
        fWriteIsotopicAbundanceFileName = "isotopic_abundance.csv";
        if (fPm->ParameterExists(GetFullParmName("WriteIsotopicAbundanceFileName"))) {
            fWriteIsotopicAbundanceFileName = fPm->GetStringParameter(GetFullParmName("WriteIsotopicAbundanceFileName"));
        }
    }
    fTreatAdditionalDecaysAsNewHistories = true;
    if (fPm->ParameterExists(GetFullParmName("TreatAdditionalDecaysAsNewHistories")))
        fTreatAdditionalDecaysAsNewHistories = fPm->GetBooleanParameter(GetFullParmName("TreatAdditionalDecaysAsNewHistories"));
    fIncludeWholeDecayChain = true;
    if (fPm->ParameterExists(GetFullParmName("IncludeWholeDecayChain")))
        fIncludeWholeDecayChain = fPm->GetBooleanParameter(GetFullParmName("IncludeWholeDecayChain"));
    fRecursivelyIncludeChildren = true;
    if (fPm->ParameterExists(GetFullParmName("RecursivelyIncludeChildren")))
        fRecursivelyIncludeChildren = fPm->GetBooleanParameter(GetFullParmName("RecursivelyIncludeChildren"));
    fVerbosity = 0;
    fReadIsotopicAbundanceFromFile = false;
    if (fPm->ParameterExists(GetFullParmName("ReadIsotopicAbundanceFromFile"))) {
        fReadIsotopicAbundanceFromFile = fPm->GetBooleanParameter(GetFullParmName("ReadIsotopicAbundanceFromFile"));
        fReadIsotopicAbundanceFileName = "isotopic_abundance.csv";
        if (fPm->ParameterExists(GetFullParmName("ReadIsotopicAbundanceFileName"))) {
            fReadIsotopicAbundanceFileName = fPm->GetStringParameter(GetFullParmName("ReadIsotopicAbundanceFileName"));
        }
        fTimeToLoadIsotopicAbundance = -1;
        if (fPm->ParameterExists(GetFullParmName("TimeToLoadIsotopicAbundance"))) {
            fTimeToLoadIsotopicAbundance = fPm->GetDoubleParameter(GetFullParmName("TimeToLoadIsotopicAbundance"), "Time")/second;
        }
        fUseFractionOfActivityFromFile = false;
        if (fPm->ParameterExists(GetFullParmName("UseFractionOfActivityFromFile"))) {
            fUseFractionOfActivityFromFile = fPm->GetBooleanParameter(GetFullParmName("UseFractionOfActivityFromFile"));
        }
    }
    if (fPm->ParameterExists(GetFullParmName("Verbosity")))
        fVerbosity = fPm->GetIntegerParameter(GetFullParmName("Verbosity"));
    TsSourceModeResolver::Resolution modeResolution =
            TsSourceModeResolver::ResolveRequestedMode(fPm, GetFullParmName("Mode"), GetFullParmName("Type"));
    G4bool modeSetExplicitly = modeResolution.modeSetExplicitly;
    fMode = modeResolution.mode;
    TsModeFactory::ModeId modeId = TsModeFactory::ResolveMode(fMode);
    if (modeId == TsModeFactory::ModeId::Unknown && modeSetExplicitly) {
        G4ExceptionDescription msg;
        msg << "Unsupported mode '" << fMode
            << "'. Supported modes: uniform, invitro_bind, activity_map, tia_binary, diffusion, biodist.";
        G4Exception("TsRadioactiveTimeGenerator::ResolveParameters()",
                    "TsRADIOACTIVE002", FatalException, msg);
    } else if (modeId == TsModeFactory::ModeId::Unknown) {
        fMode = "uniform";
    }
    if (modeResolution.usedLegacyTypeMapping) {
        G4cout << "WARNING: Source mode inferred from legacy Type='" << modeResolution.legacyTypeValue
               << "'. Please set So/<Source>/Mode='" << fMode << "' explicitly." << G4endl;
    }
    fWriteRunMetadata = true;
    if (fPm->ParameterExists(GetFullParmName("WriteRunMetadata")))
        fWriteRunMetadata = fPm->GetBooleanParameter(GetFullParmName("WriteRunMetadata"));
    fRunMetadataFileName = "run_metadata.json";
    if (fPm->ParameterExists(GetFullParmName("RunMetadataFileName")))
        fRunMetadataFileName = fPm->GetStringParameter(GetFullParmName("RunMetadataFileName"));
    fWriteModeSummary = false;
    if (fPm->ParameterExists(GetFullParmName("WriteModeSummary")))
        fWriteModeSummary = fPm->GetBooleanParameter(GetFullParmName("WriteModeSummary"));
    fModeSummaryFileName = "run_mode_summary.json";
    if (fPm->ParameterExists(GetFullParmName("ModeSummaryFileName")))
        fModeSummaryFileName = fPm->GetStringParameter(GetFullParmName("ModeSummaryFileName"));

    fActivityMapUseCalibratedCounts = false;
    fActivityMapUsedLegacyCalibrationParam = false;
    fActivityMapCalibratedCountUnits = "unspecified";
    fActivityMapCalibrationScaleBqPerCount = 1.;
    fActivityMapRequireParentGridMatch = true;
    fActivityMapGridMatchToleranceFraction = 0.15;
    fActivityMapWriteSummary = true;
    fActivityMapSummaryFileName = "activity_map_summary.json";
    fActivityMapVoxelSizeX = 0.;
    fActivityMapVoxelSizeY = 0.;
    fActivityMapVoxelSizeZ = 0.;
    fActivityMapTotalActivity = 0.;
    fActivityMapMinCount = 0.;
    fActivityMapMaxCount = 0.;
    fActivityMapRawCountSum = 0.;
    fActivityMapObservedWidthX = 0.;
    fActivityMapObservedWidthY = 0.;
    fActivityMapObservedWidthZ = 0.;
    fActivityMapParentExtentAvailable = false;
    fActivityMapParentWidthX = 0.;
    fActivityMapParentWidthY = 0.;
    fActivityMapParentWidthZ = 0.;
    fActivityMapPositions.clear();
    fActivityMapVoxelProbabilities.clear();
    fActivityMapAccumulatedCounts.clear();
    fActivityMapSampledVoxelCounts.clear();
    fBioDistBackendAvailable = false;
    fBioDistModeInvoked = false;
    fModePositionSampler.reset();
    fDiffusionSeedRadius = 0.;
    fDiffusionSeedHalfLength = 0.;
    fDiffusionGenerationStartTime = 0.;
    fDiffusionLastDecayTime = 0.;
    fDiffusionHasSeedHalfLength = false;
    fDiffusionIsDesorbed = false;
    fDiffusionCurrentPosition = G4ThreeVector(0., 0., 0.);
    fDiffusionRadionuclidesToDiffuse.clear();
    fDiffusionCoefficients.clear();
    fDiffusionBiologicalClearanceRates.clear();
    fDiffusionDesorptionProbabilities.clear();

    if (modeId == TsModeFactory::ModeId::ActivityMap)
        ConfigureActivityMapMode();
    else if (modeId == TsModeFactory::ModeId::TIABinary)
        ConfigureTIABinaryMode();
    else if (modeId == TsModeFactory::ModeId::Diffusion)
        ConfigureDiffusionMode();
    else if (modeId == TsModeFactory::ModeId::BioDist)
        ConfigureBioDistMode();

    G4String* particlesToSimulate;
    fFilterParticlesToSimulate = std::vector<G4String>();
    if (fPm->ParameterExists(GetFullParmName("FilterParticlesToSimulate"))) {
        particlesToSimulate = fPm->GetStringVector(GetFullParmName("FilterParticlesToSimulate"));
        G4int numberOfParticlesToSimulate = fPm->GetVectorLength(GetFullParmName("FilterParticlesToSimulate"));
        for (G4int i = 0; i < numberOfParticlesToSimulate; i++)
            fFilterParticlesToSimulate.push_back(particlesToSimulate[i]);
    }
    fFilterParticlesToSimulateSet.clear();
    for (const auto& particleName : fFilterParticlesToSimulate)
        fFilterParticlesToSimulateSet.insert(particleName);
    fVolumes = fComponent->GetAllPhysicalVolumes(fRecursivelyIncludeChildren);
    fNeedToCalculateExtent = true;
}

void TsRadioactiveTimeGenerator::ConfigureActivityMapMode()
{
    G4String modeParamPath = GetFullParmName("ModeParams/UseCalibratedCounts");
    G4String legacyParamPath = GetFullParmName("UseCalibratedCounts");
    if (fPm->ParameterExists(modeParamPath)) {
        fActivityMapUseCalibratedCounts = fPm->GetBooleanParameter(modeParamPath);
    } else if (fPm->ParameterExists(legacyParamPath)) {
        fActivityMapUseCalibratedCounts = fPm->GetBooleanParameter(legacyParamPath);
        fActivityMapUsedLegacyCalibrationParam = true;
        G4cout << "WARNING: activity_map calibration parameter '" << legacyParamPath
               << "' is deprecated. Use '" << modeParamPath << "'." << G4endl;
    }
    G4String unitsPath = GetFullParmName("ModeParams/CalibratedCountUnits");
    if (fPm->ParameterExists(unitsPath)) {
        fActivityMapCalibratedCountUnits = fPm->GetStringParameter(unitsPath);
    }
    G4String scalePath = GetFullParmName("ModeParams/CalibrationScaleBqPerCount");
    if (fPm->ParameterExists(scalePath)) {
        fActivityMapCalibrationScaleBqPerCount = fPm->GetUnitlessParameter(scalePath);
    }
    G4String requireGridPath = GetFullParmName("ModeParams/RequireParentGridMatch");
    if (fPm->ParameterExists(requireGridPath))
        fActivityMapRequireParentGridMatch = fPm->GetBooleanParameter(requireGridPath);
    G4String tolerancePath = GetFullParmName("ModeParams/GridMatchToleranceFraction");
    if (fPm->ParameterExists(tolerancePath))
        fActivityMapGridMatchToleranceFraction = fPm->GetUnitlessParameter(tolerancePath);
    if (fActivityMapGridMatchToleranceFraction < 0.) {
        G4ExceptionDescription msg;
        msg << "Mode 'activity_map' has negative GridMatchToleranceFraction.";
        G4Exception("TsRadioactiveTimeGenerator::ConfigureActivityMapMode()",
                    "TsRADIOACTIVE016", FatalException, msg);
    }
    G4String writeSummaryPath = GetFullParmName("ModeParams/WriteActivityMapSummary");
    if (fPm->ParameterExists(writeSummaryPath))
        fActivityMapWriteSummary = fPm->GetBooleanParameter(writeSummaryPath);
    G4String summaryFilePath = GetFullParmName("ModeParams/ActivityMapSummaryFileName");
    if (fPm->ParameterExists(summaryFilePath))
        fActivityMapSummaryFileName = fPm->GetStringParameter(summaryFilePath);

    G4String normalizedUnits = fActivityMapCalibratedCountUnits;
    std::transform(normalizedUnits.begin(), normalizedUnits.end(), normalizedUnits.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    normalizedUnits.erase(std::remove_if(normalizedUnits.begin(), normalizedUnits.end(),
                                         [](unsigned char c) { return std::isspace(c); }),
                          normalizedUnits.end());

    if (fActivityMapUseCalibratedCounts) {
        if (!fPm->ParameterExists(unitsPath)) {
            G4ExceptionDescription msg;
            msg << "Mode 'activity_map' with UseCalibratedCounts=True requires "
                << "So/<Source>/ModeParams/CalibratedCountUnits. "
                << "Use 'BqPerMl' when map values are activity concentration, or "
                << "'Counts' plus CalibrationScaleBqPerCount when map values are raw counts.";
            G4Exception("TsRadioactiveTimeGenerator::ConfigureActivityMapMode()",
                        "TsRADIOACTIVE017", FatalException, msg);
        }
        G4bool unitsAreBqPerMl =
                (normalizedUnits == "bqperml" || normalizedUnits == "bq/ml" || normalizedUnits == "bqml");
        G4bool unitsAreCounts = (normalizedUnits == "counts" || normalizedUnits == "count");
        if (!unitsAreBqPerMl && !unitsAreCounts) {
            G4ExceptionDescription msg;
            msg << "Mode 'activity_map' CalibratedCountUnits='" << fActivityMapCalibratedCountUnits
                << "' is invalid. Supported values: BqPerMl, Counts.";
            G4Exception("TsRadioactiveTimeGenerator::ConfigureActivityMapMode()",
                        "TsRADIOACTIVE018", FatalException, msg);
        }
        if (unitsAreCounts && !fPm->ParameterExists(scalePath)) {
            G4ExceptionDescription msg;
            msg << "Mode 'activity_map' with CalibratedCountUnits=Counts requires "
                << "So/<Source>/ModeParams/CalibrationScaleBqPerCount.";
            G4Exception("TsRadioactiveTimeGenerator::ConfigureActivityMapMode()",
                        "TsRADIOACTIVE019", FatalException, msg);
        }
        if (fActivityMapCalibrationScaleBqPerCount <= 0.) {
            G4ExceptionDescription msg;
            msg << "Mode 'activity_map' CalibrationScaleBqPerCount must be > 0.";
            G4Exception("TsRadioactiveTimeGenerator::ConfigureActivityMapMode()",
                        "TsRADIOACTIVE020", FatalException, msg);
        }
        if (unitsAreBqPerMl && !fPm->ParameterExists(scalePath))
            fActivityMapCalibrationScaleBqPerCount = 1.;
    } else {
        if (fPm->ParameterExists(unitsPath) || fPm->ParameterExists(scalePath)) {
            G4cout << "WARNING: activity_map calibration units/scale were provided but "
                   << "UseCalibratedCounts is False; these settings will not affect InitialActivity." << G4endl;
        }
    }

    TsDicomActivityMap* activityMap = dynamic_cast<TsDicomActivityMap*>(fComponent);
    if (!activityMap) {
        G4ExceptionDescription msg;
        msg << "Mode 'activity_map' requires source component type TsDicomActivityMap.";
        G4Exception("TsRadioactiveTimeGenerator::ConfigureActivityMapMode()",
                    "TsRADIOACTIVE003", FatalException, msg);
    }

    fActivityMapVoxelSizeX = activityMap->GetVoxelSizeX();
    fActivityMapVoxelSizeY = activityMap->GetVoxelSizeY();
    fActivityMapVoxelSizeZ = activityMap->GetVoxelSizeZ();

    std::vector<G4Point3D> positions = activityMap->GetSourcePositions();
    std::vector<G4double> counts = activityMap->GetSourceCounts();
    if (positions.empty() || counts.empty() || positions.size() != counts.size()) {
        G4ExceptionDescription msg;
        msg << "Mode 'activity_map' found invalid source voxels. "
            << "positions=" << positions.size() << ", counts=" << counts.size() << ".";
        G4Exception("TsRadioactiveTimeGenerator::ConfigureActivityMapMode()",
                    "TsRADIOACTIVE004", FatalException, msg);
    }
    fActivityMapPositions.clear();
    fActivityMapAccumulatedCounts.clear();
    fActivityMapVoxelProbabilities.clear();
    fActivityMapSampledVoxelCounts.clear();

    G4double accumulated = 0.;
    fActivityMapRawCountSum = 0.;
    fActivityMapMinCount = counts[0];
    fActivityMapMaxCount = counts[0];
    G4double minX = positions[0].x();
    G4double maxX = positions[0].x();
    G4double minY = positions[0].y();
    G4double maxY = positions[0].y();
    G4double minZ = positions[0].z();
    G4double maxZ = positions[0].z();
    for (size_t i = 0; i < counts.size(); i++) {
        fActivityMapMinCount = std::min(fActivityMapMinCount, counts[i]);
        fActivityMapMaxCount = std::max(fActivityMapMaxCount, counts[i]);
        fActivityMapRawCountSum += counts[i];
        minX = std::min(minX, positions[i].x());
        maxX = std::max(maxX, positions[i].x());
        minY = std::min(minY, positions[i].y());
        maxY = std::max(maxY, positions[i].y());
        minZ = std::min(minZ, positions[i].z());
        maxZ = std::max(maxZ, positions[i].z());
        accumulated += counts[i];
        fActivityMapPositions.push_back(G4ThreeVector(positions[i].x(), positions[i].y(), positions[i].z()));
        fActivityMapAccumulatedCounts.push_back(accumulated);
    }
    fActivityMapObservedWidthX = (maxX - minX) + fActivityMapVoxelSizeX;
    fActivityMapObservedWidthY = (maxY - minY) + fActivityMapVoxelSizeY;
    fActivityMapObservedWidthZ = (maxZ - minZ) + fActivityMapVoxelSizeZ;
    if (accumulated <= 0.) {
        G4ExceptionDescription msg;
        msg << "Mode 'activity_map' has non-positive total counts in activity map.";
        G4Exception("TsRadioactiveTimeGenerator::ConfigureActivityMapMode()",
                    "TsRADIOACTIVE005", FatalException, msg);
    }
    for (size_t i = 0; i < fActivityMapAccumulatedCounts.size(); i++)
        fActivityMapAccumulatedCounts[i] /= accumulated;
    fActivityMapVoxelProbabilities.resize(counts.size(), 0.);
    for (size_t i = 0; i < counts.size(); i++)
        fActivityMapVoxelProbabilities[i] = counts[i] / accumulated;
    fActivityMapSampledVoxelCounts.assign(counts.size(), 0ULL);

    const G4double componentWidthX = fComponent->GetFullWidth(0);
    const G4double componentWidthY = fComponent->GetFullWidth(1);
    const G4double componentWidthZ = fComponent->GetFullWidth(2);
    const G4double hardTol = 1.e-6;
    if (fActivityMapObservedWidthX > componentWidthX * (1. + hardTol) ||
        fActivityMapObservedWidthY > componentWidthY * (1. + hardTol) ||
        fActivityMapObservedWidthZ > componentWidthZ * (1. + hardTol)) {
        G4ExceptionDescription msg;
        msg << "Mode 'activity_map' voxel cloud extent exceeds component extent. "
            << "Observed(mm)=[" << fActivityMapObservedWidthX / mm << ", "
            << fActivityMapObservedWidthY / mm << ", " << fActivityMapObservedWidthZ / mm
            << "], component(mm)=[" << componentWidthX / mm << ", "
            << componentWidthY / mm << ", " << componentWidthZ / mm << "].";
        G4Exception("TsRadioactiveTimeGenerator::ConfigureActivityMapMode()",
                    "TsRADIOACTIVE021", FatalException, msg);
    }

    fActivityMapParentExtentAvailable = false;
    const G4String parentNamePath = "Ge/" + fComponent->GetName() + "/Parent";
    if (fPm->ParameterExists(parentNamePath)) {
        G4String parentName = fPm->GetStringParameter(parentNamePath);
        TsVGeometryComponent* parentComponent = fGm ? fGm->GetComponent(parentName) : nullptr;
        if (parentComponent) {
            const G4VisExtent& parentExtent = parentComponent->GetExtent();
            fActivityMapParentWidthX = parentExtent.GetXmax() - parentExtent.GetXmin();
            fActivityMapParentWidthY = parentExtent.GetYmax() - parentExtent.GetYmin();
            fActivityMapParentWidthZ = parentExtent.GetZmax() - parentExtent.GetZmin();
            fActivityMapParentExtentAvailable =
                    (fActivityMapParentWidthX > 0. && fActivityMapParentWidthY > 0. && fActivityMapParentWidthZ > 0.);
        }
    }
    if (fActivityMapRequireParentGridMatch) {
        if (!fActivityMapParentExtentAvailable) {
            G4ExceptionDescription msg;
            msg << "Mode 'activity_map' RequireParentGridMatch=True but parent extent is unavailable.";
            G4Exception("TsRadioactiveTimeGenerator::ConfigureActivityMapMode()",
                        "TsRADIOACTIVE022", FatalException, msg);
        }
        auto relativeMismatch = [](G4double observed, G4double reference) {
            if (reference <= 0.)
                return 0.;
            return std::fabs(observed - reference) / reference;
        };
        const G4double mismatchX = relativeMismatch(fActivityMapObservedWidthX, fActivityMapParentWidthX);
        const G4double mismatchY = relativeMismatch(fActivityMapObservedWidthY, fActivityMapParentWidthY);
        const G4double mismatchZ = relativeMismatch(fActivityMapObservedWidthZ, fActivityMapParentWidthZ);
        if (mismatchX > fActivityMapGridMatchToleranceFraction ||
            mismatchY > fActivityMapGridMatchToleranceFraction ||
            mismatchZ > fActivityMapGridMatchToleranceFraction) {
            G4ExceptionDescription msg;
            msg << "Mode 'activity_map' parent grid mismatch exceeds tolerance "
                << fActivityMapGridMatchToleranceFraction << ". "
                << "Mismatch fractions [x,y,z]=[" << mismatchX << ", " << mismatchY << ", " << mismatchZ << "]. "
                << "Observed(mm)=[" << fActivityMapObservedWidthX / mm << ", "
                << fActivityMapObservedWidthY / mm << ", " << fActivityMapObservedWidthZ / mm << "], "
                << "Parent(mm)=[" << fActivityMapParentWidthX / mm << ", "
                << fActivityMapParentWidthY / mm << ", " << fActivityMapParentWidthZ / mm << "].";
            G4Exception("TsRadioactiveTimeGenerator::ConfigureActivityMapMode()",
                        "TsRADIOACTIVE023", FatalException, msg);
        }
    }

    if (fActivityMapUseCalibratedCounts) {
        G4double voxelVolumeInMl = fActivityMapVoxelSizeX / cm * fActivityMapVoxelSizeY / cm * fActivityMapVoxelSizeZ / cm;
        fActivityMapTotalActivity = 0.;
        for (size_t i = 0; i < counts.size(); i++) {
            G4double activityDensityBqPerMl = counts[i] * fActivityMapCalibrationScaleBqPerCount;
            if (activityDensityBqPerMl < 0.) {
                G4ExceptionDescription msg;
                msg << "Mode 'activity_map' produced negative calibrated activity density at voxel " << i << ".";
                G4Exception("TsRadioactiveTimeGenerator::ConfigureActivityMapMode()",
                            "TsRADIOACTIVE024", FatalException, msg);
            }
            fActivityMapTotalActivity += activityDensityBqPerMl * voxelVolumeInMl;
        }
        if (fActivityMapTotalActivity <= 0.) {
            G4ExceptionDescription msg;
            msg << "Mode 'activity_map' calibration enabled but derived total activity is non-positive.";
            G4Exception("TsRadioactiveTimeGenerator::ConfigureActivityMapMode()",
                        "TsRADIOACTIVE006", FatalException, msg);
        }
        fInitialActivity = fActivityMapTotalActivity;
        G4cout << "NOTICE: activity_map is using calibrated counts. "
               << "InitialActivity overridden to " << fInitialActivity << " Bq "
               << "(units=" << fActivityMapCalibratedCountUnits
               << ", scale_bq_per_count=" << fActivityMapCalibrationScaleBqPerCount << ")." << G4endl;
    }

    TsActivityMapPositionSampler::Config samplerConfig;
    samplerConfig.positions = &fActivityMapPositions;
    samplerConfig.accumulatedCounts = &fActivityMapAccumulatedCounts;
    samplerConfig.voxelSizeX = fActivityMapVoxelSizeX;
    samplerConfig.voxelSizeY = fActivityMapVoxelSizeY;
    samplerConfig.voxelSizeZ = fActivityMapVoxelSizeZ;
    fModePositionSampler.reset(new TsActivityMapPositionSampler(samplerConfig));
}

void TsRadioactiveTimeGenerator::ConfigureTIABinaryMode()
{
    // Read BTIA file path from parameter
    G4String btiaParamPath = GetFullParmName("ModeParams/BTIAFile");
    if (!fPm->ParameterExists(btiaParamPath)) {
        G4ExceptionDescription msg;
        msg << "Mode 'tia_binary' requires So/<Source>/ModeParams/BTIAFile.";
        G4Exception("TsRadioactiveTimeGenerator::ConfigureTIABinaryMode()",
                    "TsRADIOACTIVE030", FatalException, msg);
    }
    G4String btiaPath = fPm->GetStringParameter(btiaParamPath);

    // Open and read header
    std::ifstream file(btiaPath, std::ios::binary);
    if (!file.is_open()) {
        G4ExceptionDescription msg;
        msg << "Mode 'tia_binary' cannot open file: " << btiaPath;
        G4Exception("TsRadioactiveTimeGenerator::ConfigureTIABinaryMode()",
                    "TsRADIOACTIVE031", FatalException, msg);
    }

    // Read 80-byte header
    const size_t headerSize = 80;
    char headerBuf[80];
    file.read(headerBuf, headerSize);
    if (!file.good()) {
        G4ExceptionDescription msg;
        msg << "Mode 'tia_binary' failed to read " << headerSize << "-byte header from " << btiaPath;
        G4Exception("TsRadioactiveTimeGenerator::ConfigureTIABinaryMode()",
                    "TsRADIOACTIVE032", FatalException, msg);
    }

    // Validate magic "BTIA"
    if (headerBuf[0] != 'B' || headerBuf[1] != 'T' || headerBuf[2] != 'I' || headerBuf[3] != 'A') {
        G4ExceptionDescription msg;
        msg << "Mode 'tia_binary' invalid BTIA magic in file: " << btiaPath;
        G4Exception("TsRadioactiveTimeGenerator::ConfigureTIABinaryMode()",
                    "TsRADIOACTIVE033", FatalException, msg);
    }

    // Parse header fields (all little-endian, matching Python struct "<4sHH3II3d3d")
    // Offsets: magic(4) version(6) data_type(8) nx(12) ny(16) nz(20) unit(24)
    //          voxel_x(28) voxel_y(36) voxel_z(44) origin_x(52) origin_y(60) origin_z(68)
    uint16_t version, dataType;
    uint32_t nx, ny, nz, unitCode;
    double voxelX, voxelY, voxelZ, originX, originY, originZ;

    std::memcpy(&version,  &headerBuf[4],  sizeof(uint16_t));
    std::memcpy(&dataType, &headerBuf[6],  sizeof(uint16_t));
    std::memcpy(&nx,       &headerBuf[8],  sizeof(uint32_t));
    std::memcpy(&ny,       &headerBuf[12], sizeof(uint32_t));
    std::memcpy(&nz,       &headerBuf[16], sizeof(uint32_t));
    std::memcpy(&unitCode, &headerBuf[20], sizeof(uint32_t));
    std::memcpy(&voxelX,   &headerBuf[24], sizeof(double));
    std::memcpy(&voxelY,   &headerBuf[32], sizeof(double));
    std::memcpy(&voxelZ,   &headerBuf[40], sizeof(double));
    std::memcpy(&originX,  &headerBuf[48], sizeof(double));
    std::memcpy(&originY,  &headerBuf[56], sizeof(double));
    std::memcpy(&originZ,  &headerBuf[64], sizeof(double));

    if (version != 1) {
        G4ExceptionDescription msg;
        msg << "Mode 'tia_binary' unsupported BTIA version " << version << " (expected 1).";
        G4Exception("TsRadioactiveTimeGenerator::ConfigureTIABinaryMode()",
                    "TsRADIOACTIVE034", FatalException, msg);
    }
    if (dataType != 0 && dataType != 1) {
        G4ExceptionDescription msg;
        msg << "Mode 'tia_binary' invalid data_type " << dataType << " in BTIA header.";
        G4Exception("TsRadioactiveTimeGenerator::ConfigureTIABinaryMode()",
                    "TsRADIOACTIVE035", FatalException, msg);
    }

    size_t nVoxels = (size_t)nx * (size_t)ny * (size_t)nz;
    size_t bytesPerValue = (dataType == 0) ? 4 : 8;  // float32 vs float64

    // Read voxel data
    std::vector<double> values(nVoxels);
    if (dataType == 0) {
        // float32
        std::vector<float> buf(nVoxels);
        file.read(reinterpret_cast<char*>(buf.data()), nVoxels * sizeof(float));
        if (!file.good()) {
            G4ExceptionDescription msg;
            msg << "Mode 'tia_binary' failed to read " << nVoxels << " float32 values.";
            G4Exception("TsRadioactiveTimeGenerator::ConfigureTIABinaryMode()",
                        "TsRADIOACTIVE036", FatalException, msg);
        }
        for (size_t i = 0; i < nVoxels; i++)
            values[i] = static_cast<double>(buf[i]);
    } else {
        // float64
        file.read(reinterpret_cast<char*>(values.data()), nVoxels * sizeof(double));
        if (!file.good()) {
            G4ExceptionDescription msg;
            msg << "Mode 'tia_binary' failed to read " << nVoxels << " float64 values.";
            G4Exception("TsRadioactiveTimeGenerator::ConfigureTIABinaryMode()",
                        "TsRADIOACTIVE037", FatalException, msg);
        }
    }
    file.close();

    // Convert voxel sizes from mm to Geant4 internal units (mm)
    fActivityMapVoxelSizeX = voxelX * mm;
    fActivityMapVoxelSizeY = voxelY * mm;
    fActivityMapVoxelSizeZ = voxelZ * mm;

    // Build position list and cumulative probability from dense 3D data.
    // Data is x-fastest: linear = ix + nx * (iy + ny * iz)
    fActivityMapPositions.clear();
    fActivityMapAccumulatedCounts.clear();
    fActivityMapVoxelProbabilities.clear();
    fActivityMapSampledVoxelCounts.clear();

    G4double accumulated = 0.;
    fActivityMapRawCountSum = 0.;
    fActivityMapMinCount = 1e30;
    fActivityMapMaxCount = 0.;

    for (size_t iz = 0; iz < nz; iz++) {
        for (size_t iy = 0; iy < ny; iy++) {
            for (size_t ix = 0; ix < nx; ix++) {
                size_t idx = ix + nx * (iy + ny * iz);
                G4double val = values[idx];
                if (val <= 0.)
                    continue;

                // Position: voxel center in mm
                G4double px = originX + (ix + 0.5) * voxelX;
                G4double py = originY + (iy + 0.5) * voxelY;
                G4double pz = originZ + (iz + 0.5) * voxelZ;

                fActivityMapMinCount = std::min(fActivityMapMinCount, val);
                fActivityMapMaxCount = std::max(fActivityMapMaxCount, val);
                fActivityMapRawCountSum += val;
                accumulated += val;
                fActivityMapPositions.push_back(G4ThreeVector(px * mm, py * mm, pz * mm));
                fActivityMapAccumulatedCounts.push_back(accumulated);
            }
        }
    }

    if (fActivityMapPositions.empty() || accumulated <= 0.) {
        G4ExceptionDescription msg;
        msg << "Mode 'tia_binary' found no non-zero voxels in " << btiaPath;
        G4Exception("TsRadioactiveTimeGenerator::ConfigureTIABinaryMode()",
                    "TsRADIOACTIVE038", FatalException, msg);
    }

    // Normalize cumulative distribution to [0, 1]
    for (size_t i = 0; i < fActivityMapAccumulatedCounts.size(); i++)
        fActivityMapAccumulatedCounts[i] /= accumulated;

    // Build probability array for diagnostics
    fActivityMapVoxelProbabilities.resize(fActivityMapPositions.size(), 0.);
    G4double prevAccum = 0.;
    for (size_t i = 0; i < fActivityMapAccumulatedCounts.size(); i++) {
        fActivityMapVoxelProbabilities[i] = fActivityMapAccumulatedCounts[i] - prevAccum;
        prevAccum = fActivityMapAccumulatedCounts[i];
    }
    fActivityMapSampledVoxelCounts.assign(fActivityMapPositions.size(), 0ULL);

    // Set total activity from .btia data (sum of A₀ values in Bq)
    fActivityMapTotalActivity = accumulated;
    fInitialActivity = accumulated;

    G4cout << "NOTICE: tia_binary mode loaded " << fActivityMapPositions.size()
           << " source voxels from " << btiaPath
           << " (grid " << nx << "x" << ny << "x" << nz
           << ", total A0 = " << fInitialActivity << " Bq)." << G4endl;

    // Create position sampler (reuse existing activity map sampler)
    TsActivityMapPositionSampler::Config samplerConfig;
    samplerConfig.positions = &fActivityMapPositions;
    samplerConfig.accumulatedCounts = &fActivityMapAccumulatedCounts;
    samplerConfig.voxelSizeX = fActivityMapVoxelSizeX;
    samplerConfig.voxelSizeY = fActivityMapVoxelSizeY;
    samplerConfig.voxelSizeZ = fActivityMapVoxelSizeZ;
    fModePositionSampler.reset(new TsActivityMapPositionSampler(samplerConfig));
}

void TsRadioactiveTimeGenerator::ConfigureDiffusionMode()
{
    G4String modePrefix = GetFullParmName("ModeParams/");
    G4String legacyPrefix = GetFullParmName("");

    auto getModeOrLegacyDouble = [&](const G4String& modeSuffix, const G4String& legacySuffix, const char* unit) -> std::pair<G4bool, G4double> {
        G4String modePath = modePrefix + modeSuffix;
        G4String legacyPath = legacyPrefix + legacySuffix;
        if (fPm->ParameterExists(modePath))
            return std::make_pair(true, fPm->GetDoubleParameter(modePath, unit));
        if (fPm->ParameterExists(legacyPath)) {
            G4cout << "WARNING: diffusion parameter '" << legacyPath << "' is deprecated. Use '" << modePath << "'." << G4endl;
            return std::make_pair(true, fPm->GetDoubleParameter(legacyPath, unit));
        }
        return std::make_pair(false, 0.);
    };

    auto getModeOrLegacyDoubleVector = [&](const G4String& modeSuffix, const G4String& legacySuffix, const char* unit, std::vector<G4double>& output) {
        G4String modePath = modePrefix + modeSuffix;
        G4String legacyPath = legacyPrefix + legacySuffix;
        if (fPm->ParameterExists(modePath)) {
            G4double* values = fPm->GetDoubleVector(modePath, unit);
            G4int nValues = fPm->GetVectorLength(modePath);
            output.assign(values, values + nValues);
            return;
        }
        if (fPm->ParameterExists(legacyPath)) {
            G4cout << "WARNING: diffusion parameter '" << legacyPath << "' is deprecated. Use '" << modePath << "'." << G4endl;
            G4double* values = fPm->GetDoubleVector(legacyPath, unit);
            G4int nValues = fPm->GetVectorLength(legacyPath);
            output.assign(values, values + nValues);
        }
    };

    auto getModeOrLegacyUnitlessVector = [&](const G4String& modeSuffix, const G4String& legacySuffix, std::vector<G4double>& output) {
        G4String modePath = modePrefix + modeSuffix;
        G4String legacyPath = legacyPrefix + legacySuffix;
        if (fPm->ParameterExists(modePath)) {
            G4double* values = fPm->GetUnitlessVector(modePath);
            G4int nValues = fPm->GetVectorLength(modePath);
            output.assign(values, values + nValues);
            return;
        }
        if (fPm->ParameterExists(legacyPath)) {
            G4cout << "WARNING: diffusion parameter '" << legacyPath << "' is deprecated. Use '" << modePath << "'." << G4endl;
            G4double* values = fPm->GetUnitlessVector(legacyPath);
            G4int nValues = fPm->GetVectorLength(legacyPath);
            output.assign(values, values + nValues);
        }
    };

    auto radius = getModeOrLegacyDouble("RMax", "RMax", "Length");
    if (radius.first)
        fDiffusionSeedRadius = radius.second;
    auto halfLength = getModeOrLegacyDouble("Hzlength", "Hzlength", "Length");
    if (halfLength.first) {
        fDiffusionSeedHalfLength = halfLength.second;
        fDiffusionHasSeedHalfLength = true;
    }

    G4String modeRadionuclides = modePrefix + "RadionuclidesToDiffuse";
    G4String legacyRadionuclides = legacyPrefix + "RadionuclidesToDiffuse";
    if (fPm->ParameterExists(modeRadionuclides)) {
        G4String* values = fPm->GetStringVector(modeRadionuclides);
        G4int nValues = fPm->GetVectorLength(modeRadionuclides);
        fDiffusionRadionuclidesToDiffuse.assign(values, values + nValues);
    } else if (fPm->ParameterExists(legacyRadionuclides)) {
        G4cout << "WARNING: diffusion parameter '" << legacyRadionuclides << "' is deprecated. Use '" << modeRadionuclides << "'." << G4endl;
        G4String* values = fPm->GetStringVector(legacyRadionuclides);
        G4int nValues = fPm->GetVectorLength(legacyRadionuclides);
        fDiffusionRadionuclidesToDiffuse.assign(values, values + nValues);
    }

    getModeOrLegacyDoubleVector("DiffusionCoefficients", "DiffusionCoefficients", "surface perTime", fDiffusionCoefficients);
    getModeOrLegacyUnitlessVector("BiologicalClearanceRates", "BiologicalClearanceRates", fDiffusionBiologicalClearanceRates);
    getModeOrLegacyUnitlessVector("DesorptionProbabilities", "DesorptionProbabilities", fDiffusionDesorptionProbabilities);

    G4String modeStart = modePrefix + "TimeGenerationStart";
    if (fPm->ParameterExists(modeStart))
        fDiffusionGenerationStartTime = fPm->GetDoubleParameter(modeStart, "Time") / second;
    else if (fPm->ParameterExists("Tf/TimeGenerationStart"))
        fDiffusionGenerationStartTime = fPm->GetDoubleParameter("Tf/TimeGenerationStart", "Time") / second;

    if (fDiffusionRadionuclidesToDiffuse.empty()) {
        G4cout << "WARNING: diffusion mode has empty RadionuclidesToDiffuse list. Position diffusion will be inactive." << G4endl;
    }
    if (fDiffusionCoefficients.size() < fDiffusionRadionuclidesToDiffuse.size())
        fDiffusionCoefficients.resize(fDiffusionRadionuclidesToDiffuse.size(), 0.);
    if (fDiffusionBiologicalClearanceRates.size() < fDiffusionRadionuclidesToDiffuse.size())
        fDiffusionBiologicalClearanceRates.resize(fDiffusionRadionuclidesToDiffuse.size(), 0.);
    if (fDiffusionDesorptionProbabilities.size() < fDiffusionRadionuclidesToDiffuse.size())
        fDiffusionDesorptionProbabilities.resize(fDiffusionRadionuclidesToDiffuse.size(), 0.);
}

void TsRadioactiveTimeGenerator::ConfigureBioDistMode()
{
    fBioDistModeInvoked = true;
    // Placeholder capability flag. This extension currently does not compile
    // biodistribution backend classes unless explicitly ported/available.
    fBioDistBackendAvailable = false;
}

void TsRadioactiveTimeGenerator::GeneratePrimaries(G4Event* anEvent)
{
    fIntraStepTime = 0.0;
    if (CurrentSourceHasGeneratedEnough())  return;
    if (fMode == "biodist" && !fBioDistBackendAvailable) {
        G4ExceptionDescription msg;
        msg << "Mode 'biodist' was requested but biodistribution backend is unavailable in this build. "
            << "Rebuild with biodistribution backend support or use another mode.";
        G4Exception("TsRadioactiveTimeGenerator::GeneratePrimaries()",
                    "TsRADIOACTIVE008", FatalException, msg);
    }
    //G4Radioactivation* decay = new G4Radioactivation();

    fParticleDefinition = PickNextRadionuclide();
    if (fVerbosity > 0) G4cout << "Decaying: " << fParticleDefinition->GetParticleName() << G4endl;
    fEmitterParticleName = fParticleDefinition->GetParticleName();

    // Load decay table
    G4DecayTable* decayTable = fDecay->LoadDecayTable(AsIonOrThrow(
            fParticleDefinition, "TsRadioactiveTimeGenerator::GeneratePrimaries"));
    G4DecayProducts* decayProducts = nullptr;
#if G4VERSION_NUMBER >= 1130
    decayProducts =
            static_cast<TsRadioactivationAccessor*>(fDecay)->SampleDecayProducts(*fParticleDefinition, decayTable);
#else
    G4VDecayChannel* decayChannel = decayTable->SelectADecayChannel();
    if (decayChannel == nullptr)
        decayChannel = GetDecayChannelManually(decayTable);
    if (decayChannel == nullptr) {
        if (fVerbosity > 0)
            G4cout << "WARNING: Could not select decay channel for "
                   << fParticleDefinition->GetParticleName() << ". Skipping this history." << G4endl;
        return;
    }
    // IT channels in decay tables lack a G4PhotonEvaporation pointer,
    // so calling DecayIt() on them directly causes a null dereference.
    // Mirror G4RadioactiveDecay behavior: use our own fITDecay member
    // which was initialized with a proper G4PhotonEvaporation.
    G4NuclearDecay* nucDecay = dynamic_cast<G4NuclearDecay*>(decayChannel);
    if (nucDecay && nucDecay->GetDecayMode() == IT) {
        fITDecay->SetupDecay(fParticleDefinition);
        decayProducts = fITDecay->DecayIt(0.0);
    } else {
        decayProducts = decayChannel->DecayIt();
    }
#endif
    if (!decayProducts) {
        if (fVerbosity > 0)
            G4cout << "WARNING: Null decay products for "
                   << fParticleDefinition->GetParticleName() << ". Skipping this history." << G4endl;
        return;
    }

    const G4int nDecayProducts = decayProducts->entries();
    std::vector<G4String> particleNames;
    std::vector<G4double> particleEnergies;
    std::vector<G4double> particleCharges;
    std::vector<G4ThreeVector> particleMomenta;
    particleNames.reserve(nDecayProducts);
    particleEnergies.reserve(nDecayProducts);
    particleCharges.reserve(nDecayProducts);
    particleMomenta.reserve(nDecayProducts);
    for (G4int i = 0; i < nDecayProducts; i++)
    {
        G4DynamicParticle* dp = decayProducts[0][i];
        particleNames.push_back(dp->GetDefinition()->GetParticleName());
        particleEnergies.push_back(dp->GetKineticEnergy());
        particleCharges.push_back(dp->GetCharge());
        particleMomenta.push_back(dp->GetMomentumDirection());
    }
    delete decayProducts;

    if (fVerbosity > 1) G4cout << fParticleDefinition->GetParticleName() << " decays and abundance is reduced!" << G4endl;
    // Add these products to the list of particles to be generated
    G4bool newHistory = true;
    for (G4int i = 0; i < (G4int)particleNames.size(); i++)
    {
        if (fVerbosity > 0) G4cout << "  Decay product [" << i+1 << "]: "  << particleNames[i] << " with kinetic energy " << particleEnergies[i] << " MeV " << G4endl;
        if (fFilterParticlesToSimulateSet.empty() || fFilterParticlesToSimulateSet.find(particleNames[i]) == fFilterParticlesToSimulateSet.end())
        {
            AddThisParticle(anEvent, particleNames[i], particleEnergies[i], particleCharges[i], particleMomenta[i], newHistory);
            if (fIncludeWholeDecayChain){
                G4ParticleDefinition* particle = ResolveParticleDefinitionByName(particleNames[i]);
                if (particle)
                    AddRecursivelyToIsotopeListIfRadioactive(anEvent, particle);
                newHistory = false;
            }
        }
         
    }
    
    DecreaseAbundance(fParticleDefinition);
    AddPrimariesToEvent(anEvent);
    setPositionForHistory(G4ThreeVector(0, 0, 0));
    if (fVerbosity > 2)
        PrintCounts();
}

void TsRadioactiveTimeGenerator::AddThisParticle(G4Event* anEvent, G4String name, G4double energy, G4double charge, G4ThreeVector momentum, G4bool newHistory=false)
{
    TsPrimaryParticle p;
    if (newHistory && fPositionForHistory != NO_POSITION){
        SetNewPositionForHistory();
    }

    if (fPositionForHistory == NO_POSITION) return;
    p.posX = fPositionForHistory.x();
    p.posY = fPositionForHistory.y();
    p.posZ = fPositionForHistory.z();
    p.particleDefinition = ResolveParticleDefinitionByName(name);
    if (!p.particleDefinition) {
        if (fVerbosity > 0)
            G4cout << "WARNING: Could not resolve particle definition for '" << name << "'. Skipping." << G4endl;
        return;
    }
    p.kEnergy = energy;
    p.isNewHistory = newHistory;
    p.isOpticalPhoton = false;
    p.ionCharge = charge;
    p.weight = ComputePrimaryWeight();
    p.dCos1 = momentum.x();
    p.dCos2 = momentum.y();
    p.dCos3 = momentum.z();
    TransformPrimaryForComponent(&p);
    if (fMode == "diffusion" && fIntraStepTime < fDiffusionGenerationStartTime)
        return;
    GenerateOnePrimary(anEvent, p);
    UpdateCounts(p.particleDefinition->GetParticleName());
}

void TsRadioactiveTimeGenerator::AddRecursivelyToIsotopeListIfRadioactive(G4Event* anEvent, G4ParticleDefinition* particle)
{
    static thread_local G4int recursionDepth = 0;
    struct RecursionGuard {
        G4int& depth;
        explicit RecursionGuard(G4int& d) : depth(d) { depth++; }
        ~RecursionGuard() { depth--; }
    };
    if (recursionDepth > 128) {
        if (fVerbosity > 0)
            G4cout << "WARNING: recursive decay depth exceeded safety limit for "
                   << (particle ? particle->GetParticleName() : G4String("null")) << "." << G4endl;
        return;
    }
    RecursionGuard depthGuard(recursionDepth);

    if (!particle)
        return;

    if (fVerbosity > 2) G4cout << "Now checking... " << particle->GetParticleName() << G4endl;
    //G4Radioactivation* decay = new G4Radioactivation();

    G4bool isNucleus = particle->GetParticleType() == "nucleus";
    if (fVerbosity > 2) G4cout << "is nucleus? " << isNucleus << G4endl;
    if (!isNucleus) return;

    // Some excited-state ions can crash decay-channel selection in recursive handling.
    // For chain propagation we fall back to the ground state of the same Z/A.
    if (const auto* ion = dynamic_cast<const G4Ions*>(particle)) {
        if (ion->GetExcitationEnergy() > 0.) {
            G4IonTable* ionTable = G4ParticleTable::GetParticleTable()->GetIonTable();
            if (ionTable) {
                G4ParticleDefinition* ground = ionTable->GetIon(ion->GetAtomicNumber(), ion->GetAtomicMass(), 0.);
                if (ground) {
                    if (fVerbosity > 1)
                        G4cout << "WARNING: recursive decay normalized excited isotope "
                               << particle->GetParticleName() << " to ground state "
                               << ground->GetParticleName() << "." << G4endl;
                    particle = ground;
                }
            }
        }
    }

    G4DecayTable* decayTable = fDecay->LoadDecayTable(AsIonOrThrow(
            particle, "TsRadioactiveTimeGenerator::AddRecursivelyToIsotopeListIfRadioactive"));
    if (!decayTable) {
        if (fVerbosity > 1)
            G4cout << "No decay table available for " << particle->GetParticleName()
                   << "; treating as non-radioactive in recursive chain handling." << G4endl;
        return;
    }

    G4bool isRadioactive = decayTable->entries() > 0;
    if (fVerbosity > 2) G4cout << "is radioactive? " << isRadioactive << G4endl;
    if (!isRadioactive)
    {
        return;
    }

    if (fMode == "diffusion") {
        G4String emitterBaseName = GetBaseEmitterName(particle->GetParticleName());
        G4double clearanceProbability = GetBiologicalClearanceProbabilityForEmitter(emitterBaseName);
        if (clearanceProbability > 0. && G4UniformRand() < clearanceProbability) {
            return;
        }
    }

    G4bool hasLifetime = particle->GetPDGLifeTime() > 0;
    if (fVerbosity > 2) G4cout << "has lifetime? " << hasLifetime << " Lifetime (h): " << particle->GetPDGLifeTime() / (3600. * s) << G4endl;
    if (!hasLifetime)
    {
        // Simulate all products immediately
        G4DecayProducts* decayProducts = nullptr;
#if G4VERSION_NUMBER >= 1130
        decayProducts =
                static_cast<TsRadioactivationAccessor*>(fDecay)->SampleDecayProducts(*particle, decayTable);
#else
        G4VDecayChannel* decayChannel = decayTable->SelectADecayChannel();
        if (decayChannel == nullptr)
            decayChannel = GetDecayChannelManually(decayTable);
        if (decayChannel == nullptr) {
            if (fVerbosity > 1)
                G4cout << "WARNING: Could not select decay channel for recursive isotope "
                       << particle->GetParticleName() << "." << G4endl;
            return;
        }
        decayProducts = decayChannel->DecayIt();
#endif
        if (!decayProducts) {
            if (fVerbosity > 1)
                G4cout << "WARNING: Null decay products for recursive isotope "
                       << particle->GetParticleName() << "." << G4endl;
            return;
        }
        if (fVerbosity > 2) G4cout << "no, but has " << decayProducts->entries() << " decay products" << G4endl;
        if (fVerbosity > 0) G4cout << "Decaying: " << particle->GetParticleName() << G4endl;
        fEmitterParticleName = particle->GetParticleName();
        G4bool newHistory = fTreatAdditionalDecaysAsNewHistories;
        const G4int nDecayProducts = decayProducts->entries();
        std::vector<G4String> particleNames;
        std::vector<G4double> particleEnergies;
        std::vector<G4double> particleCharges;
        std::vector<G4ThreeVector> particleMomenta;
        particleNames.reserve(nDecayProducts);
        particleEnergies.reserve(nDecayProducts);
        particleCharges.reserve(nDecayProducts);
        particleMomenta.reserve(nDecayProducts);
        for (G4int i = 0; i < nDecayProducts; i++)
        {
            G4DynamicParticle* dp = decayProducts[0][i];
            particleNames.push_back(dp->GetDefinition()->GetParticleName());
            particleEnergies.push_back(dp->GetKineticEnergy());
            particleCharges.push_back(dp->GetCharge());
            particleMomenta.push_back(dp->GetMomentumDirection());
        }
        delete decayProducts;
        for (G4int i = 0; i < (G4int)particleNames.size(); i++)
        {
            if (fVerbosity > 0) G4cout << "  Decay product [" << i+1 << "]: " << particleNames[i] << " with kinetic energy " << particleEnergies[i] << " MeV " << G4endl;
            G4ParticleDefinition* productParticle = ResolveParticleDefinitionByName(particleNames[i]);
            if (!productParticle)
                continue;
            if (productParticle->GetParticleType() != "nucleus" || particleCharges[i] != 2) {
                if (fFilterParticlesToSimulateSet.empty() || fFilterParticlesToSimulateSet.find(particleNames[i]) == fFilterParticlesToSimulateSet.end())
                {
                    AddThisParticle(anEvent, particleNames[i], particleEnergies[i], particleCharges[i], particleMomenta[i], newHistory);
                    // Checks recursively for more decays
                    if (fVerbosity > 2) G4cout << "....... and checking recursively if this guy has more decays" << G4endl;
                    if (productParticle != particle &&
                        productParticle->GetParticleName() != particle->GetParticleName()) {
                        AddRecursivelyToIsotopeListIfRadioactive(anEvent, productParticle);
                    }
                    newHistory = false;
                }
            }
        }
    }
    else
    {
        // Sample decay time and add if within step
        G4double meanlife = particle->GetPDGLifeTime()/second;
        G4double lambda = 1 / meanlife;
        G4double decayTime = -log(G4UniformRand()) / lambda;
        if (fIntraStepTime + decayTime < fNextTime - fCurrentTime)
        {
            fIntraStepTime += decayTime;
            G4DecayProducts* decayProducts = nullptr;
#if G4VERSION_NUMBER >= 1130
            decayProducts =
                    static_cast<TsRadioactivationAccessor*>(fDecay)->SampleDecayProducts(*particle, decayTable);
#else
            G4VDecayChannel* decayChannel = decayTable->SelectADecayChannel();
            if (decayChannel == nullptr)
                decayChannel = GetDecayChannelManually(decayTable);
            if (decayChannel == nullptr) {
                if (fVerbosity > 1)
                    G4cout << "WARNING: Could not select timed decay channel for recursive isotope "
                           << particle->GetParticleName() << "." << G4endl;
                return;
            }
            decayProducts = decayChannel->DecayIt();
#endif
            if (!decayProducts) {
                if (fVerbosity > 1)
                    G4cout << "WARNING: Null timed decay products for recursive isotope "
                           << particle->GetParticleName() << "." << G4endl;
                return;
            }
            if (fVerbosity > 2) G4cout << "yes, and has " << decayProducts->entries() << " decay products" << G4endl;
            if (fVerbosity > 0) G4cout << "Decaying: " << particle->GetParticleName() << G4endl;
            if (fVerbosity > 1) G4cout << "The intra-step time for this decay is " << fIntraStepTime << " s" << G4endl;
            fEmitterParticleName = particle->GetParticleName();
            G4bool newHistory = fTreatAdditionalDecaysAsNewHistories;
            const G4int nDecayProducts = decayProducts->entries();
            std::vector<G4String> particleNames;
            std::vector<G4double> particleEnergies;
            std::vector<G4double> particleCharges;
            std::vector<G4ThreeVector> particleMomenta;
            particleNames.reserve(nDecayProducts);
            particleEnergies.reserve(nDecayProducts);
            particleCharges.reserve(nDecayProducts);
            particleMomenta.reserve(nDecayProducts);
            for (G4int i = 0; i < nDecayProducts; i++)
            {
                G4DynamicParticle* dp = decayProducts[0][i];
                particleNames.push_back(dp->GetDefinition()->GetParticleName());
                particleEnergies.push_back(dp->GetKineticEnergy());
                particleCharges.push_back(dp->GetCharge());
                particleMomenta.push_back(dp->GetMomentumDirection());
            }
            delete decayProducts;
            for (G4int i = 0; i < (G4int)particleNames.size(); i++)
            {
                if (fVerbosity > 0) G4cout << "  Decay product [" << i+1 << "]: " << particleNames[i] << " with kinetic energy " << particleEnergies[i] << " MeV " << G4endl;
                if (fFilterParticlesToSimulateSet.empty() || fFilterParticlesToSimulateSet.find(particleNames[i]) == fFilterParticlesToSimulateSet.end()) {
                    AddThisParticle(anEvent, particleNames[i], particleEnergies[i], particleCharges[i],
                                    particleMomenta[i], newHistory);
                }
                // Checks recursively for more decays
                if (fVerbosity > 2) G4cout << "....... and checking recursively if this guy has more decays" << G4endl;
                G4ParticleDefinition* productParticle = ResolveParticleDefinitionByName(particleNames[i]);
                if (productParticle &&
                    productParticle != particle &&
                    productParticle->GetParticleName() != particle->GetParticleName())
                    AddRecursivelyToIsotopeListIfRadioactive(anEvent, productParticle);
                newHistory = false;
            }
        }
        else
        {
            if (fVerbosity > 2) G4cout << "yes, but not in this step" << G4endl;
            // Stable isotope, add to abundance list
            if (fPositionForHistory == NO_POSITION)
            {
                if (fVerbosity > 1) G4cout << "The position was set to NULL, which means that " << fEmitterParticleName <<
                                            " was wiped and its progeny will not be simulated." << G4endl;
            }
            else
            {
                if (fVerbosity > 1) G4cout << particle->GetParticleName() << " stays and abundance is increased!" << G4endl;
                IncreaseAbundance(particle);
            }
        }
    }
}

void TsRadioactiveTimeGenerator::UpdateForNewRun(G4bool )
{
    GetSource();
    fRunID++;
    fCurrentTime = fTimeList[fRunID];
    if (fRunID == (G4int)fTimeList.size() - 1)
        fNextTime = fFinalTime;
    else
        fNextTime = fTimeList[fRunID + 1];

    ConsolidateAbundanceDifferences();
    GetNumberOfIndependentDecaysDuringThisStep();
    fFractionsOfActivityAtTime0.push_back(fNumberOfIndependentDecaysDuringThisStep / fNumberOfIndependentDecaysDuringFirstStep);
    if (fCorrectByNumberOfHistories) {
        G4double correctionFactor = fNumberOfIndependentDecaysDuringThisStep / fNumberOfIndependentDecaysDuringFirstStep;
        if (fVerbosity > 1) G4cout << "New histories: " << (G4int)(fOriginalHistoriesPerStep * correctionFactor) << G4endl;
        fSource->SetNewNumberOfHistories((G4int)(fOriginalHistoriesPerStep * correctionFactor));
    }
    fHistoriesPerStep = GetSource()->GetNumberOfHistoriesInRun();
    if (fVerbosity > 0) G4cout << "Theoretical histories per step: " << fHistoriesPerStep << G4endl;
    AppendCurrentRunMetadata();
    SaveRunMetadata();

    if (fWriteIsotopicAbundance)
    {
        SaveIsotopicAbundanceSnapshot(false);
        if (fNextTime == fFinalTime)
        {
            fSource->SetIsotopicAbundanceNames(fOrderedIsotopes);
            fSource->SetFractionsOfActivityAtTime0(fFractionsOfActivityAtTime0);
            fSource->SetIsFinalRun(true);
        }
        else
        {
            fSource->SetIsFinalRun(false);
        }
    }
}

G4ParticleDefinition* TsRadioactiveTimeGenerator::PickNextRadionuclide()
{
    std::vector<std::pair<G4String, G4double>> emissionProbability;
    emissionProbability.reserve(fIsotopicAbundance.size());
    G4double sumProbabilities = 0.0;
    for (auto& pair : fIsotopicAbundance)
    {
        G4ParticleDefinition* particle;
        if (pair.first.find("[") != std::string::npos)
        {
            // Find the energy as the number between the brackets
            G4double energy = std::stod(pair.first.substr(pair.first.find("[") + 1, pair.first.find("]") - pair.first.find("[") - 1))*keV;
            G4String namewithoutenergy = pair.first.substr(0, pair.first.find("["));
            particle = G4ParticleTable::GetParticleTable()->FindParticle(namewithoutenergy);
            G4double Z = particle->GetAtomicNumber();
            G4double A = particle->GetAtomicMass();
            particle = G4ParticleTable::GetParticleTable()->GetIonTable()->GetIon(Z, A, energy);
        }
        else
            particle = G4ParticleTable::GetParticleTable()->FindParticle(pair.first);
        G4double lambdaForEmitter = 1 / (particle->GetPDGLifeTime()/second);
        G4double probForEmitter = pair.second * (1 - exp(-lambdaForEmitter * (fNextTime - fCurrentTime)));
        emissionProbability.emplace_back(pair.first, probForEmitter);
        sumProbabilities += probForEmitter;
    }

    // Select primary particle
    G4ParticleDefinition* next = nullptr;
    if (emissionProbability.empty())
        return next;
    if (sumProbabilities <= 0.) {
        return ResolveParticleDefinitionByName(emissionProbability.front().first);
    }
    G4double rand = G4UniformRand() * sumProbabilities;
    G4double accumulatedProb = 0.0;
    for (auto& pair : emissionProbability)
    {
        accumulatedProb += pair.second;
        if (rand <= accumulatedProb)
        {
            if (pair.first.find("[") != std::string::npos)
            {
                // Find the energy as the number between the brackets
                G4double energy = std::stod(pair.first.substr(pair.first.find("[") + 1, pair.first.find("]") - pair.first.find("[") - 1))*keV;
                G4String namewithoutenergy = pair.first.substr(0, pair.first.find("["));
                next = G4ParticleTable::GetParticleTable()->FindParticle(namewithoutenergy);
                G4double Z = next->GetAtomicNumber();
                G4double A = next->GetAtomicMass();
                next = G4ParticleTable::GetParticleTable()->GetIonTable()->GetIon(Z, A, energy);
            }
            else
                next = G4ParticleTable::GetParticleTable()->FindParticle(pair.first);
            break;
        }
    }
    if (!next)
        next = ResolveParticleDefinitionByName(emissionProbability.back().first);
    return next;
}

void TsRadioactiveTimeGenerator::IncreaseAbundance(G4ParticleDefinition* particle) {
    G4double weightOfAHistory = ComputePrimaryWeight();
    G4String name = particle->GetParticleName();
    if (fIsotopicAbundanceDifferencePerStep.find(name) == fIsotopicAbundanceDifferencePerStep.end())
        fIsotopicAbundanceDifferencePerStep[name] = weightOfAHistory / fCurrentNAtoms;
    else
        fIsotopicAbundanceDifferencePerStep[name] += weightOfAHistory / fCurrentNAtoms;
}

void TsRadioactiveTimeGenerator::DecreaseAbundance(G4ParticleDefinition* particle)
{
    G4double weightOfAHistory = ComputePrimaryWeight();
    G4String name = particle->GetParticleName();
    if (fIsotopicAbundanceDifferencePerStep.find(name) == fIsotopicAbundanceDifferencePerStep.end())
        fIsotopicAbundanceDifferencePerStep[name] = -weightOfAHistory / fCurrentNAtoms;
    else
        fIsotopicAbundanceDifferencePerStep[name] -= weightOfAHistory / fCurrentNAtoms;
}

void TsRadioactiveTimeGenerator::ConsolidateAbundanceDifferences()
{
    for (auto& pair : fIsotopicAbundanceDifferencePerStep)
    {
        if (fIsotopicAbundance.find(pair.first) == fIsotopicAbundance.end()) {
            fIsotopicAbundance[pair.first] = pair.second;
            fOrderedIsotopes.push_back(pair.first);
        }
        else
            fIsotopicAbundance[pair.first] += pair.second;
        if (fIsotopicAbundance[pair.first] < 0)
            fIsotopicAbundance[pair.first] = 0;
        if (fIsotopicAbundance[pair.first] > 1)
            fIsotopicAbundance[pair.first] = 1;
    }
    for (auto& pair : fIsotopicAbundanceDifferencePerStep)
        fIsotopicAbundanceDifferencePerStep[pair.first] = 0;
}

void TsRadioactiveTimeGenerator::ReadIsotopicAbundanceFile() {
    // Read csv file
    std::map<G4int, std::map<G4String, G4double>> csvData;  // <time, <isotope, abundance>>
    std::map<G4int, G4double> fractionsOfActivityAtTime0; // <time, fraction of activity at time 0>
    std::ifstream infile(fReadIsotopicAbundanceFileName);

    std::string line;
    std::vector<std::string> headers;

    // Read headers
    if (std::getline(infile, line)) {
        std::stringstream ss(line);
        std::string header;
        while (std::getline(ss, header, ',')) {
            headers.push_back(header);
        }
    }
    // Populate names for isotopic abundance, get headers
    for (G4int i = 1; i < (G4int)headers.size() - 1; i++)
        fOrderedIsotopes.push_back(headers[i]);
    // Read content
    while (std::getline(infile, line)) {
        std::istringstream iss(line);
        std::string token;
        std::getline(iss, token, ','); // Read time
        G4int time = std::stoi(token);
        std::map<G4String, G4double> abundances;
        for (G4int i = 1; i < (G4int)headers.size() - 1; i++) {
            std::getline(iss, token, ',');
            G4double abundance = std::stod(token);
            abundances[headers[i]] = abundance;
        }
        csvData[time] = abundances;
        std::getline(iss, token, ','); // Read fraction of activity at time 0
        G4double fractionOfActivityAtTime0 = std::stod(token);
        fractionsOfActivityAtTime0[time] = fractionOfActivityAtTime0;
    }

    // Load the right isotopic abundance
    if (csvData.empty()) {
        G4ExceptionDescription msg;
        msg << "No data found in file " << fReadIsotopicAbundanceFileName << G4endl;
        G4Exception("TsRadioactiveTimeGenerator::ReadIsotopicAbundanceFile()", "TsRADIOACTIVE001", FatalException, msg);
    }
    if (fTimeToLoadIsotopicAbundance == -1) {
        // load the last entry
        fIsotopicAbundance = csvData.rbegin()->second;
        if (fUseFractionOfActivityFromFile)
            fFractionsOfActivityAtTime0.push_back(fractionsOfActivityAtTime0.rbegin()->second);
        return;
    }
    auto it = csvData.lower_bound(fTimeToLoadIsotopicAbundance);
    auto itFractionOfActivityAtTime0 = fractionsOfActivityAtTime0.lower_bound(fTimeToLoadIsotopicAbundance);
    if (it == csvData.end()) {
        // If the target time is greater than the last time point, load the last entry
        fIsotopicAbundance = csvData.rbegin()->second;
        if (fUseFractionOfActivityFromFile)
            fFractionsOfActivityAtTime0.push_back(fractionsOfActivityAtTime0.rbegin()->second);
        return;
    }
    if (it->first == fTimeToLoadIsotopicAbundance || it == csvData.begin()) {
        // If the target times matches the time of the first entry, or if the target time is between the first and second entry, load the first entry
        fIsotopicAbundance = it->second;
        if (fUseFractionOfActivityFromFile)
            fFractionsOfActivityAtTime0.push_back(fractionsOfActivityAtTime0.rbegin()->second);
        return;
    }
    // Interpolate between the closest two points
    auto itPrev = std::prev(it);
    auto itFractionOfActivityAtTime0Prev = std::prev(itFractionOfActivityAtTime0);
    G4double ratio = G4double(fTimeToLoadIsotopicAbundance - itPrev->first) / G4double(it->first - itPrev->first);
    for (G4int i = 1; i < (G4int)headers.size() - 1; i++) {
        if (headers[i] == "Time") continue;
        fIsotopicAbundance[headers[i]] = itPrev->second[headers[i]] + ratio * (it->second[headers[i]] - itPrev->second[headers[i]]);
    }
    if (fUseFractionOfActivityFromFile)
        fFractionsOfActivityAtTime0.push_back(itFractionOfActivityAtTime0Prev->second + ratio * (itFractionOfActivityAtTime0->second - itFractionOfActivityAtTime0Prev->second));
}

G4VDecayChannel* TsRadioactiveTimeGenerator::GetDecayChannelManually(G4DecayTable* decayTable)
{
    if (!decayTable || decayTable->entries() <= 0)
        return nullptr;

    G4cout << "WARNING: SelectADecayChannel did not work. Selecting decay channel manually";
    G4double accumulatedBr = 0.0;
    std::vector<G4double> channel_br;
    for (G4int i = 0; i < decayTable->entries(); i++)
    {
        G4VDecayChannel* decayChannel = decayTable->GetDecayChannel(i);
        G4double br = decayChannel->GetBR();
        accumulatedBr += br;
        channel_br.push_back(accumulatedBr);
        if (fVerbosity > 0 ) G4cout << "Channel: " << decayChannel->GetKinematicsName() <<  " - BR: " << br << G4endl;
    }
    // Normalize
    for (G4int i = 0; i < (G4int)channel_br.size(); i++)
        channel_br[i] /= accumulatedBr;
    // Sample [0,1] and select a channel
    G4double rand = G4UniformRand();
    G4int selectedChannel = 0;
    for (G4int i = 0; i < (G4int)channel_br.size(); i++)
    {
        if (rand <= channel_br[i])
        {
            selectedChannel = i;
            break;
        }
    }
    return decayTable->GetDecayChannel(selectedChannel);
}

void TsRadioactiveTimeGenerator::GetNumberOfIndependentDecaysDuringThisStep()
{
    G4double sumDecays = 0;
    if (fVerbosity > 0) G4cout << "Time at this step: " << fCurrentTime << " s" << G4endl;
    for (auto& pair : fIsotopicAbundance)
    {
        if (fVerbosity > 1) G4cout << "Isotope: " << pair.first << " - abundance: " << pair.second * 100 << "%" << G4endl;
        // If the name contains '[E]' is in an excited state and this should be considered
        G4ParticleDefinition* particle;
        G4double lifetime;
        if (pair.first.find("[") != std::string::npos)
        {
            G4String name = pair.first;
            // Find the energy as the number in between '[' and ']'
            G4double energy = std::stod(name.substr(name.find("[") + 1, name.find("]") - name.find("[") - 1))*keV;
            G4String nameWithoutEnergy = name.substr(0, name.find("["));
            particle = G4ParticleTable::GetParticleTable()->FindParticle(nameWithoutEnergy);
            G4double Z = particle->GetAtomicNumber();
            G4double A = particle->GetAtomicMass();
            lifetime = G4IonTable::GetIonTable()->GetLifeTime(Z, A, energy)/second;
            if (lifetime == -1001/second) {
                G4cout << "WARNING: No data for this excitation energy could be found. The ground state will be used instead." << G4endl;
                particle = G4ParticleTable::GetParticleTable()->FindParticle(nameWithoutEnergy);
                lifetime = particle->GetPDGLifeTime()/second;
            }
        }
        else
        {
            particle = G4ParticleTable::GetParticleTable()->FindParticle(pair.first);
            lifetime = particle->GetPDGLifeTime()/second;
        }
        G4double lambdaForEmitter = 1 / lifetime;
        G4double nDecays = pair.second * fCurrentNAtoms * (1 - exp(-lambdaForEmitter * (fNextTime - fCurrentTime)));
        sumDecays += nDecays;
    }
    fNumberOfIndependentDecaysDuringThisStep = sumDecays;
}

void TsRadioactiveTimeGenerator::SetNewPositionForHistory() {
    if (fMode == "diffusion") {
        SetNewPositionForHistoryDiffusion();
        return;
    }

    if (fModePositionSampler) {
        TsEventContext context;
        context.runID = fRunID;
        context.eventID = 0;
        context.currentTimeS = fCurrentTime;
        context.nextTimeS = fNextTime;
        setPositionForHistory(fModePositionSampler->SamplePosition(context));
        if (fMode == "activity_map") {
            TsActivityMapPositionSampler* sampler =
                    dynamic_cast<TsActivityMapPositionSampler*>(fModePositionSampler.get());
            if (sampler) {
                size_t selectedIndex = sampler->GetLastSelectedIndex();
                if (selectedIndex < fActivityMapSampledVoxelCounts.size())
                    fActivityMapSampledVoxelCounts[selectedIndex]++;
            }
        }
        if (fModePositionSampler->IsHistorySuppressed(context))
            setPositionForHistory(NO_POSITION);
        return;
    }

    if (fNeedToCalculateExtent) {
        G4VisExtent myExtent = fComponent->GetExtent();
        fXMin = myExtent.GetXmin();
        fXMax = myExtent.GetXmax();
        fYMin = myExtent.GetYmin();
        fYMax = myExtent.GetYmax();
        fZMin = myExtent.GetZmin();
        fZMax = myExtent.GetZmax();

        fNeedToCalculateExtent = false;
        G4TransportationManager *transportationManager = G4TransportationManager::GetTransportationManager();
        fNavigator = transportationManager->GetNavigator(
                transportationManager->GetParallelWorld(fComponent->GetWorldName()));
    }

    // Case of point source
    if (fXMax - fXMin == 0 && fYMax - fYMin == 0 && fZMax - fZMin == 0) {
        fPositionForHistory = G4ThreeVector(fXMin, fYMin, fZMin);
        return;
    }

    G4double testX, testY, testZ;
    G4VPhysicalVolume* foundVolume;
    G4bool foundPointInComponent = false;

    while (!foundPointInComponent)
    {
        testX = G4RandFlat::shoot(fXMin, fXMax);
        testY = G4RandFlat::shoot(fYMin, fYMax);
        testZ = G4RandFlat::shoot(fZMin, fZMax);

        foundVolume = fNavigator->LocateGlobalPointAndSetup(G4ThreeVector(testX, testY, testZ));
        if (foundVolume) {
            for (size_t t = 0; !foundPointInComponent && t < fVolumes.size(); t++)
                foundPointInComponent = (foundVolume == fVolumes[t]);
        }
    }
    G4Point3D* myCenter = fComponent->GetTransRelToWorld();
    fPositionForHistory = G4ThreeVector(testX - myCenter[0].x(), testY - myCenter[0].y(), testZ - myCenter[0].z());
}

void TsRadioactiveTimeGenerator::SetNewPositionForHistoryDiffusion()
{
    G4String currentEmitterBaseName = GetBaseEmitterName(fEmitterParticleName);
    G4String parentEmitterBaseName = GetBaseEmitterName(fParticleDefinition->GetParticleName());
    if (currentEmitterBaseName == parentEmitterBaseName) {
        fDiffusionCurrentPosition = GetInitialPositionForDiffusion();
        fDiffusionIsDesorbed = false;
        fDiffusionLastDecayTime = fIntraStepTime;
        setPositionForHistory(fDiffusionCurrentPosition);
        return;
    }

    G4double diffusionTime = std::max(0., fIntraStepTime - fDiffusionLastDecayTime);
    fDiffusionLastDecayTime = fIntraStepTime;
    if (!fDiffusionIsDesorbed) {
        G4double desorptionProbability = GetDesorptionProbabilityForEmitter(currentEmitterBaseName);
        if (desorptionProbability > 0. && G4UniformRand() < desorptionProbability)
            fDiffusionIsDesorbed = true;
    }

    if (fDiffusionIsDesorbed) {
        G4double diffusionCoefficient = GetDiffusionCoefficientForEmitter(currentEmitterBaseName);
        if (diffusionCoefficient > 0. && diffusionTime > 0.) {
            G4double sigma = std::sqrt(6. * diffusionCoefficient * diffusionTime);
            G4double radialDisplacement = G4RandGauss::shoot(0., sigma);
            G4double phi = G4UniformRand() * 2. * CLHEP::pi;
            G4double theta = std::acos(2. * G4UniformRand() - 1.);
            fDiffusionCurrentPosition = G4ThreeVector(
                    fDiffusionCurrentPosition.x() + radialDisplacement * std::sin(theta) * std::cos(phi),
                    fDiffusionCurrentPosition.y() + radialDisplacement * std::sin(theta) * std::sin(phi),
                    fDiffusionCurrentPosition.z() + radialDisplacement * std::cos(theta));
        }
    }

    setPositionForHistory(fDiffusionCurrentPosition);
}

G4ThreeVector TsRadioactiveTimeGenerator::GetInitialPositionForDiffusion()
{
    G4String geometryType = fComponent->GetEnvelopePhysicalVolume()->GetLogicalVolume()->GetSolid()->GetEntityType();
    if (geometryType == "G4Tubs") {
        G4double cylinderLength = 2. * (fDiffusionHasSeedHalfLength ? fDiffusionSeedHalfLength : (fZMax - fZMin) / 2.);
        return fDiffusionSampler.SampleAtCylinderSurface(fDiffusionSeedRadius, cylinderLength);
    }
    if (geometryType == "G4Sphere")
        return fDiffusionSampler.SampleAtSphereSurface(fDiffusionSeedRadius);

    G4double x = G4RandFlat::shoot(fXMin, fXMax);
    G4double y = G4RandFlat::shoot(fYMin, fYMax);
    G4double z = G4RandFlat::shoot(fZMin, fZMax);
    G4Point3D* center = fComponent->GetTransRelToWorld();
    return G4ThreeVector(x - center[0].x(), y - center[0].y(), z - center[0].z());
}

G4double TsRadioactiveTimeGenerator::GetDiffusionCoefficientForEmitter(const G4String& emitterBaseName) const
{
    for (size_t i = 0; i < fDiffusionRadionuclidesToDiffuse.size(); i++) {
        if (fDiffusionRadionuclidesToDiffuse[i] == emitterBaseName)
            return i < fDiffusionCoefficients.size() ? fDiffusionCoefficients[i] : 0.;
    }
    return 0.;
}

G4double TsRadioactiveTimeGenerator::GetDesorptionProbabilityForEmitter(const G4String& emitterBaseName) const
{
    for (size_t i = 0; i < fDiffusionRadionuclidesToDiffuse.size(); i++) {
        if (fDiffusionRadionuclidesToDiffuse[i] == emitterBaseName)
            return i < fDiffusionDesorptionProbabilities.size() ? fDiffusionDesorptionProbabilities[i] : 0.;
    }
    return 0.;
}

G4double TsRadioactiveTimeGenerator::GetBiologicalClearanceProbabilityForEmitter(const G4String& emitterBaseName) const
{
    for (size_t i = 0; i < fDiffusionRadionuclidesToDiffuse.size(); i++) {
        if (fDiffusionRadionuclidesToDiffuse[i] == emitterBaseName)
            return i < fDiffusionBiologicalClearanceRates.size() ? fDiffusionBiologicalClearanceRates[i] : 0.;
    }
    return 0.;
}

void TsRadioactiveTimeGenerator::setPositionForHistory(G4ThreeVector NewPosition){
    fPositionForHistory = NewPosition;
    if (fVerbosity > 1)
        G4cout << "New position for history (x, y, z): " << fPositionForHistory << G4endl;
}

void TsRadioactiveTimeGenerator::UpdateCounts(G4String particleName)
{
    if (particleName == "e-")
        fNumberOfElectronsGenerated++;
    else if (particleName == "alpha")
        fNumberOfAlphasGenerated++;
    else if (particleName == "gamma")
        fNumberOfGammasGenerated++;
    else if (particleName == "neutron")
        fNumberOfNeutronsGenerated++;
    else if (particleName == "proton")
        fNumberOfProtonsGenerated++;
    else if (particleName == "e+")
        fNumberOfPositronsGenerated++;
    else
        fNumberOfOtherGenerated++;
}

void TsRadioactiveTimeGenerator::PrintCounts() {
    G4cout << "Current counts" << G4endl;
    G4cout << "------" << G4endl;
    G4cout << "Number of electrons generated: " << fNumberOfElectronsGenerated << G4endl;
    G4cout << "Number of alphas generated: " << fNumberOfAlphasGenerated << G4endl;
    G4cout << "Number of gammas generated: " << fNumberOfGammasGenerated << G4endl;
    G4cout << "Number of neutrons generated: " << fNumberOfNeutronsGenerated << G4endl;
    G4cout << "Number of protons generated: " << fNumberOfProtonsGenerated << G4endl;
    G4cout << "Number of positrons generated: " << fNumberOfPositronsGenerated << G4endl;
    G4cout << "Number of other particles generated: " << fNumberOfOtherGenerated << G4endl;
}

G4double TsRadioactiveTimeGenerator::ComputePrimaryWeight() const
{
    return fTimeDecayKernel ? fTimeDecayKernel->ComputePrimaryWeight(BuildTimeDecayState())
                            : BuildNormalizationContext().ComputePrimaryWeight();
}

void TsRadioactiveTimeGenerator::AppendCurrentRunMetadata()
{
    fNormalizationHistory.push_back(BuildNormalizationContext());
}

TsTimeDecayState TsRadioactiveTimeGenerator::BuildTimeDecayState() const
{
    TsTimeDecayState state;
    state.runID = fRunID;
    state.timeStartS = fCurrentTime;
    state.timeEndS = fNextTime;
    state.numberOfIndependentDecaysDuringThisStep = fNumberOfIndependentDecaysDuringThisStep;
    state.numberOfIndependentDecaysDuringFirstStep = fNumberOfIndependentDecaysDuringFirstStep;
    state.historiesPerStep = fHistoriesPerStep;
    state.correctByNumberOfHistories = fCorrectByNumberOfHistories;
    state.initialActivityBq = fInitialActivity;
    return state;
}

TsNormalizationContext TsRadioactiveTimeGenerator::BuildNormalizationContext() const
{
    if (fTimeDecayKernel)
        return fTimeDecayKernel->BuildNormalizationContext(BuildTimeDecayState());

    TsNormalizationContext context;
    context.runID = fRunID;
    context.timeStartS = fCurrentTime;
    context.timeEndS = fNextTime;
    context.stepDurationS = fNextTime - fCurrentTime;
    context.numberOfIndependentDecaysDuringThisStep = fNumberOfIndependentDecaysDuringThisStep;
    context.numberOfIndependentDecaysDuringFirstStep = fNumberOfIndependentDecaysDuringFirstStep;
    context.historiesPerStep = fHistoriesPerStep;
    context.correctByNumberOfHistories = fCorrectByNumberOfHistories;
    context.initialActivityBq = fInitialActivity;
    context.effectiveActivityBq =
            context.stepDurationS > 0. ? context.numberOfIndependentDecaysDuringThisStep / context.stepDurationS : 0.;
    return context;
}

void TsRadioactiveTimeGenerator::SaveRunMetadata() const
{
    if (!fWriteRunMetadata)
        return;
    if (fResultReporter)
        fResultReporter->WriteRunMetadata(fRunMetadataFileName, fMode, GetModeMetadataJson(), fNormalizationHistory);
    else
        TsRunMetadataWriter::Write(fRunMetadataFileName, fMode, GetModeMetadataJson(), fNormalizationHistory);
    if (fMode == "activity_map")
        WriteActivityMapSummary();
    if (fWriteModeSummary && fModeSummaryReporter)
        fModeSummaryReporter->WriteSummary(fModeSummaryFileName, fMode, GetModeMetadataJson());
}

void TsRadioactiveTimeGenerator::WriteActivityMapSummary() const
{
    if (!fActivityMapWriteSummary)
        return;
    if (fActivityMapPositions.empty())
        return;

    std::ofstream out(fActivityMapSummaryFileName, std::ios::trunc);
    out << std::setprecision(16);

    unsigned long long totalSampled = 0ULL;
    for (size_t i = 0; i < fActivityMapSampledVoxelCounts.size(); i++)
        totalSampled += fActivityMapSampledVoxelCounts[i];

    std::vector<std::pair<unsigned long long, size_t>> ranked;
    ranked.reserve(fActivityMapSampledVoxelCounts.size());
    for (size_t i = 0; i < fActivityMapSampledVoxelCounts.size(); i++)
        ranked.push_back(std::make_pair(fActivityMapSampledVoxelCounts[i], i));
    std::sort(ranked.begin(), ranked.end(),
              [](const std::pair<unsigned long long, size_t>& a, const std::pair<unsigned long long, size_t>& b) {
                  return a.first > b.first;
              });

    const size_t topN = std::min(static_cast<size_t>(20), ranked.size());
    out << "{\n";
    out << "  \"summary_schema\": \"tsrts.activity_map_summary.v1\",\n";
    out << "  \"mode\": \"activity_map\",\n";
    out << "  \"use_calibrated_counts\": " << (fActivityMapUseCalibratedCounts ? "true" : "false") << ",\n";
    out << "  \"calibrated_count_units\": \"" << fActivityMapCalibratedCountUnits << "\",\n";
    out << "  \"calibration_scale_bq_per_count\": " << fActivityMapCalibrationScaleBqPerCount << ",\n";
    out << "  \"grid_match_required\": " << (fActivityMapRequireParentGridMatch ? "true" : "false") << ",\n";
    out << "  \"grid_match_tolerance_fraction\": " << fActivityMapGridMatchToleranceFraction << ",\n";
    out << "  \"n_source_voxels\": " << fActivityMapPositions.size() << ",\n";
    out << "  \"raw_count_min\": " << fActivityMapMinCount << ",\n";
    out << "  \"raw_count_max\": " << fActivityMapMaxCount << ",\n";
    out << "  \"raw_count_sum\": " << fActivityMapRawCountSum << ",\n";
    out << "  \"calibrated_total_activity_bq\": " << fActivityMapTotalActivity << ",\n";
    out << "  \"voxel_size_mm\": [" << (fActivityMapVoxelSizeX / mm) << ", "
        << (fActivityMapVoxelSizeY / mm) << ", " << (fActivityMapVoxelSizeZ / mm) << "],\n";
    out << "  \"observed_map_extent_mm\": [" << (fActivityMapObservedWidthX / mm) << ", "
        << (fActivityMapObservedWidthY / mm) << ", " << (fActivityMapObservedWidthZ / mm) << "],\n";
    out << "  \"parent_extent_available\": " << (fActivityMapParentExtentAvailable ? "true" : "false") << ",\n";
    out << "  \"parent_extent_mm\": [" << (fActivityMapParentWidthX / mm) << ", "
        << (fActivityMapParentWidthY / mm) << ", " << (fActivityMapParentWidthZ / mm) << "],\n";
    out << "  \"sampled_histories_total\": " << totalSampled << ",\n";
    out << "  \"top_sampled_voxels\": [\n";
    for (size_t i = 0; i < topN; i++) {
        const size_t voxelIndex = ranked[i].second;
        const unsigned long long sampledCount = ranked[i].first;
        const G4double expected = (voxelIndex < fActivityMapVoxelProbabilities.size()) ? fActivityMapVoxelProbabilities[voxelIndex] : 0.;
        const G4double observed = (totalSampled > 0) ? static_cast<G4double>(sampledCount) / static_cast<G4double>(totalSampled) : 0.;
        out << "    {\"voxel_index\": " << voxelIndex
            << ", \"sampled_count\": " << sampledCount
            << ", \"expected_probability\": " << expected
            << ", \"observed_fraction\": " << observed << "}";
        if (i + 1 < topN)
            out << ",";
        out << "\n";
    }
    out << "  ]\n";
    out << "}\n";
}

void TsRadioactiveTimeGenerator::SaveIsotopicAbundanceSnapshot(G4bool initializeFile) const
{
    if (!fIsotopicReporter)
        return;
    if (initializeFile) {
        fIsotopicReporter->Initialize(fWriteIsotopicAbundanceFileName, fOrderedIsotopes,
                                      fIsotopicAbundance, fCurrentTime);
    } else {
        fIsotopicReporter->Append(fWriteIsotopicAbundanceFileName, fOrderedIsotopes,
                                  fIsotopicAbundance, fCurrentTime);
    }
}

G4String TsRadioactiveTimeGenerator::GetModeMetadataJson() const
{
    if (fMode == "activity_map") {
        std::ostringstream out;
        out << "{";
        out << "\"use_calibrated_counts\": " << (fActivityMapUseCalibratedCounts ? "true" : "false") << ", ";
        out << "\"calibrated_count_units\": \"" << fActivityMapCalibratedCountUnits << "\", ";
        out << "\"calibration_scale_bq_per_count\": " << fActivityMapCalibrationScaleBqPerCount << ", ";
        out << "\"used_legacy_calibration_param\": "
            << (fActivityMapUsedLegacyCalibrationParam ? "true" : "false") << ", ";
        out << "\"require_parent_grid_match\": "
            << (fActivityMapRequireParentGridMatch ? "true" : "false") << ", ";
        out << "\"grid_match_tolerance_fraction\": " << fActivityMapGridMatchToleranceFraction << ", ";
        out << "\"voxel_size_mm\": ["
            << (fActivityMapVoxelSizeX / mm) << ", "
            << (fActivityMapVoxelSizeY / mm) << ", "
            << (fActivityMapVoxelSizeZ / mm) << "], ";
        out << "\"observed_map_extent_mm\": ["
            << (fActivityMapObservedWidthX / mm) << ", "
            << (fActivityMapObservedWidthY / mm) << ", "
            << (fActivityMapObservedWidthZ / mm) << "], ";
        out << "\"parent_extent_available\": "
            << (fActivityMapParentExtentAvailable ? "true" : "false") << ", ";
        out << "\"parent_extent_mm\": ["
            << (fActivityMapParentWidthX / mm) << ", "
            << (fActivityMapParentWidthY / mm) << ", "
            << (fActivityMapParentWidthZ / mm) << "], ";
        out << "\"raw_count_min\": " << fActivityMapMinCount << ", ";
        out << "\"raw_count_max\": " << fActivityMapMaxCount << ", ";
        out << "\"raw_count_sum\": " << fActivityMapRawCountSum << ", ";
        out << "\"n_source_voxels\": " << fActivityMapPositions.size() << ", ";
        out << "\"calibrated_total_activity_bq\": " << fActivityMapTotalActivity;
        out << "}";
        return out.str();
    }
    if (fMode == "diffusion") {
        std::ostringstream out;
        out << "{";
        out << "\"generation_start_s\": " << fDiffusionGenerationStartTime << ", ";
        out << "\"seed_radius_mm\": " << (fDiffusionSeedRadius / mm) << ", ";
        out << "\"n_diffusing_radionuclides\": " << fDiffusionRadionuclidesToDiffuse.size();
        out << "}";
        return out.str();
    }
    if (fMode == "biodist") {
        std::ostringstream out;
        out << "{";
        out << "\"biodist_backend_available\": " << (fBioDistBackendAvailable ? "true" : "false") << ", ";
        out << "\"mode_invoked\": " << (fBioDistModeInvoked ? "true" : "false");
        out << "}";
        return out.str();
    }
    return "{}";
}
