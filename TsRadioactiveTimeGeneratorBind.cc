// Particle Generator for RadioactiveTimeSourceBind
//
// Created by Daniel Suarez and Victor Valladolid on 12/18/23.
//
#include <cmath>
#include <iomanip>
#include <sstream>

#include "TsRadioactiveTimeGeneratorBind.hh"
#include "Randomize.hh"

#include "TsVGeometryComponent.hh"
#include "TsSource.hh"
#include "TsGeometryManager.hh"

#include "G4Radioactivation.hh"
#include "G4DecayTable.hh"
#include "G4VDecayChannel.hh"
#include "G4DecayProducts.hh"
#include "G4SystemOfUnits.hh"
#include "G4ParticleTable.hh"
#include "G4VPhysicalVolume.hh"
#include "G4VisExtent.hh"
#include "G4TransportationManager.hh"


TsRadioactiveTimeGeneratorBind::TsRadioactiveTimeGeneratorBind(TsParameterManager *pM, TsGeometryManager *gM,
                                                               TsGeneratorManager *pgM, G4String sourceName)
        : TsRadioactiveTimeGenerator(pM, gM, pgM, sourceName),
          fSumProb(false), fMediumIsSphere(false), SamplingPosition(new TsSamplingAtVolumes()) {

    // This generator requires TsCellMonolayer or TsCell3D as its geometry component.
    G4String path = fComponent->GetFullParmName("Type");

    if (!fPm->ParameterExists(path) || (fPm->GetStringParameter(path) != "TsCellMonolayer" && fPm->GetStringParameter(path) != "TsCell3D")) {
        G4cerr << "Topas is exiting due to a serious error in source setup." << G4endl;
        G4cerr << "This source was designed to be executed with TsCellMonolayer or TsCell3D geometry component." <<  G4endl;
        exit(1);
    }

    // Retrieve all physical volumes in the component:
    // fPhysicalVolumes[0] is the envelope, i.e. the medium culture
    // fPhysicalVolumes[1] is the cell (cytoplasm)
    // fPhysicalVolumes[2] is the nucleus
    fPhysicalVolumes = fComponent->GetAllPhysicalVolumes();

    // Retrieve cell/monolayer information.
    TsCellMonolayer *cM = dynamic_cast<TsCellMonolayer *>(fComponent);
    fCellInfo = cM->GetCellInfo();
    fCellPositions = cM->GetCellPositions();
    fMonolayerInfo = cM->GetMonolayerInfo();
    // Number of cells
    fNumberOfCells = cM->GetNoCopies();
    // Check geometry
    fCellGeometry = cM->GetCellGeometry();
    // Update geometry flags
    if (fCellGeometry == "Sphere") {
        fCellIsSphere = true;
    } else if (fCellGeometry == "Cylinder") {
        fCellIsCylinder = true;
    } else {
        G4cerr << "Topas is exiting due to a serious error in source setup." << G4endl;
        G4cerr << "CellGeometry must be Cylinder or Sphere" << G4endl;
        exit(1);
    }
    
    ResolveParameters();

}

TsRadioactiveTimeGeneratorBind::~TsRadioactiveTimeGeneratorBind() {}

