// Particle Source for RadioactiveTimeSource
//
// Created by ai925 on 7/10/23.
//

#include "TsRadioactiveTimeSource.hh"
#include "TsParameterManager.hh"
#include "G4DeexPrecoParameters.hh"
#include "G4NuclearLevelData.hh"
#include "G4Exception.hh"
#include <fstream>

TsRadioactiveTimeSource::TsRadioactiveTimeSource(TsParameterManager* pM, TsSourceManager* psM, G4String sourceName)
        : TsSource(pM, psM, sourceName)
{
    ResolveParameters();
    fIsFinalRun = false;
}

TsRadioactiveTimeSource::~TsRadioactiveTimeSource()
{
}

void TsRadioactiveTimeSource::ResolveParameters() {
    TsSource::ResolveParameters();
    // DeexciteDaughters needs the tsrts_deexcitation module: it switches on the
    // internal-conversion shell data, isomer production and Auger options, which only a
    // physics constructor can set (before initialization), and installs the atomic
    // de-excitation that keeps stale vacancies out of the next decay. Geant4 11.3 stores the
    // IC data by default, so the IC flag alone no longer shows whether the module is there;
    // the module list of the physics list is checked as well.
    if (fPm->ParameterExists(GetFullParmName("DeexciteDaughters")) &&
        fPm->GetBooleanParameter(GetFullParmName("DeexciteDaughters"))) {
        const G4String listName = fPm->GetStringParameter("Ph/ListName");
        G4String modulesParm = "Ph/" + listName + "/Modules";
        G4bool hasModule = false;
        if (fPm->ParameterExists(modulesParm)) {
            G4String* modules = fPm->GetStringVector(modulesParm);
            for (G4int i = 0; i < fPm->GetVectorLength(modulesParm); ++i) {
                G4String name = modules[i];
                G4StrUtil::to_lower(name);
                if (name == "tsrts_deexcitation") hasModule = true;
            }
        }
        if (!hasModule || !G4NuclearLevelData::GetInstance()->GetParameters()->StoreICLevelData()) {
            G4ExceptionDescription msg;
            msg << GetFullParmName("DeexciteDaughters") << " is True, but "
                << (hasModule ? G4String("internal-conversion data are off")
                    : fPm->ParameterExists(modulesParm)
                        ? "the \"tsrts_deexcitation\" physics module is not in " + modulesParm
                        : "physics list \"" + listName + "\" has no module list (" + modulesParm
                          + "); DeexciteDaughters needs a Geant4_Modular list")
                << ", so the de-excitation products would be incomplete. Add the "
                << "\"tsrts_deexcitation\" physics module after the EM module.";
            G4Exception("TsRadioactiveTimeSource::ResolveParameters()", "TsRADIOACTIVE020",
                        FatalException, msg);
        }
    }
    // Same defaults as the generator, which writes the rows this source completes at the end.
    fWriteIsotopicAbundance = true;
    if (fPm->ParameterExists(GetFullParmName("WriteIsotopicAbundance")))
        fWriteIsotopicAbundance = fPm->GetBooleanParameter(GetFullParmName("WriteIsotopicAbundance"));
    fIsotopicAbundanceFileName = "isotopic_abundance.csv";
    if (fPm->ParameterExists(GetFullParmName("WriteIsotopicAbundanceFileName")))
        fIsotopicAbundanceFileName = fPm->GetStringParameter(GetFullParmName("WriteIsotopicAbundanceFileName"));
}

void TsRadioactiveTimeSource::UpdateForNewRun(G4bool ) {
}
void TsRadioactiveTimeSource::UpdateForEndOfRun()
{
    if (fWriteIsotopicAbundance && fIsFinalRun) {
        std::string oldFileName = fIsotopicAbundanceFileName;
        // Next to the output, so that two sources writing in one folder do not share it.
        std::string tempFileName = oldFileName + ".tmp";

        // Open original file and temporary file
        std::ifstream oldFile(oldFileName);
        std::ofstream tempFile(tempFileName);

        // Write header to temporary file
        std::string newHeader = "Time";
        for (auto name : fIsotopicAbundanceNames) {
            newHeader += "," + name;
        }
        // Add fractions of activity
        newHeader += ",FractionOfDecaysAtTime0";
        newHeader += "\n";
        tempFile << newHeader;

        // Count the number of commas in the new header
        G4int numCommasInHeader = std::count(newHeader.begin(), newHeader.end(), ',') - 1; // except for act fraction

        // Append remaining lines from the original file to the temporary file adding 0s when necessary
        std::string line;
        G4int lineNum = 0;
        while (std::getline(oldFile, line)) {
            G4int numCommasInRow = std::count(line.begin(), line.end(), ',');
            G4int zerosToAdd = numCommasInHeader - numCommasInRow;
            for (G4int i = 0; i < zerosToAdd; i++) {
                line += ",0";
            }
            // Add fractions of activity
            line += "," + std::to_string(fFractionsOfActivityAtTime0[lineNum]);
            lineNum++;
            tempFile << line << "\n";
        }

        // Close files
        oldFile.close();
        tempFile.close();

        // Delete original file and rename temporary file
        remove(oldFileName.c_str());
        rename(tempFileName.c_str(), oldFileName.c_str());
    }
}


void TsRadioactiveTimeSource::SetNewNumberOfHistories(G4int n) {
    fNumberOfHistoriesInRun = n;
}
