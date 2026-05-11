#pragma once

#include "Crowny/Audio/AudioEffect.h"
#include "Crowny/Common/Common.h"
#include "Crowny/Common/RefCounted.h"

#include <AL/al.h>

namespace Crowny
{

    class AudioSource;

    // A runtime audio bus. Buses form a tree rooted at the mixer's master bus. Each bus owns one
    // OpenAL auxiliary effect slot when EFX is available; that slot holds the first effect on the
    // bus's effect chain. Sources route to a bus via SetBus() — they pick up the bus chain's aux
    // slots as their AL_AUXILIARY_SEND_FILTER targets.
    //
    // The serial effect chain currently maps to a single aux slot (the first effect). The remaining
    // effects are reserved for follow-up work that will daisy-chain them via additional slots.
    class AudioBus : public RefCounted
    {
    public:
        AudioBus(const String& name, AudioBus* parent = nullptr);
        ~AudioBus();

        const String& GetName() const { return m_Name; }
        AudioBus* GetParent() const { return m_Parent; }

        // Multiplicative volume in [0, +inf). The effective gain a source sees is the product of
        // its own volume and the volume of every bus from this bus up to the master, with any
        // muted/solo'd bus contributing 0.
        float GetVolume() const { return m_Volume; }
        void SetVolume(float volume);

        bool IsMuted() const { return m_Muted; }
        void SetMuted(bool muted);

        bool IsSolo() const { return m_Solo; }
        void SetSolo(bool solo);

        // Cached effective gain — combines volume + mute/solo of this bus and all ancestors.
        float GetEffectiveGain() const { return m_EffectiveGain; }

        // Effects (serial chain). Only the first effect is wired to the aux slot in this pass;
        // the rest are stored but inactive until follow-up work adds multi-slot chaining.
        const Vector<Ref<AudioEffect>>& GetEffects() const { return m_Effects; }
        void AddEffect(const Ref<AudioEffect>& effect);
        void RemoveEffect(size_t index);
        void ReplaceEffect(size_t index, const Ref<AudioEffect>& effect);

        // OpenAL aux slot id used for source send routing. Returns 0 when EFX is unavailable.
        ALuint GetAuxSlot() const { return m_AuxSlot; }

        void RegisterSource(AudioSource* source);
        void UnregisterSource(AudioSource* source);

        // Recomputes m_EffectiveGain from the volume/mute/solo chain and pushes the new gain to
        // every routed source. Called automatically on volume/mute/solo change.
        void RefreshGain();

        // Called by AudioMixer when solo state somewhere in the tree changed — re-evaluates whether
        // *this* bus is muted by a solo'd sibling.
        void SetSoloMute(bool soloMute);

    private:
        void RebindAuxSlot();
        float ComputeChainGain() const;

        String m_Name;
        AudioBus* m_Parent = nullptr;
        Vector<AudioBus*> m_Children;
        float m_Volume = 1.0f;
        bool m_Muted = false;
        bool m_Solo = false;
        bool m_SoloMute = false; // muted because some other bus is soloed
        float m_EffectiveGain = 1.0f;

        Vector<Ref<AudioEffect>> m_Effects;
        ALuint m_AuxSlot = 0;

        Set<AudioSource*> m_Sources;

        friend class AudioMixer;
    };

} // namespace Crowny