void TsRadioactiveTimeGeneratorBind::ResolveParameters() {
    fModeParamSources.clear();
    auto modeParam = [this](const G4String& key) { return GetFullParmName("ModeParams/" + key); };
    auto legacyParam = [this](const G4String& key) { return GetFullParmName(key); };

    // Experimental condition and RPT features:
    // activity (Bq/mL) and specific activity (Bq/nmol).
    G4String sourceTag;
    if (TsInVitroBindModeAdapter::ResolveUnitless(fPm, modeParam("Activity_permL"), legacyParam("Activity_permL"),
                                                  &fActivity_permL, &sourceTag))
        fModeParamSources["activity_per_ml"] = sourceTag;
    if (TsInVitroBindModeAdapter::ResolveUnitless(fPm, modeParam("SpecificActivity_pernmol"),
                                                  legacyParam("SpecificActivity_pernmol"),
                                                  &fSpecificActivity_pernmol, &sourceTag))
        fModeParamSources["specific_activity_per_nmol"] = sourceTag;

    if (fActivity_permL == -1 || fSpecificActivity_pernmol == -1) {
        G4cerr << "Topas is exiting due to a serious error in source setup." << G4endl;
        G4cerr << "Activity and specific activity are required parameters." << G4endl;
        G4cerr << "Please check the parameter file." << G4endl;
        exit(1);
    }

    // Calculating concentration of the simulation (multiplying by 1000 to convert from nmol/mL to nmol/L)
    fConcentration = 1000 * fActivity_permL / fSpecificActivity_pernmol;
    fEconcentration = fConcentration; // Initially, all isotopes are in the medium
    fEProbability = 1; // Initially, all isotopes are in the medium

    // Washout time. Large time is interpreted as no washout
    fWashOutTime = 1e10;
    G4double washOutTimeRaw = 0.;
    if (TsInVitroBindModeAdapter::ResolveDouble(fPm, modeParam("WashoutTime"), legacyParam("WashoutTime"), "Time",
                                                &washOutTimeRaw, &sourceTag)) {
        fWashOutTime = washOutTimeRaw / second;
        fModeParamSources["washout_time_s"] = sourceTag;
    }


    // Optional spherical medium override.
    G4double mediumRadius = 0.;
    if (TsInVitroBindModeAdapter::ResolveDouble(fPm, modeParam("MediumRad"), legacyParam("MediumRad"), "Length",
                                                &mediumRadius, &sourceTag)) {
        G4cout << "Setting medium to be a sphere with radius: " << mediumRadius << G4endl;
        fMediumRadius = mediumRadius;
        fMediumIsSphere = true;
        fModeParamSources["medium_radius"] = sourceTag;
    }

    // Calling the method to set the binding kinetic model
    GettingBindingKineticParameters();

    SetUptakePerCell();

    fCompartmentProbabilities.medium = fEProbability;
    fCompartmentProbabilities.membrane = fMProbability;
    fCompartmentProbabilities.cytoplasm = fIProbability;
    fCompartmentProbabilities.nucleus = fNProbability;
    fCompartmentProbabilities.degraded = fDProbability;

    TsInVitroBindKineticsState state;
    state.econcentration = &fEconcentration;
    state.mconcentration = &fMconcentration;
    state.iconcentration = &fIconcentration;
    state.nconcentration = &fNconcentration;
    state.dconcentration = &fDconcentration;
    state.ratioIsotopesRemaining = &fRatioIsotopesRemaining;
    state.currentNAtoms = &fCurrentNAtoms;
    state.initialNAtoms = fInitialNAtoms;
    state.totalConcentration = fConcentration;
    fKineticsModelAdapter.reset(
            new TsInVitroBindKineticsModel(fModel, fModelName, fProbabilitiesFilename, state));
    if (fSaveModelToFile) {
        fKineticsModelAdapter->WriteProbabilityTrace(fInitialTime, fFinalTime, fProbabilitiesFilename.c_str());
    }

    TsInVitroBindPositionSampler::Config samplerConfig;
    samplerConfig.samplingPosition = SamplingPosition;
    samplerConfig.cellInfo = &fCellInfo;
    samplerConfig.monolayerInfo = &fMonolayerInfo;
    samplerConfig.cellPositions = &fCellPositions;
    samplerConfig.physicalVolumes = &fPhysicalVolumes;
    samplerConfig.componentCenter = fComponent->GetTransRelToWorld()[0];
    samplerConfig.probabilities = &fCompartmentProbabilities;
    samplerConfig.sampleMembraneCellId = [this]() { return getCellId(); };
    samplerConfig.worldName = fComponent->GetWorldName();
    samplerConfig.mediumIsSphere = fMediumIsSphere;
    samplerConfig.mediumRadius = fMediumRadius;
    samplerConfig.cellIsSphere = fCellIsSphere;
    samplerConfig.cellIsCylinder = fCellIsCylinder;
    fPositionSamplerAdapter.reset(new TsInVitroBindPositionSampler(samplerConfig));

    for (const auto& kv : fModeParamSources) {
        if (kv.second == "legacy") {
            G4cout << "WARNING: invitro_bind parameter '" << kv.first
                   << "' was read from legacy key. Prefer So/<Source>/ModeParams/*." << G4endl;
        }
    }

}

