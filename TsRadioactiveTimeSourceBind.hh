//
// Created by Victor Valladolid on 12/18/23.
//

#ifndef TsRadioactiveTimeSourceBind_hh
#define TsRadioactiveTimeSourceBind_hh

#include "TsSource.hh"
#include "TsParameterManager.hh"
#include "TsRadioactiveTimeSource.hh"

class TsRadioactiveTimeSourceBind : public TsRadioactiveTimeSource
{
public:
    TsRadioactiveTimeSourceBind(TsParameterManager* pM, TsSourceManager* psM, G4String sourceName);
    ~TsRadioactiveTimeSourceBind();

    void ResolveParameters();
};
#endif /* TsRadioactiveTimeSourceBind_hh */
