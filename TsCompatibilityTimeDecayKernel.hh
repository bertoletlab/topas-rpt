//
// Compatibility kernel implementation preserving existing normalization behavior.
//

#ifndef TsCompatibilityTimeDecayKernel_hh
#define TsCompatibilityTimeDecayKernel_hh

#include "ITimeDecayKernel.hh"

class TsCompatibilityTimeDecayKernel : public ITimeDecayKernel
{
public:
    TsNormalizationContext BuildNormalizationContext(const TsTimeDecayState& state) const override
    {
        TsNormalizationContext context;
        context.runID = state.runID;
        context.timeStartS = state.timeStartS;
        context.timeEndS = state.timeEndS;
        context.stepDurationS = state.timeEndS - state.timeStartS;
        context.numberOfIndependentDecaysDuringThisStep = state.numberOfIndependentDecaysDuringThisStep;
        context.numberOfIndependentDecaysDuringFirstStep = state.numberOfIndependentDecaysDuringFirstStep;
        context.historiesPerStep = state.historiesPerStep;
        context.correctByNumberOfHistories = state.correctByNumberOfHistories;
        context.initialActivityBq = state.initialActivityBq;
        context.effectiveActivityBq =
                context.stepDurationS > 0. ? context.numberOfIndependentDecaysDuringThisStep / context.stepDurationS : 0.;
        return context;
    }

    G4double ComputePrimaryWeight(const TsTimeDecayState& state) const override
    {
        return BuildNormalizationContext(state).ComputePrimaryWeight();
    }
};

#endif