void TsRadioactiveTimeGeneratorBind::GettingBindingKineticParameters() {
    auto modeParam = [this](const G4String& key) { return GetFullParmName("ModeParams/" + key); };
    auto legacyParam = [this](const G4String& key) { return GetFullParmName(key); };
    G4String sourceTag;

    // Get binding kinetic parameters from the parameter manager.
    // All parameters are in s^-1 except Kd (nM). By consistency, kon is in nM^-1 s^-1.
    
    // Number of receptors per cell
    if (TsInVitroBindModeAdapter::ResolveUnitless(fPm, modeParam("ReceptorsPerCell"), legacyParam("ReceptorsPerCell"),
                                                  &fBmax, &sourceTag)) {
        fModeParamSources["receptors_per_cell"] = sourceTag;
        // Calculating concentration of receptors in nmol/L.
        // 6.022e23 is the Avogadro number, 1e9 is to convert from mol to nmol, and 1e6 is to convert from mm3 (default for volume) to L
        fRi = fBmax * fNumberOfCells * 1e9 / 6.022e23 / fPhysicalVolumes[0]->GetLogicalVolume()->GetSolid()->GetCubicVolume() * 1e6;
    }
    // koff
    if (TsInVitroBindModeAdapter::ResolveUnitless(fPm, modeParam("koff"), legacyParam("koff"), &fkoff, &sourceTag))
        fModeParamSources["koff_per_s"] = sourceTag;
    // Kd
    if (TsInVitroBindModeAdapter::ResolveUnitless(fPm, modeParam("Kd"), legacyParam("Kd"), &fKd, &sourceTag)) {
        fModeParamSources["kd_nM"] = sourceTag;
        fkon = fkoff / fKd;}
    // kon
    if (TsInVitroBindModeAdapter::ResolveUnitless(fPm, modeParam("kon"), legacyParam("kon"), &fkon, &sourceTag))
        fModeParamSources["kon_per_nM_s"] = sourceTag;
    // kint
    if (TsInVitroBindModeAdapter::ResolveUnitless(fPm, modeParam("kint"), legacyParam("kint"), &fkint, &sourceTag))
        fModeParamSources["kint_per_s"] = sourceTag;
    // krec
    if (TsInVitroBindModeAdapter::ResolveUnitless(fPm, modeParam("krec"), legacyParam("krec"), &fkrec, &sourceTag))
        fModeParamSources["krec_per_s"] = sourceTag;
    // kefflux
    if (TsInVitroBindModeAdapter::ResolveUnitless(fPm, modeParam("kefflux"), legacyParam("kefflux"), &fkefflux, &sourceTag))
        fModeParamSources["kefflux_per_s"] = sourceTag;

    // Optional file output for probability distributions.
    fSaveModelToFile = false;
    if (TsInVitroBindModeAdapter::ResolveBoolean(fPm, modeParam("SaveProbabilityDistribution"),
                                                 legacyParam("SaveProbabilityDistribution"),
                                                 &fSaveModelToFile, &sourceTag))
        fModeParamSources["save_probability_distribution"] = sourceTag;

    if (TsInVitroBindModeAdapter::ResolveString(fPm, modeParam("ProbabilityDistributionFilename"),
                                                legacyParam("ProbabilityDistributionFilename"),
                                                &fProbabilitiesFilename, &sourceTag)) {
        fModeParamSources["probability_distribution_filename"] = sourceTag;
    } else {
        fProbabilitiesFilename = "BindingProbabilities.txt";
    }

    // Getting probabilities for the manual binding model
    std::vector<G4double> manualValues;
    if (TsInVitroBindModeAdapter::ResolveUnitlessVector(fPm, modeParam("ManualBindingProbabilities"),
                                                        legacyParam("ManualBindingProbabilities"),
                                                        &manualValues, &sourceTag)) {
        fModeParamSources["manual_probabilities"] = sourceTag;
        G4int size = manualValues.size();
        // Check that all manual probabilities are provided.
        if (size != 5) {
            G4cerr << "Topas is exiting due to a serious error in source setup." << G4endl;
            G4cerr << "The manual binding model requires 5 values for the compartments:" << G4endl;
            G4cerr << "Medium (E), membrane (M), cytoplasm (I), nucleus (N) and degraded (D)." << G4endl;
            exit(1);
        }

        // Normalized to 1
        G4double sum = 0;
        for (G4int i = 0; i < size; i++) sum += manualValues[i]; // Sum of all probabilities
        fManualBindingProbabilities.clear();
        for (G4int i = 0; i < size; i++) fManualBindingProbabilities.push_back(manualValues[i] / sum); // Normalizing to 1

        // Assigning values to the binding probabilities
        fEProbability = fManualBindingProbabilities[0];
        fMProbability = fManualBindingProbabilities[1];
        fIProbability = fManualBindingProbabilities[2];
        fNProbability = fManualBindingProbabilities[3];
        fDProbability = fManualBindingProbabilities[4];
    }

    G4bool isManualBindingModel = fPm->ParameterExists(GetFullParmName("BindingModel")) &&
                                  fPm->GetStringParameter(GetFullParmName("BindingModel")) == "Manual";

    // Checking if kon and koff are defined. These parameters are required for non-manual models.
    if (!isManualBindingModel && (fkoff==-1 || fkon==-1 || fRi==-1)) {
        G4cerr << "Topas is exiting due to a serious error in source setup." << G4endl;
        G4cerr << "The following parameters are required:" << G4endl;
        if (fkoff==-1) G4cerr << "koff [1/s] ";
        if (fkon==-1) G4cerr << "kon [1/nM/s] or Kd [nM] (kon will be calculated from koff and Kd)" << G4endl;
        if (fRi==-1) G4cerr << "ReceptorsPerCell [number of receptors]" << G4endl;
        G4cerr << "Please check the parameter file." << G4endl; 
        exit(1);
    }



    // Method to set the binding kinetic model
    SettingBindingKineticModel();

}

