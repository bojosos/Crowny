#include "cwpch.h"

#include "Crowny/Audio/EFXLoader.h"

namespace Crowny
{
    namespace
    {
        void* ResolveEFXEntrypoint(ALCdevice* device, const char* name)
        {
            if (void* entrypoint = alGetProcAddress(name))
                return entrypoint;

            // Some OpenAL routers expose ALC_EXT_EFX's AL entrypoints only through the device
            // resolver. OpenAL Soft accepts either resolver, so this remains portable.
            return device != nullptr ? alcGetProcAddress(device, name) : nullptr;
        }

        void ClearOpenALErrors(ALCdevice* device)
        {
            while (alGetError() != AL_NO_ERROR)
            {
            }
            if (device != nullptr)
                alcGetError(device);
        }
    } // namespace

    bool EFX::Load(ALCdevice* device)
    {
        Reset();

        if (device == nullptr)
        {
            Status = EFXLoadStatus::NoDevice;
            return false;
        }

        ALCcontext* currentContext = alcGetCurrentContext();
        if (currentContext == nullptr || alcGetContextsDevice(currentContext) != device)
        {
            Status = EFXLoadStatus::NoCurrentContext;
            return false;
        }

        if (alcIsExtensionPresent(device, "ALC_EXT_EFX") == ALC_FALSE)
        {
            Status = EFXLoadStatus::ExtensionUnavailable;
            return false;
        }

        ClearOpenALErrors(device);

#define CW_LOAD_EFX(symbol, type) symbol = reinterpret_cast<type>(ResolveEFXEntrypoint(device, "al" #symbol))

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

        if (const char* missing = GetMissingRequiredEntrypoint())
        {
            ClearOpenALErrors(device);
            Reset();
            Status = EFXLoadStatus::MissingEntrypoint;
            MissingEntrypoint = missing;
            return false;
        }

        alcGetIntegerv(device, ALC_MAX_AUXILIARY_SENDS, 1, &MaxAuxiliarySends);
        if (alcGetError(device) != ALC_NO_ERROR)
            MaxAuxiliarySends = 0;

        ClearOpenALErrors(device);

        ALuint probe = 0;
        GenEffects(1, &probe);
        if (probe == 0 || alGetError() != AL_NO_ERROR)
        {
            if (probe != 0)
                DeleteEffects(1, &probe);
            ClearOpenALErrors(device);
            Reset();
            Status = EFXLoadStatus::EffectCreationFailed;
            return false;
        }

        Effecti(probe, AL_EFFECT_TYPE, AL_EFFECT_EAXREVERB);
        HasEAXReverb = alGetError() == AL_NO_ERROR;
        if (!HasEAXReverb)
        {
            Effecti(probe, AL_EFFECT_TYPE, AL_EFFECT_REVERB);
            if (alGetError() != AL_NO_ERROR)
            {
                DeleteEffects(1, &probe);
                ClearOpenALErrors(device);
                Reset();
                Status = EFXLoadStatus::EffectCreationFailed;
                return false;
            }
        }
        DeleteEffects(1, &probe);
        ClearOpenALErrors(device);

        Available = true;
        Status = EFXLoadStatus::Available;
        return true;
    }

    void EFX::Reset() { *this = EFX{}; }

    const char* EFX::GetMissingRequiredEntrypoint() const
    {
#define CW_REQUIRE_EFX(symbol)                                                                                                                       \
    if (symbol == nullptr)                                                                                                                           \
        return "al" #symbol

        CW_REQUIRE_EFX(GenEffects);
        CW_REQUIRE_EFX(DeleteEffects);
        CW_REQUIRE_EFX(IsEffect);
        CW_REQUIRE_EFX(Effecti);
        CW_REQUIRE_EFX(Effectiv);
        CW_REQUIRE_EFX(Effectf);
        CW_REQUIRE_EFX(Effectfv);
        CW_REQUIRE_EFX(GetEffecti);
        CW_REQUIRE_EFX(GetEffectf);
        CW_REQUIRE_EFX(GenFilters);
        CW_REQUIRE_EFX(DeleteFilters);
        CW_REQUIRE_EFX(IsFilter);
        CW_REQUIRE_EFX(Filteri);
        CW_REQUIRE_EFX(Filterf);
        CW_REQUIRE_EFX(GetFilteri);
        CW_REQUIRE_EFX(GetFilterf);
        CW_REQUIRE_EFX(GenAuxiliaryEffectSlots);
        CW_REQUIRE_EFX(DeleteAuxiliaryEffectSlots);
        CW_REQUIRE_EFX(IsAuxiliaryEffectSlot);
        CW_REQUIRE_EFX(AuxiliaryEffectSloti);
        CW_REQUIRE_EFX(AuxiliaryEffectSlotf);

#undef CW_REQUIRE_EFX
        return nullptr;
    }

    const char* EFX::GetStatusName(EFXLoadStatus status)
    {
        switch (status)
        {
        case EFXLoadStatus::NotLoaded: return "not loaded";
        case EFXLoadStatus::Available: return "available";
        case EFXLoadStatus::NoDevice: return "no OpenAL device";
        case EFXLoadStatus::NoCurrentContext: return "the device context is not current";
        case EFXLoadStatus::ExtensionUnavailable: return "ALC_EXT_EFX is not advertised";
        case EFXLoadStatus::MissingEntrypoint: return "a required entrypoint is unavailable";
        case EFXLoadStatus::EffectCreationFailed: return "the driver could not create an effect object";
        default: return "unknown";
        }
    }

} // namespace Crowny
