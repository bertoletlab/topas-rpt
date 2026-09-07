//
// Shared mode resolution helpers for radioactive source generators
//

#ifndef TsSourceModeResolver_hh
#define TsSourceModeResolver_hh

#include "TsModeFactory.hh"
#include "TsParameterManager.hh"

class TsSourceModeResolver
{
public:
    struct Resolution
    {
        G4String mode;
        G4bool modeSetExplicitly;
        G4bool usedLegacyTypeMapping;
        G4String legacyTypeValue;

        Resolution()
            : mode("uniform"), modeSetExplicitly(false), usedLegacyTypeMapping(false), legacyTypeValue("")
        {
        }
    };

    static G4String InferModeFromSourceType(const G4String& sourceType)
    {
        if (sourceType == "RadioactiveTimeSourceBind")
            return "invitro_bind";
        if (sourceType == "RadioactiveActivityMap" || sourceType == "RadioactiveActivityMapSource")
            return "activity_map";
        if (sourceType == "RadioactiveDiffusion")
            return "diffusion";
        if (sourceType == "BioDistSource")
            return "biodist";
        return "uniform";
    }

    static Resolution ResolveRequestedMode(TsParameterManager* pM, const G4String& modeParmPath,
                                           const G4String& typeParmPath)
    {
        Resolution result;

        if (pM->ParameterExists(modeParmPath)) {
            result.mode = pM->GetStringParameter(modeParmPath);
            result.modeSetExplicitly = true;
        } else if (pM->ParameterExists(typeParmPath)) {
            result.legacyTypeValue = pM->GetStringParameter(typeParmPath);
            result.mode = InferModeFromSourceType(result.legacyTypeValue);
            result.usedLegacyTypeMapping = true;
        }

        result.mode = TsModeFactory::NormalizeModeName(result.mode);
        return result;
    }
};

#endif
