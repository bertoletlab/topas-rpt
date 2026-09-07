//
// Mode resolver scaffolding for radioactive source extensions
//

#ifndef TsModeFactory_hh
#define TsModeFactory_hh

#include <algorithm>
#include <cctype>
#include <string>

#include "globals.hh"

class TsModeFactory
{
public:
    enum class ModeId {
        Uniform,
        InVitroBind,
        ActivityMap,
        TIABinary,
        Diffusion,
        BioDist,
        Unknown
    };

    static G4String NormalizeModeName(const G4String& modeName)
    {
        std::string raw = modeName;
        std::transform(raw.begin(), raw.end(), raw.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        if (raw == "uniform")
            return "uniform";
        if (raw == "invitro_bind" || raw == "invitro" || raw == "bind")
            return "invitro_bind";
        if (raw == "activity_map" || raw == "activitymap")
            return "activity_map";
        if (raw == "tia_binary" || raw == "tiabinary")
            return "tia_binary";
        if (raw == "diffusion")
            return "diffusion";
        if (raw == "biodist" || raw == "biodistribution")
            return "biodist";
        return "unknown";
    }

    static ModeId ResolveMode(const G4String& modeName)
    {
        G4String normalized = NormalizeModeName(modeName);
        if (normalized == "uniform")
            return ModeId::Uniform;
        if (normalized == "invitro_bind")
            return ModeId::InVitroBind;
        if (normalized == "activity_map")
            return ModeId::ActivityMap;
        if (normalized == "tia_binary")
            return ModeId::TIABinary;
        if (normalized == "diffusion")
            return ModeId::Diffusion;
        if (normalized == "biodist")
            return ModeId::BioDist;
        return ModeId::Unknown;
    }
};

#endif
