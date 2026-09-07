//
// Position sampler interface for source modes
//

#ifndef ITsPositionSampler_hh
#define ITsPositionSampler_hh

#include "G4ThreeVector.hh"

struct TsEventContext
{
    int runID;
    int eventID;
    double currentTimeS;
    double nextTimeS;
};

class ITsPositionSampler
{
public:
    virtual ~ITsPositionSampler() {}
    virtual G4ThreeVector SamplePosition(const TsEventContext& context) = 0;
    virtual bool IsHistorySuppressed(const TsEventContext&) { return false; }
};

#endif
