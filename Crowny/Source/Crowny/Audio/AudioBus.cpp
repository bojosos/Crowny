#include "cwpch.h"

#include "Crowny/Audio/AudioBus.h"
#include "Crowny/Audio/AudioManager.h"
#include "Crowny/Audio/AudioSource.h"

#include <AL/efx.h>

namespace Crowny
{

    AudioBus::AudioBus(const String& name, AudioBus* parent) : m_Name(name), m_Parent(parent)
    {
        if (m_Parent != nullptr)
            m_Parent->m_Children.push_back(this);

        if (const EFX* efx = AudioManager::TryGetEFX())
        {
            efx->GenAuxiliaryEffectSlots(1, &m_AuxSlot);
            if (alGetError() != AL_NO_ERROR)
            {
                CW_ENGINE_WARN("Failed to allocate aux effect slot for bus '{0}'.", name);
                m_AuxSlot = 0;
            }
        }
        m_EffectiveGain = ComputeChainGain();
    }

    AudioBus::~AudioBus()
    {
        if (m_Parent != nullptr)
        {
            auto& siblings = m_Parent->m_Children;
            siblings.erase(std::remove(siblings.begin(), siblings.end(), this), siblings.end());
        }
        if (const EFX* efx = AudioManager::TryGetEFX(); m_AuxSlot != 0 && efx != nullptr)
        {
            // Detach the effect from the slot so OpenAL releases its reference.
            efx->AuxiliaryEffectSloti(m_AuxSlot, AL_EFFECTSLOT_EFFECT, AL_EFFECT_NULL);
            efx->DeleteAuxiliaryEffectSlots(1, &m_AuxSlot);
            m_AuxSlot = 0;
        }
    }

    void AudioBus::SetVolume(float volume)
    {
        if (m_Volume == volume)
            return;
        m_Volume = std::max(0.0f, volume);
        RefreshGain();
    }

    void AudioBus::SetMuted(bool muted)
    {
        if (m_Muted == muted)
            return;
        m_Muted = muted;
        RefreshGain();
    }

    void AudioBus::SetSolo(bool solo)
    {
        m_Solo = solo;
        // Solo evaluation across siblings is the mixer's job — it will call SetSoloMute on every
        // bus and then RefreshGain itself.
    }

    void AudioBus::SetSoloMute(bool soloMute)
    {
        if (m_SoloMute == soloMute)
            return;
        m_SoloMute = soloMute;
        RefreshGain();
    }

    void AudioBus::AddEffect(const Ref<AudioEffect>& effect)
    {
        if (!effect)
            return;
        m_Effects.push_back(effect);
        if (m_Effects.size() == 1)
            RebindAuxSlot();
    }

    void AudioBus::RemoveEffect(size_t index)
    {
        if (index >= m_Effects.size())
            return;
        m_Effects.erase(m_Effects.begin() + index);
        if (index == 0)
            RebindAuxSlot();
    }

    void AudioBus::ReplaceEffect(size_t index, const Ref<AudioEffect>& effect)
    {
        if (index >= m_Effects.size())
            return;
        m_Effects[index] = effect;
        if (index == 0)
            RebindAuxSlot();
    }

    void AudioBus::RegisterSource(AudioSource* source) { m_Sources.insert(source); }

    void AudioBus::UnregisterSource(AudioSource* source) { m_Sources.erase(source); }

    void AudioBus::RefreshGain()
    {
        m_EffectiveGain = ComputeChainGain();
        for (AudioSource* source : m_Sources)
            source->RefreshEffectiveGain();
        for (AudioBus* child : m_Children)
            child->RefreshGain();
    }

    void AudioBus::RebindAuxSlot()
    {
        const EFX* efx = AudioManager::TryGetEFX();
        if (m_AuxSlot == 0 || efx == nullptr)
            return;
        ALuint effectId = AL_EFFECT_NULL;
        if (!m_Effects.empty() && m_Effects[0]->IsValid())
        {
            m_Effects[0]->Apply();
            effectId = m_Effects[0]->GetEffectId();
        }
        efx->AuxiliaryEffectSloti(m_AuxSlot, AL_EFFECTSLOT_EFFECT, static_cast<ALint>(effectId));
    }

    float AudioBus::ComputeChainGain() const
    {
        if (m_Muted || m_SoloMute)
            return 0.0f;
        float g = m_Volume;
        for (const AudioBus* p = m_Parent; p != nullptr; p = p->m_Parent)
        {
            if (p->m_Muted || p->m_SoloMute)
                return 0.0f;
            g *= p->m_Volume;
        }
        return g;
    }

} // namespace Crowny