void TsRadioactiveTimeGeneratorBind::SettingBindingKineticModel(){
    auto modeParam = [this](const G4String& key) { return GetFullParmName("ModeParams/" + key); };
    auto legacyParam = [this](const G4String& key) { return GetFullParmName(key); };
    G4String sourceTag;

    if (TsInVitroBindModeAdapter::ResolveString(fPm, modeParam("BindingModel"), legacyParam("BindingModel"),
                                                &fModelName, &sourceTag)) {
        fModeParamSources["binding_model"] = sourceTag;
    } else {
        G4cerr << "Topas is exiting due to a serious error in source setup." << G4endl;
        G4cerr << "The binding model is not defined." << G4endl;
        exit(1);
    }

    // Checking if the binding model is valid ("2compartments", "3compartments" or "4compartments")
    if (fModelName != "2compartmentsCell" && fModelName != "2compartmentsNucleus" && 
        fModelName != "3compartments" && fModelName != "4compartments" && fModelName != "Manual") {
        G4cerr << "Topas is exiting due to a serious error in source setup." << G4endl;
        G4cerr << "The binding model selected is not valid. The options are: 2compartments, 3compartments or 4compartments." << G4endl;
        exit(1);
    }

    // Check required parameters for the selected binding model.
    if (fModelName == "3compartments"){
        if (fkint == -1 || fkefflux == -1) {
            G4cerr << "Topas is exiting due to a serious error in source setup." << G4endl;
            G4cerr << "The following parameters are required for the 3compartments binding kinetic model:" << G4endl;
            if (fkint==-1) G4cerr << "kint [1/s] ";
            if (fkefflux==-1) G4cerr << "kefflux [1/s]" << G4endl;
            G4cerr << "Please check the parameter file." << G4endl; 
            exit(1);
        }
    } else if (fModelName == "4compartments") {
        if (fkint == -1 || fkrec == -1 || fkefflux == -1) {
            G4cerr << "Topas is exiting due to a serious error in source setup." << G4endl;
            G4cerr << "The following parameters are required for the 4compartments binding kinetic model:" << G4endl;
            if (fkint==-1) G4cerr << "kint [1/s] ";
            if (fkrec==-1) G4cerr << "krec [1/s] ";
            if (fkefflux==-1) G4cerr << "kefflux [1/s]" << G4endl;
            G4cerr << "Please check the parameter file." << G4endl;
            exit(1);
        }
    } else if (fModelName == "Manual"){
        fSaveModelToFile = false; // The manual model does not save the probabilities to a file
        if (fManualBindingProbabilities.empty()) {
            G4cerr << "Topas is exiting due to a serious error in source setup." << G4endl;
            G4cerr << "The manual binding model requires ManualBindingProbabilities to be defined." << G4endl;
            G4cerr << "Please check the parameter file." << G4endl;
            exit(1);
        }
    }

    // Initialize the dynamic binding model.
    fModel = new TsDynamicBindModel(fkoff, fkon, fkint, fkrec, fkefflux,
                                    fRi, fConcentration, fWashOutTime);

    showBindingModel();


}

