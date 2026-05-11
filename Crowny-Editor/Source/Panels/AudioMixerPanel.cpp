#include "cwepch.h"

#include "Panels/AudioMixerPanel.h"
#include "UI/Properties.h"
#include "UI/UIUtils.h"

#include "Crowny/Assets/AssetManager.h"
#include "Crowny/Audio/AudioBus.h"
#include "Crowny/Audio/AudioEffect.h"
#include "Crowny/Audio/AudioManager.h"

#include <imgui.h>

namespace Crowny
{

    AudioMixerPanel::AudioMixerPanel(const String& name) : ImGuiPanel(name) {}

    void AudioMixerPanel::Render()
    {
        BeginPanel();

        // Allow setting an audio mixer asset as active.
        AssetHandle<AudioMixer> handle = gAudioManager->GetActiveMixer();
        if (UIUtils::AssetReference<AudioMixer>("Active Mixer", handle))
        {
            gAudioManager->SetActiveMixer(handle);
        }

        if (!handle)
        {
            ImGui::TextDisabled("No mixer set. Assign an AudioMixer asset above to begin.");
            EndPanel();
            return;
        }

        AudioMixer& mixer = *handle;
        Vector<AudioBusDesc>& descs = mixer.GetBusDescs();

        ImGui::Separator();

        // "Add Bus" — adds a new child of the master bus. We keep the parent-before-child ordering
        // invariant by always appending to the end.
        if (ImGui::Button("Add Bus") && !descs.empty())
        {
            AudioBusDesc desc;
            desc.Name = "Bus" + std::to_string(descs.size());
            desc.Parent = descs.front().Name;
            descs.push_back(desc);
            mixer.Init();
        }

        ImGui::Separator();

        for (size_t i = 0; i < descs.size(); i++)
            RenderBus(mixer, i);

        EndPanel();
    }

