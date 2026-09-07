//
// Default run-metadata reporter implementation.
//

#ifndef TsRunMetadataReporter_hh
#define TsRunMetadataReporter_hh

#include "IResultReporter.hh"
#include "TsRunMetadataWriter.hh"

class TsRunMetadataReporter : public IResultReporter
{
public:
    void WriteRunMetadata(const G4String& outputPath,
                          const G4String& mode,
                          const G4String& modeSettingsJson,
                          const std::vector<TsNormalizationContext>& entries) const override
    {
        TsRunMetadataWriter::Write(outputPath, mode, modeSettingsJson, entries);
    }
};

#endif
