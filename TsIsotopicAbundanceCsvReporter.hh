//
// CSV isotopic abundance reporter.
//

#ifndef TsIsotopicAbundanceCsvReporter_hh
#define TsIsotopicAbundanceCsvReporter_hh

#include <cstdio>
#include <fstream>
#include <iomanip>
#include <sstream>

#include "IIsotopicAbundanceReporter.hh"

class TsIsotopicAbundanceCsvReporter : public IIsotopicAbundanceReporter
{
public:
    void Initialize(const G4String& outputPath,
                    const std::vector<G4String>& orderedIsotopes,
                    const std::map<G4String, G4double>& abundances,
                    G4double currentTimeS) const override
    {
        std::remove(outputPath.c_str());
        std::ofstream out(outputPath, std::ios::app);
        WriteRow(out, orderedIsotopes, abundances, currentTimeS);
    }

    void Append(const G4String& outputPath,
                const std::vector<G4String>& orderedIsotopes,
                const std::map<G4String, G4double>& abundances,
                G4double currentTimeS) const override
    {
        std::ofstream out(outputPath, std::ios::app);
        WriteRow(out, orderedIsotopes, abundances, currentTimeS);
    }

private:
    static void WriteRow(std::ofstream& out,
                         const std::vector<G4String>& orderedIsotopes,
                         const std::map<G4String, G4double>& abundances,
                         G4double currentTimeS)
    {
        // Full precision: with the default 6 digits, 1209600 s was written as 1.2096e+06.
        std::ostringstream time;
        time << std::setprecision(15) << currentTimeS;
        out << time.str();
        for (size_t i = 0; i < orderedIsotopes.size(); i++) {
            auto it = abundances.find(orderedIsotopes[i]);
            out << "," << (it != abundances.end() ? it->second : 0.);
        }
        out << "\n";
    }
};

#endif
