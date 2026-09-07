//
// Interface for time/decay run-step bookkeeping and normalization.
//

#ifndef ITimeDecayKernel_hh
#define ITimeDecayKernel_hh

#include <type_traits>

#include "globals.hh"

#include "TsNormalizationContext.hh"

struct TsTimeDecayState
{
    G4int runID = 0;
    G4double timeStartS = 0.;
    G4double timeEndS = 0.;
    G4double numberOfIndependentDecaysDuringThisStep = 0.;
    G4double numberOfIndependentDecaysDuringFirstStep = 0.;
    G4double historiesPerStep = 0.;
    G4bool correctByNumberOfHistories = true;
    G4double initialActivityBq = 0.;
};

static_assert(std::is_standard_layout<TsTimeDecayState>::value,
              "TsTimeDecayState must remain standard-layout for stable adapter usage.");

class ITimeDecayKernel
{
public:
    virtual ~ITimeDecayKernel() {}

    virtual TsNormalizationContext BuildNormalizationContext(const TsTimeDecayState& state) const = 0;
    virtual G4double ComputePrimaryWeight(const TsTimeDecayState& state) const = 0;
};

#endif
