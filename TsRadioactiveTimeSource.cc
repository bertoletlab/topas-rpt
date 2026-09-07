// Particle Source for RadioactiveTimeSource
//
// Created by ai925 on 7/10/23.
//

#include "TsRadioactiveTimeSource.hh"
#include "TsParameterManager.hh"
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
    fWriteIsotopicAbundance = true;
    if (fPm->ParameterExists(GetFullParmName("WriteIsotopicAbundance"))) {
        fWriteIsotopicAbundance = fPm->GetBooleanParameter(GetFullParmName("WriteIsotopicAbundance"));
        fIsotopicAbundanceFileName = "isotopic_abundance.csv";
        if (fPm->ParameterExists(GetFullParmName("WriteIsotopicAbundanceFileName"))) {
            fIsotopicAbundanceFileName = fPm->GetStringParameter(GetFullParmName("WriteIsotopicAbundanceFileName"));
        }
    }
}

void TsRadioactiveTimeSource::UpdateForNewRun(G4bool ) {
}
void TsRadioactiveTimeSource::UpdateForEndOfRun()
{
    if (fWriteIsotopicAbundance && fIsFinalRun) {
        std::string oldFileName = fIsotopicAbundanceFileName;
        std::string tempFileName = "temp.csv";

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
