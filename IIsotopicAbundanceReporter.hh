//
// Interface for isotopic abundance output reporting.
//

#ifndef IIsotopicAbundanceReporter_hh
#define IIsotopicAbundanceReporter_hh

#include <map>
#include <vector>

#include "globals.hh"

class IIsotopicAbundanceReporter
{
public:
    virtual ~IIsotopicAbundanceReporter() {}

    virtual void Initialize(const G4String& outputPath,
                            const std::vector<G4String>& orderedIsotopes,
                            const std::map<G4String, G4double>& abundances,
                            G4double currentTimeS) const = 0;

    virtual void Append(const G4String& outputPath,
                        const std::vector<G4String>& orderedIsotopes,
                        const std::map<G4String, G4double>& abundances,
                        G4double currentTimeS) const = 0;
};

#endif
