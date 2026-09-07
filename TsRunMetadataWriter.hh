//
// Writes run metadata for radioactive time-source runs
//

#ifndef TsRunMetadataWriter_hh
#define TsRunMetadataWriter_hh

#include <fstream>
#include <iomanip>
#include <vector>

#include "globals.hh"

#include "TsNormalizationContext.hh"

class TsRunMetadataWriter
{
public:
    static const char* NormalizationSpecVersion()
    {
        return "tsrts.norm.v1";
    }

    static const char* NormalizationBasisText()
    {
        return "primary_weight := n_decays_step / n_histories_step";
    }

    static const char* SchemaVersion()
    {
        return "tsrts.run_metadata.v1";
    }

    static void Write(const G4String& outputPath, const G4String& mode, const G4String& modeSettingsJson,
                      const std::vector<TsNormalizationContext>& entries)
    {
        std::ofstream out(outputPath, std::ios::trunc);
        out << std::setprecision(16);
        out << "{\n";
        out << "  \"schema_version\": \"" << SchemaVersion() << "\",\n";
        out << "  \"normalization_spec\": \"" << NormalizationSpecVersion() << "\",\n";
        out << "  \"normalization_basis\": \"" << NormalizationBasisText() << "\",\n";
        out << "  \"mode\": \"" << mode << "\",\n";
        out << "  \"mode_settings\": " << modeSettingsJson << ",\n";
        out << "  \"entries\": [\n";
        for (size_t i = 0; i < entries.size(); i++) {
            const TsNormalizationContext& entry = entries[i];
            out << "    {\n";
            out << "      \"run_id\": " << entry.runID << ",\n";
            out << "      \"time_start_s\": " << entry.timeStartS << ",\n";
            out << "      \"time_end_s\": " << entry.timeEndS << ",\n";
            out << "      \"step_duration_s\": " << entry.stepDurationS << ",\n";
            out << "      \"n_decays_step\": " << entry.numberOfIndependentDecaysDuringThisStep << ",\n";
            out << "      \"n_decays_first\": " << entry.numberOfIndependentDecaysDuringFirstStep << ",\n";
            out << "      \"n_histories_step\": " << entry.historiesPerStep << ",\n";
            out << "      \"correct_by_number_of_histories\": "
                << (entry.correctByNumberOfHistories ? "true" : "false") << ",\n";
            out << "      \"initial_activity_bq\": " << entry.initialActivityBq << ",\n";
            out << "      \"effective_activity_bq\": " << entry.effectiveActivityBq << ",\n";
            out << "      \"primary_weight\": " << entry.ComputePrimaryWeight() << "\n";
            out << "    }" << (i + 1 < entries.size() ? "," : "") << "\n";
        }
        out << "  ]\n";
        out << "}\n";
    }
};

#endif
