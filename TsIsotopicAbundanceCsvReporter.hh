//
// CSV isotopic abundance reporter.
//

#ifndef TsIsotopicAbundanceCsvReporter_hh
#define TsIsotopicAbundanceCsvReporter_hh

#include <cstdio>
#include <fstream>

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
        out << currentTimeS;
        for (size_t i = 0; i < orderedIsotopes.size(); i++) {
            auto it = abundances.find(orderedIsotopes[i]);
            out << "," << (it != abundances.end() ? it->second : 0.);
        }
        out << "\n";
    }
};

#endif
