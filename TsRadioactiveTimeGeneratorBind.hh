#ifndef TSRADIOACTIVETIMESOURCE_TSRADIOACTIVETIMEGENERATORBIND_H
#define TSRADIOACTIVETIMESOURCE_TSRADIOACTIVETIMEGENERATORBIND_H

#include "TsCellMonolayer.hh"
#include "TsParameterManager.hh"

#include "TsDynamicBindModel.hh"
#include "ITsKineticsModel.hh"
#include "ITsPositionSampler.hh"
#include "TsInVitroBindKineticsModel.hh"
#include "TsInVitroBindModeAdapter.hh"
#include "TsInVitroBindPositionSampler.hh"

#include "TsRadioactiveTimeSourceBind.hh"
#include "TsRadioactiveTimeGenerator.hh"

#include "TsSamplingAtVolumes.hh"
#include <memory>
#include <map>


// Particle generator for RadioactiveTimeSourceBind.
//
// Created by Victor Valladolid on 12/18/23.
//

class TsRadioactiveTimeGeneratorBind : public TsRadioactiveTimeGenerator {
public:
    TsRadioactiveTimeGeneratorBind(TsParameterManager *pM, TsGeometryManager *gM, TsGeneratorManager *pgM,
                                   G4String sourceName);

    ~TsRadioactiveTimeGeneratorBind();

    void ResolveParameters() override; // Getting experimental parameters
    void GettingBindingKineticParameters(); // Getting binding kinetic parameters
    void SettingBindingKineticModel(); // Setting binding kinetic model
    void CalculateBindingProbabilities(G4double timeInit, G4double timeEnd, 
                                        G4double fEi, G4double fMi, G4double fIi, G4double fDi); // Calculate the binding probabilities
    void SetNewPositionForHistory() override; // Set the new position for the decay
    void GetAdaptedSource() override; // Get source
    void UpdateForNewRun(G4bool force) override; // All the parameters, included probabilities, are updated in this method
    void showBindingModel(); // Method to show the binding model
    void SetUptakePerCell(); // Method to set the uptake per cell
    G4String GetModeMetadataJson() const override;

private:

    TsRadioactiveTimeSourceBind* fSource; // Source

    // All parameters are initialized to -1, to check if they are set in the parameter file
    // Probabilities are initialized to 0 for models that do not use all compartments.

    // Experimental condition and RPT features
    G4double fActivity_permL = -1; // Activity (per ml)
    G4double fSpecificActivity_pernmol = -1; // Specific activity (per nmol)
    G4double fConcentration; // Concentration (nM)
    G4double fNumberOfCells; // Number of cells
    G4double fWashOutTime; // Washout time (s)

    // Binding kinetic parameters
    TsDynamicBindModel* fModel; // Binding model
    G4double fBmax = -1; // Number of receptors per cell (cte)
    G4double fRi = -1; // Total concentration of receptors (nM)
    G4double fkon = -1; // From medium to membrane
    G4double fkoff = -1; // From membrane to medium
    G4double fKd = -1; // Dissociation constant
    G4double fkint = -1; // From membrane to cytoplasm
    G4double fkrec = -1; // From cytoplasm to membrane
    G4double fkefflux = -1; // From cytoplasm to medium (or degraded)
    G4String fModelName; // Binding model name
    std::vector<G4double> fManualBindingProbabilities = {}; // Binding probabilities for manual model
    G4bool fSaveModelToFile = false; // Flag to save the probability distribution to a file
    G4String fProbabilitiesFilename; // Filename to save the probability distribution

    // Internal parameters of binding model
    G4double fEconcentration = 0; // Medium
    G4double fMconcentration = 0; // Membrane
    G4double fIconcentration = 0; // Cytoplasm
    G4double fNconcentration = 0; // Nucleus
    G4double fDconcentration = 0; // Degraded
    G4double fEProbability = 0; // Probability of decay in the medium
    G4double fMProbability = 0; // Probability of decay in the membrane
    G4double fIProbability = 0; // Probability of decay in the cytoplasm
    G4double fNProbability = 0; // Probability of decay in the nucleus
    G4double fDProbability = 0; // Probability of decay in the degraded
    G4double fRatioIsotopesRemaining; // Fraction of isotopes remaining (to recalculate after washout)
    G4bool fSumProb; // Flag to check if the sum of probabilities is 1
    TsCompartmentProbabilities fCompartmentProbabilities;
    std::unique_ptr<ITsKineticsModel> fKineticsModelAdapter;
    std::unique_ptr<ITsPositionSampler> fPositionSamplerAdapter;
    std::map<G4String, G4String> fModeParamSources;

    // Geometry parameters
    std::vector<G4ThreeVector> fCellPositions;
    std::vector<G4double> fCellInfo;
    std::vector<G4double> fMonolayerInfo;
    std::vector<G4VPhysicalVolume*> fPhysicalVolumes;
    G4String fCellGeometry;

    // Medium shape
    G4bool fMediumIsSphere; // Flag to check if the medium is a sphere
    G4double fMediumRadius;

    // Geometry flags for efficient branch selection.
    G4bool fCellIsSphere = false; // Flag to check if the cell is a sphere
    G4bool fCellIsCylinder = false; // Flag to check if the cell is a cylinder


    // Sampling parameters
    TsSamplingAtVolumes* SamplingPosition; // Sampling object (class to sample points in volumes at surfaces)
    G4ThreeVector fCurrentPosition; // Current position of the decay. To be overwritten in the method SetNewPositionForHistory    

    // Uptake per cell
    std::vector<G4float> fUptakePerCell;
    std::vector<G4int> fUptake1DVector;
    G4int fLengthUptakeVector;
    std::string fUptakeFilename;
    G4int getCellId();


};


#endif //TSRADIOACTIVETIMESOURCE_TSRADIOACTIVETIMEGENERATORBIND_H
