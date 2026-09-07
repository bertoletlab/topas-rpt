//
// Interface for mode summary reporting.
//

#ifndef IModeSummaryReporter_hh
#define IModeSummaryReporter_hh

#include "globals.hh"

class IModeSummaryReporter
{
public:
    virtual ~IModeSummaryReporter() {}

    virtual void WriteSummary(const G4String& outputPath,
                              const G4String& mode,
                              const G4String& modeSettingsJson) const = 0;
};

#endif
