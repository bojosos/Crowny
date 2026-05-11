#include "cwpch.h"

#include "Crowny/Audio/AudioMixer.h"

namespace Crowny
{

    AudioMixer::AudioMixer()
    {
        // Default empty mixer has a single Master bus. Users add children via the mixer panel.
        AudioBusDesc master;
        master.Name = "Master";
        master.Parent = "";
        master.Volume = 1.0f;
        m_BusDescs.push_back(master);
    }

    // Builds the concrete AudioEffect instance for a bus from its descriptor. Returns nullptr when
    // the bus has no effect (or the requested type isn't implemented).
    static Ref<AudioEffect> CreateEffectFromDesc(const AudioBusDesc& desc)
    {
        switch (desc.FirstEffect)
        {
        case AudioEffectType::Reverb:
        {
            Ref<ReverbEffect> e = CreateRef<ReverbEffect>();
            e->Density = desc.Reverb.Density;
            e->Diffusion = desc.Reverb.Diffusion;
            e->Gain = desc.Reverb.Gain;
            e->GainHF = desc.Reverb.GainHF;
            e->DecayTime = desc.Reverb.DecayTime;
            e->DecayHFRatio = desc.Reverb.DecayHFRatio;
            e->ReflectionsGain = desc.Reverb.ReflectionsGain;
            e->ReflectionsDelay = desc.Reverb.ReflectionsDelay;
            e->LateReverbGain = desc.Reverb.LateReverbGain;
            e->LateReverbDelay = desc.Reverb.LateReverbDelay;
            e->AirAbsorptionGainHF = desc.Reverb.AirAbsorptionGainHF;
            e->RoomRolloffFactor = desc.Reverb.RoomRolloffFactor;
            return e;
        }
        case AudioEffectType::Echo:
        {
            Ref<EchoEffect> e = CreateRef<EchoEffect>();
            e->Delay = desc.Echo.Delay;
            e->LRDelay = desc.Echo.LRDelay;
            e->Damping = desc.Echo.Damping;
            e->Feedback = desc.Echo.Feedback;
            e->Spread = desc.Echo.Spread;
            return e;
        }
        case AudioEffectType::Distortion:
        {
            Ref<DistortionEffect> e = CreateRef<DistortionEffect>();
            e->Edge = desc.Distortion.Edge;
            e->Gain = desc.Distortion.Gain;
            e->LowpassCutoff = desc.Distortion.LowpassCutoff;
            e->EqCenter = desc.Distortion.EqCenter;
            e->EqBandwidth = desc.Distortion.EqBandwidth;
            return e;
        }
        case AudioEffectType::Chorus:
        {
            Ref<ChorusEffect> e = CreateRef<ChorusEffect>();
            e->Waveform = desc.Chorus.Waveform;
            e->Phase = desc.Chorus.Phase;
            e->Rate = desc.Chorus.Rate;
            e->Depth = desc.Chorus.Depth;
            e->Feedback = desc.Chorus.Feedback;
            e->Delay = desc.Chorus.Delay;
            return e;
        }
        case AudioEffectType::Equalizer:
        {
            Ref<EqualizerEffect> e = CreateRef<EqualizerEffect>();
            e->LowGain = desc.Equalizer.LowGain;
            e->LowCutoff = desc.Equalizer.LowCutoff;
            e->Mid1Gain = desc.Equalizer.Mid1Gain;
            e->Mid1Center = desc.Equalizer.Mid1Center;
            e->Mid1Width = desc.Equalizer.Mid1Width;
            e->Mid2Gain = desc.Equalizer.Mid2Gain;
            e->Mid2Center = desc.Equalizer.Mid2Center;
            e->Mid2Width = desc.Equalizer.Mid2Width;
            e->HighGain = desc.Equalizer.HighGain;
            e->HighCutoff = desc.Equalizer.HighCutoff;
            return e;
        }
        case AudioEffectType::PitchShifter:
        {
            Ref<PitchShifterEffect> e = CreateRef<PitchShifterEffect>();
            e->CoarseTune = desc.PitchShifter.CoarseTune;
            e->FineTune = desc.PitchShifter.FineTune;
            return e;
        }
        case AudioEffectType::Flanger:
        {
            Ref<FlangerEffect> e = CreateRef<FlangerEffect>();
            e->Waveform = desc.Flanger.Waveform;
            e->Phase = desc.Flanger.Phase;
            e->Rate = desc.Flanger.Rate;
            e->Depth = desc.Flanger.Depth;
            e->Feedback = desc.Flanger.Feedback;
            e->Delay = desc.Flanger.Delay;
            return e;
        }
        case AudioEffectType::Compressor:
        {
            Ref<CompressorEffect> e = CreateRef<CompressorEffect>();
            e->Enabled = desc.Compressor.Enabled;
            return e;
        }
        case AudioEffectType::RingModulator:
        {
            Ref<RingModulatorEffect> e = CreateRef<RingModulatorEffect>();
            e->Frequency = desc.RingModulator.Frequency;
            e->HighpassCutoff = desc.RingModulator.HighpassCutoff;
            e->Waveform = desc.RingModulator.Waveform;
            return e;
        }
        default:
            return nullptr;
        }
    }

