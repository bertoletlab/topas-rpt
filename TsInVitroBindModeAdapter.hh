//
// invitro_bind parameter adapter:
// prefers ModeParams/* and falls back to legacy keys with source tracking.
//

#ifndef TsInVitroBindModeAdapter_hh
#define TsInVitroBindModeAdapter_hh

#include "TsParameterManager.hh"

class TsInVitroBindModeAdapter
{
public:
    static G4bool ResolveUnitless(TsParameterManager* pm,
                                  const G4String& fullModeParam,
                                  const G4String& fullLegacyParam,
                                  G4double* outValue,
                                  G4String* outSource)
    {
        if (pm->ParameterExists(fullModeParam)) {
            *outValue = pm->GetUnitlessParameter(fullModeParam);
            if (outSource) *outSource = "mode_params";
            return true;
        }
        if (pm->ParameterExists(fullLegacyParam)) {
            *outValue = pm->GetUnitlessParameter(fullLegacyParam);
            if (outSource) *outSource = "legacy";
            return true;
        }
        return false;
    }

    static G4bool ResolveDouble(TsParameterManager* pm,
                                const G4String& fullModeParam,
                                const G4String& fullLegacyParam,
                                const G4String& unitCategory,
                                G4double* outValue,
                                G4String* outSource)
    {
        if (pm->ParameterExists(fullModeParam)) {
            *outValue = pm->GetDoubleParameter(fullModeParam, unitCategory);
            if (outSource) *outSource = "mode_params";
            return true;
        }
        if (pm->ParameterExists(fullLegacyParam)) {
            *outValue = pm->GetDoubleParameter(fullLegacyParam, unitCategory);
            if (outSource) *outSource = "legacy";
            return true;
        }
        return false;
    }

    static G4bool ResolveString(TsParameterManager* pm,
                                const G4String& fullModeParam,
                                const G4String& fullLegacyParam,
                                G4String* outValue,
                                G4String* outSource)
    {
        if (pm->ParameterExists(fullModeParam)) {
            *outValue = pm->GetStringParameter(fullModeParam);
            if (outSource) *outSource = "mode_params";
            return true;
        }
        if (pm->ParameterExists(fullLegacyParam)) {
            *outValue = pm->GetStringParameter(fullLegacyParam);
            if (outSource) *outSource = "legacy";
            return true;
        }
        return false;
    }

    static G4bool ResolveBoolean(TsParameterManager* pm,
                                 const G4String& fullModeParam,
                                 const G4String& fullLegacyParam,
                                 G4bool* outValue,
                                 G4String* outSource)
    {
        if (pm->ParameterExists(fullModeParam)) {
            *outValue = pm->GetBooleanParameter(fullModeParam);
            if (outSource) *outSource = "mode_params";
            return true;
        }
        if (pm->ParameterExists(fullLegacyParam)) {
            *outValue = pm->GetBooleanParameter(fullLegacyParam);
            if (outSource) *outSource = "legacy";
            return true;
        }
        return false;
    }

    static G4bool ResolveUnitlessVector(TsParameterManager* pm,
                                        const G4String& fullModeParam,
                                        const G4String& fullLegacyParam,
                                        std::vector<G4double>* outValues,
                                        G4String* outSource)
    {
        if (pm->ParameterExists(fullModeParam)) {
            G4double* values = pm->GetUnitlessVector(fullModeParam);
            G4int size = pm->GetVectorLength(fullModeParam);
            outValues->assign(values, values + size);
            delete[] values;
            if (outSource) *outSource = "mode_params";
            return true;
        }
        if (pm->ParameterExists(fullLegacyParam)) {
            G4double* values = pm->GetUnitlessVector(fullLegacyParam);
            G4int size = pm->GetVectorLength(fullLegacyParam);
            outValues->assign(values, values + size);
            delete[] values;
            if (outSource) *outSource = "legacy";
            return true;
        }
        return false;
    }
};

#endif
