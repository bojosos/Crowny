#pragma once

#include <AL/al.h>
#include <AL/alc.h>
#include <AL/efx.h>

#include <cstdint>

namespace Crowny
{
    enum class EFXLoadStatus : uint8_t
    {
        NotLoaded,
        Available,
        NoDevice,
        NoCurrentContext,
        ExtensionUnavailable,
        MissingEntrypoint,
        EffectCreationFailed,
    };

    enum class EFXEntrypointState : uint8_t
    {
        Empty,
        Partial,
        Complete,
    };

    struct EFXCapabilityState
    {
        EFXLoadStatus Status = EFXLoadStatus::NotLoaded;
        EFXEntrypointState Entrypoints = EFXEntrypointState::Empty;
        uint32_t LoadedEntrypoints = 0;
        uint32_t RequiredEntrypoints = 0;
        ALCint MaxAuxiliarySends = 0;
        bool SupportsEAXReverb = false;
        bool Available = false;
        const char* MissingEntrypoint = nullptr;
    };

    using EFXEntrypointResolver = void* (*)(void* context, const char* name);

    // Wrapper around the ALC_EXT_EFX extension entrypoints.
    // The struct holds function pointers loaded once at AudioManager startup.
    // If the device does not advertise EFX, m_Available is false and the rest of the audio system
    // collapses to a gain-only path (bus volumes still propagate, effects/filters become no-ops).
    struct EFX
    {
        static constexpr uint32_t RequiredEntrypointCount = 21;

        // Effect objects
        LPALGENEFFECTS GenEffects = nullptr;
        LPALDELETEEFFECTS DeleteEffects = nullptr;
        LPALISEFFECT IsEffect = nullptr;
        LPALEFFECTI Effecti = nullptr;
        LPALEFFECTIV Effectiv = nullptr;
        LPALEFFECTF Effectf = nullptr;
        LPALEFFECTFV Effectfv = nullptr;
        LPALGETEFFECTI GetEffecti = nullptr;
        LPALGETEFFECTF GetEffectf = nullptr;

        // Filter objects
        LPALGENFILTERS GenFilters = nullptr;
        LPALDELETEFILTERS DeleteFilters = nullptr;
        LPALISFILTER IsFilter = nullptr;
        LPALFILTERI Filteri = nullptr;
        LPALFILTERF Filterf = nullptr;
        LPALGETFILTERI GetFilteri = nullptr;
        LPALGETFILTERF GetFilterf = nullptr;

        // Auxiliary effect slots
        LPALGENAUXILIARYEFFECTSLOTS GenAuxiliaryEffectSlots = nullptr;
        LPALDELETEAUXILIARYEFFECTSLOTS DeleteAuxiliaryEffectSlots = nullptr;
        LPALISAUXILIARYEFFECTSLOT IsAuxiliaryEffectSlot = nullptr;
        LPALAUXILIARYEFFECTSLOTI AuxiliaryEffectSloti = nullptr;
        LPALAUXILIARYEFFECTSLOTF AuxiliaryEffectSlotf = nullptr;

        // True when ALC_EXT_EFX is present on the active device AND all entrypoints loaded.
        bool Available = false;

        // Maximum aux sends per source advertised by the context (typically 4).
        ALCint MaxAuxiliarySends = 0;

        // Whether ALC_EAXREVERB is supported. Falls back to AL_REVERB otherwise.
        bool HasEAXReverb = false;

        EFXLoadStatus Status = EFXLoadStatus::NotLoaded;
        const char* MissingEntrypoint = nullptr;

        bool Load(ALCdevice* device);
        // Resolves and classifies the function table without calling an entrypoint. A complete
        // table is not marked available until Load() also validates it against the active device.
        bool ResolveEntrypoints(EFXEntrypointResolver resolver, void* context = nullptr);
        void Reset();
        EFXCapabilityState GetCapabilityState() const;
        const char* GetMissingRequiredEntrypoint() const;
        static const char* GetStatusName(EFXLoadStatus status);
    };

} // namespace Crowny
