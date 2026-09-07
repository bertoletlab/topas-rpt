//
// Created by ai925 on 7/10/23.
//

#ifndef TsRadioactiveTimeSource_hh
#define TsRadioactiveTimeSource_hh

#include "TsSource.hh"
#include "TsParameterManager.hh"

class TsRadioactiveTimeSource : public TsSource
{
public:
    TsRadioactiveTimeSource(TsParameterManager* pM, TsSourceManager* psM, G4String sourceName);
    ~TsRadioactiveTimeSource();

    void UpdateForNewRun(G4bool force);
    void UpdateForEndOfRun();
    void SetNewNumberOfHistories(G4int n);

    void inline SetIsotopicAbundanceNames(std::vector<G4String> names) { fIsotopicAbundanceNames = names; }
    void inline SetFractionsOfActivityAtTime0(std::vector<G4double> fractions) { fFractionsOfActivityAtTime0 = fractions; }
    void inline SetIsFinalRun(G4bool isFinalRun) { fIsFinalRun = isFinalRun; }

    void ResolveParameters();

private:
    G4bool fWriteIsotopicAbundance;
    G4String fIsotopicAbundanceFileName;
    std::vector<G4String> fIsotopicAbundanceNames;
    G4bool fIsFinalRun;

    std::vector<G4double> fFractionsOfActivityAtTime0;
};
#endif
