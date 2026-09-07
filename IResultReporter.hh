//
// Interface for writing run results/metadata in a mode-agnostic way.
//

#ifndef IResultReporter_hh
#define IResultReporter_hh

#include <vector>

#include "globals.hh"

#include "TsNormalizationContext.hh"

class IResultReporter
{
public:
    virtual ~IResultReporter() {}

    virtual void WriteRunMetadata(const G4String& outputPath,
                                  const G4String& mode,
                                  const G4String& modeSettingsJson,
                                  const std::vector<TsNormalizationContext>& entries) const = 0;
};

#endif