    void AudioMixerPanel::RenderBus(AudioMixer& mixer, size_t descIndex)
    {
        Vector<AudioBusDesc>& descs = mixer.GetBusDescs();
        AudioBusDesc& desc = descs[descIndex];

        ImGui::PushID((int)descIndex);

        // Master gets a distinctive header; children are nested under their parent visually.
        const bool isMaster = descIndex == 0;
        const String header = isMaster ? ("[Master] " + desc.Name) : (desc.Parent + " / " + desc.Name);
        if (ImGui::CollapsingHeader(header.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
        {
            if (!isMaster)
            {
                String name = desc.Name;
                if (UI::Property("Name", name))
                {
                    // Rename also fixes up any child's parent pointer.
                    const String oldName = desc.Name;
                    desc.Name = name;
                    for (AudioBusDesc& other : descs)
                        if (other.Parent == oldName)
                            other.Parent = name;
                    mixer.Init();
                }

                // Parent picker — every bus except the renamed-into-itself one.
                Vector<String> parents;
                for (size_t i = 0; i < descs.size(); i++)
                    if (i != descIndex)
                        parents.push_back(descs[i].Name);
                int parentIdx = 0;
                for (int i = 0; i < (int)parents.size(); i++)
                    if (parents[i] == desc.Parent) { parentIdx = i; break; }
                if (UI::PropertyDropdown("Parent", parents, parentIdx))
                {
                    desc.Parent = parents[parentIdx];
                    mixer.Init();
                }
            }

            float volume = desc.Volume;
            if (UI::PropertySlider("Volume", volume, 0.0f, 2.0f))
            {
                desc.Volume = volume;
                mixer.SyncRuntimeFromDescs();
            }

            bool muted = desc.Muted;
            if (UI::Property("Muted", muted))
            {
                desc.Muted = muted;
                mixer.SyncRuntimeFromDescs();
            }

            Vector<String> effectNames = {
                "None", "Reverb", "Echo", "Distortion", "Chorus",
                "Equalizer", "Pitch Shifter", "Flanger", "Compressor", "Ring Modulator"
            };
            int effectIdx = (int)desc.FirstEffect;
            if (effectIdx >= (int)effectNames.size())
                effectIdx = 0;
            if (UI::PropertyDropdown("Effect", effectNames, effectIdx))
            {
                desc.FirstEffect = (AudioEffectType)effectIdx;
                mixer.Init();
            }

            RenderEffectControls(desc);

            if (!isMaster && ImGui::Button("Remove Bus"))
            {
                // Reparent children to the master if their parent disappears.
                const String removedName = desc.Name;
                descs.erase(descs.begin() + descIndex);
                for (AudioBusDesc& other : descs)
                    if (other.Parent == removedName)
                        other.Parent = descs.front().Name;
                mixer.Init();
                ImGui::PopID();
                return;
            }
        }

        ImGui::PopID();
    }

    void AudioMixerPanel::RenderEffectControls(AudioBusDesc& desc)
    {
        switch (desc.FirstEffect)
        {
        case AudioEffectType::Reverb: RenderReverbControls(desc); break;
        case AudioEffectType::Echo: RenderEchoControls(desc); break;
        case AudioEffectType::Distortion: RenderDistortionControls(desc); break;
        case AudioEffectType::Chorus: RenderChorusControls(desc); break;
        case AudioEffectType::Equalizer: RenderEqualizerControls(desc); break;
        case AudioEffectType::PitchShifter: RenderPitchShifterControls(desc); break;
        case AudioEffectType::Flanger: RenderFlangerControls(desc); break;
        case AudioEffectType::Compressor: RenderCompressorControls(desc); break;
        case AudioEffectType::RingModulator: RenderRingModulatorControls(desc); break;
        default: break;
        }
    }

    // Pushes desc changes into the active mixer's runtime buses so parameter tweaks are audible
    // without rebuilding the bus tree.
    static void SyncActiveMixer()
    {
        if (AssetHandle<AudioMixer> mixer = gAudioManager->GetActiveMixer())
            mixer->SyncRuntimeFromDescs();
    }

    void AudioMixerPanel::RenderReverbControls(AudioBusDesc& desc)
    {
        // Preset buttons — applying a preset overwrites all reverb parameters.
        for (uint8_t i = 0; i < (uint8_t)ReverbEffect::Preset::Count; i++)
        {
            ReverbEffect::Preset preset = (ReverbEffect::Preset)i;
            if (i > 0)
                ImGui::SameLine();
            if (ImGui::Button(ReverbEffect::GetPresetName(preset)))
            {
                ReverbEffect tmp;
                tmp.ApplyPreset(preset);
                desc.Reverb.Density = tmp.Density;
                desc.Reverb.Diffusion = tmp.Diffusion;
                desc.Reverb.Gain = tmp.Gain;
                desc.Reverb.GainHF = tmp.GainHF;
                desc.Reverb.DecayTime = tmp.DecayTime;
                desc.Reverb.DecayHFRatio = tmp.DecayHFRatio;
                desc.Reverb.ReflectionsGain = tmp.ReflectionsGain;
                desc.Reverb.ReflectionsDelay = tmp.ReflectionsDelay;
                desc.Reverb.LateReverbGain = tmp.LateReverbGain;
                desc.Reverb.LateReverbDelay = tmp.LateReverbDelay;
                desc.Reverb.AirAbsorptionGainHF = tmp.AirAbsorptionGainHF;
                desc.Reverb.RoomRolloffFactor = tmp.RoomRolloffFactor;
                SyncActiveMixer();
            }
        }

        bool changed = false;
        changed |= UI::PropertySlider("Density", desc.Reverb.Density, 0.0f, 1.0f);
        changed |= UI::PropertySlider("Diffusion", desc.Reverb.Diffusion, 0.0f, 1.0f);
        changed |= UI::PropertySlider("Gain", desc.Reverb.Gain, 0.0f, 1.0f);
        changed |= UI::PropertySlider("Gain HF", desc.Reverb.GainHF, 0.0f, 1.0f);
        changed |= UI::PropertySlider("Decay Time", desc.Reverb.DecayTime, 0.1f, 20.0f);
        changed |= UI::PropertySlider("Decay HF Ratio", desc.Reverb.DecayHFRatio, 0.1f, 2.0f);
        changed |= UI::PropertySlider("Reflections Gain", desc.Reverb.ReflectionsGain, 0.0f, 3.16f);
        changed |= UI::PropertySlider("Reflections Delay", desc.Reverb.ReflectionsDelay, 0.0f, 0.3f);
        changed |= UI::PropertySlider("Late Reverb Gain", desc.Reverb.LateReverbGain, 0.0f, 10.0f);
        changed |= UI::PropertySlider("Late Reverb Delay", desc.Reverb.LateReverbDelay, 0.0f, 0.1f);

        if (changed)
            SyncActiveMixer();
    }

    void AudioMixerPanel::RenderEchoControls(AudioBusDesc& desc)
    {
        bool changed = false;
        changed |= UI::PropertySlider("Delay", desc.Echo.Delay, 0.0f, 0.207f);
        changed |= UI::PropertySlider("LR Delay", desc.Echo.LRDelay, 0.0f, 0.404f);
        changed |= UI::PropertySlider("Damping", desc.Echo.Damping, 0.0f, 0.99f);
        changed |= UI::PropertySlider("Feedback", desc.Echo.Feedback, 0.0f, 1.0f);
        changed |= UI::PropertySlider("Spread", desc.Echo.Spread, -1.0f, 1.0f);
        if (changed)
            SyncActiveMixer();
    }

    void AudioMixerPanel::RenderDistortionControls(AudioBusDesc& desc)
    {
        bool changed = false;
        changed |= UI::PropertySlider("Edge", desc.Distortion.Edge, 0.0f, 1.0f);
        changed |= UI::PropertySlider("Gain", desc.Distortion.Gain, 0.01f, 1.0f);
        changed |= UI::PropertySlider("Lowpass Cutoff", desc.Distortion.LowpassCutoff, 80.0f, 24000.0f);
        changed |= UI::PropertySlider("EQ Center", desc.Distortion.EqCenter, 80.0f, 24000.0f);
        changed |= UI::PropertySlider("EQ Bandwidth", desc.Distortion.EqBandwidth, 80.0f, 24000.0f);
        if (changed)
            SyncActiveMixer();
    }

    void AudioMixerPanel::RenderChorusControls(AudioBusDesc& desc)
    {
        bool changed = false;
        Vector<String> waveforms = { "Sinusoid", "Triangle" };
        int waveform = desc.Chorus.Waveform;
        if (UI::PropertyDropdown("Waveform", waveforms, waveform))
        {
            desc.Chorus.Waveform = waveform;
            changed = true;
        }
        changed |= UI::PropertySlider("Phase", desc.Chorus.Phase, -180, 180);
        changed |= UI::PropertySlider("Rate", desc.Chorus.Rate, 0.0f, 10.0f);
        changed |= UI::PropertySlider("Depth", desc.Chorus.Depth, 0.0f, 1.0f);
        changed |= UI::PropertySlider("Feedback", desc.Chorus.Feedback, -1.0f, 1.0f);
        changed |= UI::PropertySlider("Delay", desc.Chorus.Delay, 0.0f, 0.016f);
        if (changed)
            SyncActiveMixer();
    }

    void AudioMixerPanel::RenderEqualizerControls(AudioBusDesc& desc)
    {
        bool changed = false;
        changed |= UI::PropertySlider("Low Gain", desc.Equalizer.LowGain, 0.126f, 7.943f);
        changed |= UI::PropertySlider("Low Cutoff", desc.Equalizer.LowCutoff, 50.0f, 800.0f);
        changed |= UI::PropertySlider("Mid1 Gain", desc.Equalizer.Mid1Gain, 0.126f, 7.943f);
        changed |= UI::PropertySlider("Mid1 Center", desc.Equalizer.Mid1Center, 200.0f, 3000.0f);
        changed |= UI::PropertySlider("Mid1 Width", desc.Equalizer.Mid1Width, 0.01f, 1.0f);
        changed |= UI::PropertySlider("Mid2 Gain", desc.Equalizer.Mid2Gain, 0.126f, 7.943f);
        changed |= UI::PropertySlider("Mid2 Center", desc.Equalizer.Mid2Center, 1000.0f, 8000.0f);
        changed |= UI::PropertySlider("Mid2 Width", desc.Equalizer.Mid2Width, 0.01f, 1.0f);
        changed |= UI::PropertySlider("High Gain", desc.Equalizer.HighGain, 0.126f, 7.943f);
        changed |= UI::PropertySlider("High Cutoff", desc.Equalizer.HighCutoff, 4000.0f, 16000.0f);
        if (changed)
            SyncActiveMixer();
    }

    void AudioMixerPanel::RenderPitchShifterControls(AudioBusDesc& desc)
    {
        bool changed = false;
        changed |= UI::PropertySlider("Coarse Tune (semitones)", desc.PitchShifter.CoarseTune, -12, 12);
        changed |= UI::PropertySlider("Fine Tune (cents)", desc.PitchShifter.FineTune, -50, 50);
        if (changed)
            SyncActiveMixer();
    }

    void AudioMixerPanel::RenderFlangerControls(AudioBusDesc& desc)
    {
        bool changed = false;
        Vector<String> waveforms = { "Sinusoid", "Triangle" };
        int waveform = desc.Flanger.Waveform;
        if (UI::PropertyDropdown("Waveform", waveforms, waveform))
        {
            desc.Flanger.Waveform = waveform;
            changed = true;
        }
        changed |= UI::PropertySlider("Phase", desc.Flanger.Phase, -180, 180);
        changed |= UI::PropertySlider("Rate", desc.Flanger.Rate, 0.0f, 10.0f);
        changed |= UI::PropertySlider("Depth", desc.Flanger.Depth, 0.0f, 1.0f);
        changed |= UI::PropertySlider("Feedback", desc.Flanger.Feedback, -1.0f, 1.0f);
        changed |= UI::PropertySlider("Delay", desc.Flanger.Delay, 0.0f, 0.004f);
        if (changed)
            SyncActiveMixer();
    }

    void AudioMixerPanel::RenderCompressorControls(AudioBusDesc& desc)
    {
        if (UI::Property("Enabled", desc.Compressor.Enabled))
            SyncActiveMixer();
    }

    void AudioMixerPanel::RenderRingModulatorControls(AudioBusDesc& desc)
    {
        bool changed = false;
        changed |= UI::PropertySlider("Frequency", desc.RingModulator.Frequency, 0.0f, 8000.0f);
        changed |= UI::PropertySlider("Highpass Cutoff", desc.RingModulator.HighpassCutoff, 0.0f, 24000.0f);
        Vector<String> waveforms = { "Sinusoid", "Sawtooth", "Square" };
        int waveform = desc.RingModulator.Waveform;
        if (UI::PropertyDropdown("Waveform", waveforms, waveform))
        {
            desc.RingModulator.Waveform = waveform;
            changed = true;
        }
        if (changed)
            SyncActiveMixer();
    }

} // namespace Crowny
