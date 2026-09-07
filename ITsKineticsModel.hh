//
// Kinetics model interface for source modes
//

#ifndef ITsKineticsModel_hh
#define ITsKineticsModel_hh

struct TsTimeStepContext
{
    int runID;
    double timeStartS;
    double timeEndS;
};

struct TsCompartmentProbabilities
{
    double medium;
    double membrane;
    double cytoplasm;
    double nucleus;
    double degraded;

    TsCompartmentProbabilities()
        : medium(1.), membrane(0.), cytoplasm(0.), nucleus(0.), degraded(0.)
    {
    }
};

class ITsKineticsModel
{
public:
    virtual ~ITsKineticsModel() {}
    virtual void UpdateForStep(const TsTimeStepContext& context) = 0;
    virtual TsCompartmentProbabilities GetProbabilities() const = 0;
    virtual double GetRemainingActivityFraction() const = 0;
    virtual void WriteProbabilityTrace(double timeStartS, double timeEndS, const char* filename)
    {
        (void)timeStartS;
        (void)timeEndS;
        (void)filename;
    }
};

#endif
