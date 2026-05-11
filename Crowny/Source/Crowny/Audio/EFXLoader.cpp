#include "cwpch.h"

#include "Crowny/Audio/EFXLoader.h"

#include <AL/efx-presets.h>

namespace Crowny
{

    bool EFX::Load(ALCdevice* device)
    {
        Reset();

        if (device == nullptr)
            return false;

        if (alcIsExtensionPresent(device, "ALC_EXT_EFX") == ALC_FALSE)
        {
            CW_ENGINE_WARN("ALC_EXT_EFX not present on this device; audio effects disabled.");
            return false;
        }

#define CW_LOAD_EFX(symbol, type)                                                                                      \
    symbol = reinterpret_cast<type>(alGetProcAddress(#symbol));                                                        \
    if (symbol == nullptr)                                                                                             \
    {                                                                                                                  \
        CW_ENGINE_WARN("Failed to load EFX entrypoint {0}; audio effects disabled.", #symbol);                         \
        Reset();                                                                                                       \
        return false;                                                                                                  \
    }

        CW_LOAD_EFX(GenEffects, LPALGENEFFECTS);
        CW_LOAD_EFX(DeleteEffects, LPALDELETEEFFECTS);
        CW_LOAD_EFX(IsEffect, LPALISEFFECT);
        CW_LOAD_EFX(Effecti, LPALEFFECTI);
        CW_LOAD_EFX(Effectiv, LPALEFFECTIV);
        CW_LOAD_EFX(Effectf, LPALEFFECTF);
        CW_LOAD_EFX(Effectfv, LPALEFFECTFV);
        CW_LOAD_EFX(GetEffecti, LPALGETEFFECTI);
        CW_LOAD_EFX(GetEffectf, LPALGETEFFECTF);

        CW_LOAD_EFX(GenFilters, LPALGENFILTERS);
        CW_LOAD_EFX(DeleteFilters, LPALDELETEFILTERS);
        CW_LOAD_EFX(IsFilter, LPALISFILTER);
        CW_LOAD_EFX(Filteri, LPALFILTERI);
        CW_LOAD_EFX(Filterf, LPALFILTERF);
        CW_LOAD_EFX(GetFilteri, LPALGETFILTERI);
        CW_LOAD_EFX(GetFilterf, LPALGETFILTERF);

        CW_LOAD_EFX(GenAuxiliaryEffectSlots, LPALGENAUXILIARYEFFECTSLOTS);
        CW_LOAD_EFX(DeleteAuxiliaryEffectSlots, LPALDELETEAUXILIARYEFFECTSLOTS);
        CW_LOAD_EFX(IsAuxiliaryEffectSlot, LPALISAUXILIARYEFFECTSLOT);
        CW_LOAD_EFX(AuxiliaryEffectSloti, LPALAUXILIARYEFFECTSLOTI);
        CW_LOAD_EFX(AuxiliaryEffectSlotf, LPALAUXILIARYEFFECTSLOTF);

#undef CW_LOAD_EFX

        alcGetIntegerv(device, ALC_MAX_AUXILIARY_SENDS, 1, &MaxAuxiliarySends);

        // Probe EAXREVERB support by trying to set the effect type.
        ALuint probe = 0;
        GenEffects(1, &probe);
        Effecti(probe, AL_EFFECT_TYPE, AL_EFFECT_EAXREVERB);
        HasEAXReverb = (alGetError() == AL_NO_ERROR);
        DeleteEffects(1, &probe);
        // Clear any error from the probe.
        alGetError();

        Available = true;
        CW_ENGINE_INFO("EFX initialised — max aux sends: {0}, EAX reverb: {1}", MaxAuxiliarySends, HasEAXReverb);
        return true;
    }

    void EFX::Reset()
    {
        std::memset(this, 0, sizeof(EFX));
    }

} // namespace Crowny