void TsRadioactiveTimeGeneratorBind::SetNewPositionForHistory() {
    TsEventContext eventContext;
    eventContext.runID = fRunID;
    eventContext.eventID = 0;
    eventContext.currentTimeS = fCurrentTime;
    eventContext.nextTimeS = fNextTime;

    fCurrentPosition = fPositionSamplerAdapter->SamplePosition(eventContext);
    if (fPositionSamplerAdapter->IsHistorySuppressed(eventContext)) {
        if (!fSumProb) {
            G4cout << "WARNING: SUM OF PROBABILITIES IS NOT 1" << G4endl;
            fSumProb = true;
        }
        return;
    }
    setPositionForHistory(fCurrentPosition);

}

void TsRadioactiveTimeGeneratorBind::GetAdaptedSource() {
    // Get typed source.
    fSource = (TsRadioactiveTimeSourceBind *) GetSource();
    if (fFirstTimeGettingSource) {
        if (fWriteIsotopicAbundance) {
            SetUpIsotopicAbundanceFile();
        }
        // Set initial source isotopic abundance
        fSource->SetIsotopicAbundanceNames(fOrderedIsotopes);
    }
    fFirstTimeGettingSource = false;
}

void TsRadioactiveTimeGeneratorBind::UpdateForNewRun(bool force) {

    // This method is called to update the binding probabilities and the isotopic abundance of the source
    
    CalculateBindingProbabilities(fTimeList[fRunID], fNextTime, fEconcentration, fMconcentration, fIconcentration, fDconcentration);
    fSumProb = false; // Resetting the flag to check if the probabilities are properly normalized

    // UpdateForNewRun of the parent class to manage time, isotopes abundances, number of decays in each step time, etc
    TsRadioactiveTimeGenerator::UpdateForNewRun(force);

    // Printing the new run features
    G4cout << "---------------------------" << G4endl;
    G4cout << "New step time (new run) " << G4endl;
    G4cout << "Timeinit: " << fTimeList[fRunID]/3600 << " h" << G4endl; // Time variables were converted to second manually in the parent class
    G4cout << "TimeEnd: " << fNextTime/3600 << " h" << G4endl;
    G4cout << "Number of atoms " << fCurrentNAtoms << G4endl;
    G4cout << "  " << G4endl; 
    G4cout << "Medium probability: " << fEProbability << G4endl;
    G4cout << "Membrane probability: " << fMProbability << G4endl;
    G4cout << "Cytoplasm probability: " << fIProbability << G4endl;
    G4cout << "Nucleus probability: " << fNProbability << G4endl;
    G4cout << "Degraded probability: " << fDProbability << G4endl;
    G4cout << "---------------------------" << G4endl;

}

