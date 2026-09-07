//
// Default mode summary reporter.
//

#ifndef TsModeSummaryReporter_hh
#define TsModeSummaryReporter_hh

#include <fstream>

#include "IModeSummaryReporter.hh"

class TsModeSummaryReporter : public IModeSummaryReporter
{
public:
    void WriteSummary(const G4String& outputPath,
                      const G4String& mode,
                      const G4String& modeSettingsJson) const override
    {
        std::ofstream out(outputPath, std::ios::trunc);
        out << "{\n";
        out << "  \"schema_version\": \"tsrts.mode_summary.v1\",\n";
        out << "  \"normalization_spec\": \"tsrts.norm.v1\",\n";
        out << "  \"normalization_basis\": \"primary_weight := n_decays_step / n_histories_step\",\n";
        out << "  \"mode\": \"" << mode << "\",\n";
        out << "  \"mode_settings\": " << modeSettingsJson << "\n";
        out << "}\n";
    }
};

#endif