    // Pushes design-time desc values into an already-allocated runtime effect. The runtime effect's
    // type must match desc.FirstEffect (the caller is responsible for rebuilding the bus chain
    // when types diverge — Init() handles that).
    static void UpdateEffectFromDesc(AudioEffect& effect, const AudioBusDesc& desc)
    {
        switch (desc.FirstEffect)
        {
        case AudioEffectType::Reverb:
        {
            ReverbEffect& e = static_cast<ReverbEffect&>(effect);
            e.Density = desc.Reverb.Density;
            e.Diffusion = desc.Reverb.Diffusion;
            e.Gain = desc.Reverb.Gain;
            e.GainHF = desc.Reverb.GainHF;
            e.DecayTime = desc.Reverb.DecayTime;
            e.DecayHFRatio = desc.Reverb.DecayHFRatio;
            e.ReflectionsGain = desc.Reverb.ReflectionsGain;
            e.ReflectionsDelay = desc.Reverb.ReflectionsDelay;
            e.LateReverbGain = desc.Reverb.LateReverbGain;
            e.LateReverbDelay = desc.Reverb.LateReverbDelay;
            e.AirAbsorptionGainHF = desc.Reverb.AirAbsorptionGainHF;
            e.RoomRolloffFactor = desc.Reverb.RoomRolloffFactor;
            break;
        }
        case AudioEffectType::Echo:
        {
            EchoEffect& e = static_cast<EchoEffect&>(effect);
            e.Delay = desc.Echo.Delay;
            e.LRDelay = desc.Echo.LRDelay;
            e.Damping = desc.Echo.Damping;
            e.Feedback = desc.Echo.Feedback;
            e.Spread = desc.Echo.Spread;
            break;
        }
        case AudioEffectType::Distortion:
        {
            DistortionEffect& e = static_cast<DistortionEffect&>(effect);
            e.Edge = desc.Distortion.Edge;
            e.Gain = desc.Distortion.Gain;
            e.LowpassCutoff = desc.Distortion.LowpassCutoff;
            e.EqCenter = desc.Distortion.EqCenter;
            e.EqBandwidth = desc.Distortion.EqBandwidth;
            break;
        }
        case AudioEffectType::Chorus:
        {
            ChorusEffect& e = static_cast<ChorusEffect&>(effect);
            e.Waveform = desc.Chorus.Waveform;
            e.Phase = desc.Chorus.Phase;
            e.Rate = desc.Chorus.Rate;
            e.Depth = desc.Chorus.Depth;
            e.Feedback = desc.Chorus.Feedback;
            e.Delay = desc.Chorus.Delay;
            break;
        }
        case AudioEffectType::Equalizer:
        {
            EqualizerEffect& e = static_cast<EqualizerEffect&>(effect);
            e.LowGain = desc.Equalizer.LowGain;
            e.LowCutoff = desc.Equalizer.LowCutoff;
            e.Mid1Gain = desc.Equalizer.Mid1Gain;
            e.Mid1Center = desc.Equalizer.Mid1Center;
            e.Mid1Width = desc.Equalizer.Mid1Width;
            e.Mid2Gain = desc.Equalizer.Mid2Gain;
            e.Mid2Center = desc.Equalizer.Mid2Center;
            e.Mid2Width = desc.Equalizer.Mid2Width;
            e.HighGain = desc.Equalizer.HighGain;
            e.HighCutoff = desc.Equalizer.HighCutoff;
            break;
        }
        case AudioEffectType::PitchShifter:
        {
            PitchShifterEffect& e = static_cast<PitchShifterEffect&>(effect);
            e.CoarseTune = desc.PitchShifter.CoarseTune;
            e.FineTune = desc.PitchShifter.FineTune;
            break;
        }
        case AudioEffectType::Flanger:
        {
            FlangerEffect& e = static_cast<FlangerEffect&>(effect);
            e.Waveform = desc.Flanger.Waveform;
            e.Phase = desc.Flanger.Phase;
            e.Rate = desc.Flanger.Rate;
            e.Depth = desc.Flanger.Depth;
            e.Feedback = desc.Flanger.Feedback;
            e.Delay = desc.Flanger.Delay;
            break;
        }
        case AudioEffectType::Compressor:
        {
            CompressorEffect& e = static_cast<CompressorEffect&>(effect);
            e.Enabled = desc.Compressor.Enabled;
            break;
        }
        case AudioEffectType::RingModulator:
        {
            RingModulatorEffect& e = static_cast<RingModulatorEffect&>(effect);
            e.Frequency = desc.RingModulator.Frequency;
            e.HighpassCutoff = desc.RingModulator.HighpassCutoff;
            e.Waveform = desc.RingModulator.Waveform;
            break;
        }
        default:
            break;
        }
    }