void TsRadioactiveTimeGeneratorBind::CalculateBindingProbabilities(G4double timeInit, G4double timeEnd, [[maybe_unused]] G4double fEi,
[[maybe_unused]] G4double fMi, [[maybe_unused]] G4double fIi, [[maybe_unused]] G4double fDi) {
    TsTimeStepContext context;
    context.runID = fRunID;
    context.timeStartS = timeInit;
    context.timeEndS = timeEnd;
    fKineticsModelAdapter->UpdateForStep(context);

    fCompartmentProbabilities = fKineticsModelAdapter->GetProbabilities();
    fEProbability = fCompartmentProbabilities.medium;
    fMProbability = fCompartmentProbabilities.membrane;
    fIProbability = fCompartmentProbabilities.cytoplasm;
    fNProbability = fCompartmentProbabilities.nucleus;
    fDProbability = fCompartmentProbabilities.degraded;
    fRatioIsotopesRemaining = fKineticsModelAdapter->GetRemainingActivityFraction();

}

void TsRadioactiveTimeGeneratorBind::showBindingModel() {

    G4cout << G4endl;
    G4cout << "==================================" << G4endl;
    G4cout << "========== BINDING MODEL =========" << G4endl;
    G4cout << "==================================" << G4endl;
    G4cout << G4endl;
    G4cout << "Binding model selected: " << fModelName << G4endl;
    G4cout << "fActivity: " << fActivity_permL / 1e3 << " kBq/mL" << G4endl;
    G4cout << "fSpecificActivity: " << fSpecificActivity_pernmol / 1e9 << " GBq/nmol" << G4endl;
    G4cout << "Concentration: " << fConcentration << " nmol/L" << G4endl;
    G4cout << "Receptors per cell: " << fBmax << G4endl;
    G4cout << "Concentration of receptors: " << fRi << " nmol/L" << G4endl;
    if (fModelName != "Manual"){
        if (fKd != -1) G4cout << "Kd: " << fKd << " nM" << G4endl;
        G4cout << "koff: " << fkoff << " s^-1" << G4endl;
        G4cout << "kon: " << fkon << " nM^-1 s^-1" << G4endl;
        if (fModelName == "3compartments") {
            G4cout << "kint: " << fkint << " s^-1" << G4endl;
            G4cout << "kefflux: " << fkefflux << " s^-1" << G4endl;
        } else if (fModelName == "4compartments") {
            G4cout << "kint: " << fkint << " s^-1" << G4endl;
            G4cout << "krec: " << fkrec << " s^-1" << G4endl;
            G4cout << "kefflux: " << fkefflux << " s^-1" << G4endl;
        }
    } else {
        G4cout << "Medium probability: " << fManualBindingProbabilities[0] << G4endl;
        G4cout << "Membrane probability: " << fManualBindingProbabilities[1] << G4endl;
        G4cout << "Cytoplasm probability: " << fManualBindingProbabilities[2] << G4endl;
        G4cout << "Nucleus probability: " << fManualBindingProbabilities[3] << G4endl;
        G4cout << "Degraded probability: " << fManualBindingProbabilities[4] << G4endl;
    }
    G4cout << "==================================" << G4endl;
    G4cout << G4endl;

}


