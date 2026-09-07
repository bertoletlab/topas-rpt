//
// invitro_bind kinetics model adapter
//

#ifndef TsInVitroBindKineticsModel_hh
#define TsInVitroBindKineticsModel_hh

#include <vector>

#include "globals.hh"

#include "ITsKineticsModel.hh"
#include "TsDynamicBindModel.hh"

struct TsInVitroBindKineticsState
{
    G4double* econcentration;
    G4double* mconcentration;
    G4double* iconcentration;
    G4double* nconcentration;
    G4double* dconcentration;
    G4double* ratioIsotopesRemaining;
    G4double* currentNAtoms;
    G4double initialNAtoms;
    G4double totalConcentration;
};

class TsInVitroBindKineticsModel : public ITsKineticsModel
{
public:
    TsInVitroBindKineticsModel(TsDynamicBindModel* model, const G4String& modelName,
                               const G4String& probabilitiesFilename,
                               const TsInVitroBindKineticsState& state)
        : fModel(model), fModelName(modelName), fProbabilitiesFilename(probabilitiesFilename), fState(state)
    {
    }

    void UpdateForStep(const TsTimeStepContext& context) override
    {
        UpdateModel(context.timeStartS, context.timeEndS, false, fProbabilitiesFilename);

        std::vector<G4double> results = fModel->GetModelResults();
        *fState.econcentration = results[0];
        *fState.mconcentration = results[1];
        *fState.iconcentration = results[2];
        *fState.nconcentration = results[3];
        *fState.dconcentration = results[4];
        *fState.ratioIsotopesRemaining = results[5];
        *fState.currentNAtoms = fState.initialNAtoms * (*fState.ratioIsotopesRemaining);

        if (*fState.ratioIsotopesRemaining <= 0. || fState.totalConcentration <= 0.) {
            fProbabilities.medium = 0.;
            fProbabilities.membrane = 0.;
            fProbabilities.cytoplasm = 0.;
            fProbabilities.nucleus = 0.;
            fProbabilities.degraded = 0.;
            return;
        }

        fProbabilities.medium = *fState.econcentration / fState.totalConcentration / *fState.ratioIsotopesRemaining;
        fProbabilities.membrane =
                *fState.mconcentration / fState.totalConcentration / *fState.ratioIsotopesRemaining;
        fProbabilities.cytoplasm =
                *fState.iconcentration / fState.totalConcentration / *fState.ratioIsotopesRemaining;
        fProbabilities.nucleus =
                *fState.nconcentration / fState.totalConcentration / *fState.ratioIsotopesRemaining;
        fProbabilities.degraded =
                *fState.dconcentration / fState.totalConcentration / *fState.ratioIsotopesRemaining;
    }

    TsCompartmentProbabilities GetProbabilities() const override { return fProbabilities; }

    double GetRemainingActivityFraction() const override { return *fState.ratioIsotopesRemaining; }

    void WriteProbabilityTrace(double timeStartS, double timeEndS, const char* filename) override
    {
        G4String traceFilename = fProbabilitiesFilename;
        if (filename)
            traceFilename = filename;
        UpdateModel(timeStartS, timeEndS, true, traceFilename);
    }

private:
    void UpdateModel(G4double timeStartS, G4double timeEndS, G4bool isWrite, const G4String& filename)
    {
        if (fModelName == "2compartmentsCell") {
            fModel->UpdateModel2compartmentsCell(timeStartS, timeEndS, *fState.econcentration,
                                                 *fState.iconcentration, isWrite, filename);
        } else if (fModelName == "2compartmentsNucleus") {
            fModel->UpdateModel2compartmentsNucleus(timeStartS, timeEndS, *fState.econcentration,
                                                    *fState.nconcentration, isWrite, filename);
        } else if (fModelName == "3compartments") {
            fModel->UpdateModel3compartments(timeStartS, timeEndS, *fState.econcentration, *fState.mconcentration,
                                             *fState.iconcentration, isWrite, filename);
        } else if (fModelName == "4compartments") {
            fModel->UpdateModel4compartments(timeStartS, timeEndS, *fState.econcentration, *fState.mconcentration,
                                             *fState.iconcentration, *fState.dconcentration, isWrite, filename);
        } else if (fModelName == "Manual") {
            fModel->UpdateModelManual(timeStartS, timeEndS, *fState.econcentration, *fState.mconcentration,
                                      *fState.iconcentration, *fState.nconcentration, *fState.dconcentration);
        }
    }

    TsDynamicBindModel* fModel;
    G4String fModelName;
    G4String fProbabilitiesFilename;
    TsInVitroBindKineticsState fState;
    TsCompartmentProbabilities fProbabilities;
};

#endif
