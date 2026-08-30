#include "cwpch.h"

#include "Crowny/Audio/EFXLoader.h"

#define CW_EFX_ENTRYPOINTS(X)                                                                                                                        \
    X(GenEffects, LPALGENEFFECTS)                                                                                                                    \
    X(DeleteEffects, LPALDELETEEFFECTS)                                                                                                              \
    X(IsEffect, LPALISEFFECT)                                                                                                                        \
    X(Effecti, LPALEFFECTI)                                                                                                                          \
    X(Effectiv, LPALEFFECTIV)                                                                                                                        \
    X(Effectf, LPALEFFECTF)                                                                                                                          \
    X(Effectfv, LPALEFFECTFV)                                                                                                                        \
    X(GetEffecti, LPALGETEFFECTI)                                                                                                                    \
    X(GetEffectf, LPALGETEFFECTF)                                                                                                                    \
    X(GenFilters, LPALGENFILTERS)                                                                                                                    \
    X(DeleteFilters, LPALDELETEFILTERS)                                                                                                              \
    X(IsFilter, LPALISFILTER)                                                                                                                        \
    X(Filteri, LPALFILTERI)                                                                                                                          \
    X(Filterf, LPALFILTERF)                                                                                                                          \
    X(GetFilteri, LPALGETFILTERI)                                                                                                                    \
    X(GetFilterf, LPALGETFILTERF)                                                                                                                    \
    X(GenAuxiliaryEffectSlots, LPALGENAUXILIARYEFFECTSLOTS)                                                                                          \
    X(DeleteAuxiliaryEffectSlots, LPALDELETEAUXILIARYEFFECTSLOTS)                                                                                    \
    X(IsAuxiliaryEffectSlot, LPALISAUXILIARYEFFECTSLOT)                                                                                              \
    X(AuxiliaryEffectSloti, LPALAUXILIARYEFFECTSLOTI)                                                                                                \
    X(AuxiliaryEffectSlotf, LPALAUXILIARYEFFECTSLOTF)

namespace Crowny
{
    namespace
    {
#define CW_COUNT_EFX_ENTRYPOINT(symbol, type) +1
        constexpr uint32_t EFXEntrypointCount = 0 CW_EFX_ENTRYPOINTS(CW_COUNT_EFX_ENTRYPOINT);
#undef CW_COUNT_EFX_ENTRYPOINT

        static_assert(EFXEntrypointCount == EFX::RequiredEntrypointCount);

        void* ResolveEFXEntrypoint(void* context, const char* name)
        {
            ALCdevice* device = static_cast<ALCdevice*>(context);
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

        if (!ResolveEntrypoints(ResolveEFXEntrypoint, device))
        {
            ClearOpenALErrors(device);
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
            Available = false;
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
                Available = false;
                HasEAXReverb = false;
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

    bool EFX::ResolveEntrypoints(EFXEntrypointResolver resolver, void* context)
    {
        Reset();

#define CW_RESOLVE_EFX(symbol, type) symbol = resolver != nullptr ? reinterpret_cast<type>(resolver(context, "al" #symbol)) : nullptr;

        CW_EFX_ENTRYPOINTS(CW_RESOLVE_EFX)

#undef CW_RESOLVE_EFX

        MissingEntrypoint = GetMissingRequiredEntrypoint();
        if (MissingEntrypoint == nullptr)
            return true;

        Status = EFXLoadStatus::MissingEntrypoint;
        return false;
    }

    void EFX::Reset() { *this = EFX{}; }

    EFXCapabilityState EFX::GetCapabilityState() const
    {
        uint32_t loadedEntrypoints = 0;

#define CW_COUNT_EFX(symbol, type) loadedEntrypoints += symbol != nullptr ? 1 : 0;

        CW_EFX_ENTRYPOINTS(CW_COUNT_EFX)

#undef CW_COUNT_EFX

        EFXEntrypointState entrypointState = EFXEntrypointState::Partial;
        if (loadedEntrypoints == 0)
            entrypointState = EFXEntrypointState::Empty;
        else if (loadedEntrypoints == RequiredEntrypointCount)
            entrypointState = EFXEntrypointState::Complete;

        return { Status, entrypointState, loadedEntrypoints, RequiredEntrypointCount, MaxAuxiliarySends, HasEAXReverb, Available, MissingEntrypoint };
    }

    const char* EFX::GetMissingRequiredEntrypoint() const
    {
#define CW_REQUIRE_EFX(symbol, type)                                                                                                                 \
    if (symbol == nullptr)                                                                                                                           \
        return "al" #symbol;

        CW_EFX_ENTRYPOINTS(CW_REQUIRE_EFX)

#undef CW_REQUIRE_EFX
        return nullptr;
    }

    const char* EFX::GetStatusName(EFXLoadStatus status)
    {
        switch (status)
        {
        case EFXLoadStatus::NotLoaded:
            return "not loaded";
        case EFXLoadStatus::Available:
            return "available";
        case EFXLoadStatus::NoDevice:
            return "no OpenAL device";
        case EFXLoadStatus::NoCurrentContext:
            return "the device context is not current";
        case EFXLoadStatus::ExtensionUnavailable:
            return "ALC_EXT_EFX is not advertised";
        case EFXLoadStatus::MissingEntrypoint:
            return "a required entrypoint is unavailable";
        case EFXLoadStatus::EffectCreationFailed:
            return "the driver could not create an effect object";
        default:
            return "unknown";
        }
    }

} // namespace Crowny

#undef CW_EFX_ENTRYPOINTS
