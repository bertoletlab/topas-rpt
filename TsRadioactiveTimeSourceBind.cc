// Particle Source for RadioactiveTimeSourceBind
//
// Created by Daniel Suarez and Victor Valladolid on 12/18/23.
//

#include "TsRadioactiveTimeSourceBind.hh"
#include "TsParameterManager.hh"
#include <fstream>

TsRadioactiveTimeSourceBind::TsRadioactiveTimeSourceBind(TsParameterManager* pM, TsSourceManager* psM, G4String sourceName)
        : TsRadioactiveTimeSource(pM, psM, sourceName){}

TsRadioactiveTimeSourceBind::~TsRadioactiveTimeSourceBind(){}

void TsRadioactiveTimeSourceBind::ResolveParameters() {
    TsRadioactiveTimeSource::ResolveParameters();
}