    void AudioMixer::Init()
    {
        m_Buses.clear();
        m_MasterBus.Reset();

        if (m_BusDescs.empty())
            return;

        // First pass: build the bus objects with parent pointers wired up. We assume parents appear
        // before children in m_BusDescs — the editor preserves that ordering on add/remove.
        UnorderedMap<String, Ref<AudioBus>> byName;
        for (const AudioBusDesc& desc : m_BusDescs)
        {
            AudioBus* parent = nullptr;
            if (!desc.Parent.empty())
            {
                const auto it = byName.find(desc.Parent);
                if (it != byName.end())
                    parent = it->second.Get();
            }
            Ref<AudioBus> bus = CreateRef<AudioBus>(desc.Name, parent);
            bus->SetVolume(desc.Volume);
            bus->SetMuted(desc.Muted);

            if (Ref<AudioEffect> effect = CreateEffectFromDesc(desc))
                bus->AddEffect(effect);

            byName[desc.Name] = bus;
            m_Buses.push_back(bus);
            if (m_MasterBus == nullptr)
                m_MasterBus = bus;
        }
    }

    Ref<AudioBus> AudioMixer::FindBus(const String& name) const
    {
        for (const Ref<AudioBus>& bus : m_Buses)
            if (bus->GetName() == name)
                return bus;
        return nullptr;
    }

    void AudioMixer::RefreshSolo()
    {
        // Determine whether any bus in the tree is soloed. If so, every non-soloed bus that is not
        // an ancestor of a soloed bus is silenced via SetSoloMute(true).
        bool anySolo = false;
        for (const Ref<AudioBus>& bus : m_Buses)
            if (bus->IsSolo()) { anySolo = true; break; }

        for (const Ref<AudioBus>& bus : m_Buses)
        {
            if (!anySolo)
            {
                bus->SetSoloMute(false);
                continue;
            }

            // A bus is audible if it is soloed itself OR has a soloed ancestor OR has a soloed descendant.
            bool audible = bus->IsSolo();
            for (AudioBus* p = bus->GetParent(); p != nullptr && !audible; p = p->GetParent())
                if (p->IsSolo())
                    audible = true;
            if (!audible)
            {
                for (const Ref<AudioBus>& other : m_Buses)
                {
                    if (!other->IsSolo())
                        continue;
                    for (AudioBus* p = other->GetParent(); p != nullptr; p = p->GetParent())
                    {
                        if (p == bus.Get())
                        {
                            audible = true;
                            break;
                        }
                    }
                    if (audible)
                        break;
                }
            }
            bus->SetSoloMute(!audible);
        }
    }

    void AudioMixer::SyncRuntimeFromDescs()
    {
        for (const AudioBusDesc& desc : m_BusDescs)
        {
            Ref<AudioBus> bus = FindBus(desc.Name);
            if (!bus)
                continue;
            bus->SetVolume(desc.Volume);
            bus->SetMuted(desc.Muted);

            if (desc.FirstEffect == AudioEffectType::None || bus->GetEffects().empty())
                continue;

            Ref<AudioEffect> effect = bus->GetEffects().front();
            if (effect->GetType() != desc.FirstEffect)
                continue;
            UpdateEffectFromDesc(*effect, desc);
            effect->Apply();
            // Re-bind so OpenAL's aux slot picks up the new parameters.
            bus->ReplaceEffect(0, effect);
        }
    }

} // namespace Crowny
