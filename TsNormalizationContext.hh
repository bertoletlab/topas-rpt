//
// Normalization metadata snapshot for a single run step.
// Ownership: producer (generator) stores immutable copies in per-run history.
//

#ifndef TsNormalizationContext_hh
#define TsNormalizationContext_hh

#include "globals.hh"

class TsNormalizationContext
{
public:
    TsNormalizationContext()
    {
        runID = 0;
        timeStartS = 0.;
        timeEndS = 0.;
        stepDurationS = 0.;
        numberOfIndependentDecaysDuringThisStep = 0.;
        numberOfIndependentDecaysDuringFirstStep = 0.;
        historiesPerStep = 0.;
        correctByNumberOfHistories = true;
        initialActivityBq = 0.;
        effectiveActivityBq = 0.;
    }

    G4double ComputePrimaryWeight() const
    {
        if (historiesPerStep <= 0.)
            return 0.;

        G4double weight = numberOfIndependentDecaysDuringThisStep / historiesPerStep;
        if (!correctByNumberOfHistories && numberOfIndependentDecaysDuringThisStep > 0.)
            weight *= numberOfIndependentDecaysDuringFirstStep / numberOfIndependentDecaysDuringThisStep;
        return weight;
    }

    G4int runID;
    G4double timeStartS;
    G4double timeEndS;
    G4double stepDurationS;

    G4double numberOfIndependentDecaysDuringThisStep;
    G4double numberOfIndependentDecaysDuringFirstStep;
    G4double historiesPerStep;

    G4bool correctByNumberOfHistories;
    G4double initialActivityBq;
    G4double effectiveActivityBq;
};

#endif