void TsRadioactiveTimeGeneratorBind::SetUptakePerCell() {
    auto modeParam = [this](const G4String& key) { return GetFullParmName("ModeParams/" + key); };
    auto legacyParam = [this](const G4String& key) { return GetFullParmName(key); };
    G4String uptakePath;
    G4String sourceTag;

    // Read uptake file path if provided.
    if (TsInVitroBindModeAdapter::ResolveString(fPm, modeParam("UptakePerCellFilename"),
                                                legacyParam("UptakePerCellFilename"),
                                                &uptakePath, &sourceTag)) {
        fUptakeFilename = uptakePath;
        fModeParamSources["uptake_per_cell_filename"] = sourceTag;
    }

    // If no file is provided, assume equal uptake across all cells.
    if (fUptakeFilename.empty()){
        for (G4int i=0; i<fNumberOfCells; i++){
            fUptake1DVector.push_back(i);
            fLengthUptakeVector = fUptake1DVector.size();
        }
    }
    // If filename is provided, read the file and store the uptake per cell in a vector
    else {
        G4cout << "Reading uptake file: " << fUptakeFilename << G4endl;
        std::ifstream infile(fUptakeFilename);
        if (!infile.is_open()) {
            G4cerr << "Topas is exiting due to a serious error in source setup." << G4endl;
            G4cerr << "Could not open uptake file: " << fUptakeFilename << G4endl;
            exit(1);
        }
        // Read file and load values.
        G4float value;
        G4int id;
        // Set uptake vector size to the number of cells.
        fUptakePerCell.resize(int(fNumberOfCells),0.0);
        while (infile >> id >> value) {
            fUptakePerCell[id] = value;
        }
        infile.close();
        // Normalize uptake to the minimum positive value.
        G4int CellUptake;
        G4float minValue = 1.0e9; // Initialize to a large value
        // Find minimum value greater than 0.
        for (G4int i=0; i<fNumberOfCells; i++){
            if (fUptakePerCell[i] > 0 && fUptakePerCell[i] < minValue) {
                minValue = fUptakePerCell[i];
            }
        }

        for (G4int i=0; i<fNumberOfCells; i++){
            fUptakePerCell[i] = fUptakePerCell[i]/minValue;
            CellUptake = std::round(fUptakePerCell[i]);
            for (G4int j=0; j<CellUptake; j++) {
                fUptake1DVector.push_back(i);
            }
        }
        fLengthUptakeVector = fUptake1DVector.size();
    }
    G4cout << "Uptake vector size: " << fLengthUptakeVector << G4endl;
}

G4int TsRadioactiveTimeGeneratorBind::getCellId() {

    // This method returns a random cell ID based on the uptake per cell
    // If the uptake per cell is not defined, it returns a random cell ID

    if (fLengthUptakeVector == 0) {
        G4cerr << "Topas is exiting due to a serious error in source setup." << G4endl;
        G4cerr << "Uptake per cell vector is empty. Please check the parameter file." << G4endl;
        exit(1);
    }

    // Sampling a random cell ID from the uptake vector
    G4int cellId = fUptake1DVector[std::floor(G4UniformRand() * fLengthUptakeVector)];
    return cellId;

}

G4String TsRadioactiveTimeGeneratorBind::GetModeMetadataJson() const
{
    std::ostringstream out;
    out << std::setprecision(16);
    out << "{";
    out << "\"binding_model\":\"" << fModelName << "\",";
    out << "\"activity_per_ml\":" << fActivity_permL << ",";
    out << "\"specific_activity_per_nmol\":" << fSpecificActivity_pernmol << ",";
    out << "\"concentration_nmol_per_l\":" << fConcentration << ",";
    out << "\"washout_time_s\":" << fWashOutTime << ",";
    out << "\"receptors_per_cell\":" << fBmax << ",";
    out << "\"kd_nM\":" << fKd << ",";
    out << "\"koff_per_s\":" << fkoff << ",";
    out << "\"kon_per_nM_s\":" << fkon << ",";
    out << "\"kint_per_s\":" << fkint << ",";
    out << "\"krec_per_s\":" << fkrec << ",";
    out << "\"kefflux_per_s\":" << fkefflux << ",";
    out << "\"manual_probabilities\":[";
    for (size_t i = 0; i < fManualBindingProbabilities.size(); i++) {
        out << fManualBindingProbabilities[i];
        if (i + 1 < fManualBindingProbabilities.size())
            out << ",";
    }
    out << "]";
    out << ",\"param_sources\":{";
    G4bool first = true;
    for (const auto& kv : fModeParamSources) {
        if (!first)
            out << ",";
        out << "\"" << kv.first << "\":\"" << kv.second << "\"";
        first = false;
    }
    out << "}";
    out << "}";
    return out.str();
}
