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
        if (!BeginPanel())
        {
            EndPanel();
            return;
        }

        AssetHandle<AudioMixer> handle = AudioManager::TryGet()->GetActiveMixer();
        ImGui::Columns(2, "##activeMixerColumns", false);
        ImGui::SetColumnWidth(0, 110.0f);
        if (UIUtils::AssetReference<AudioMixer>("Active Mixer", handle))
            AudioManager::TryGet()->SetActiveMixer(handle);
        ImGui::Columns(1);

        if (!handle)
        {
            ImGui::Spacing();
            ImGui::TextDisabled("Assign an Audio Mixer asset to edit its buses.");
            m_SelectedBusIndex = 0;
            m_PendingBusRemoval = std::numeric_limits<size_t>::max();
            EndPanel();
            return;
        }

        AudioMixer& mixer = *handle;
        Vector<AudioBusDesc>& descs = mixer.GetBusDescs();
        if (descs.empty())
        {
            ImGui::Separator();
            ImGui::TextDisabled("This mixer has no buses.");
            if (ImGui::Button("Create master bus"))
            {
                AudioBusDesc master;
                master.Name = "Master";
                descs.push_back(master);
                mixer.Init();
                m_SelectedBusIndex = 0;
            }
            EndPanel();
            return;
        }

        m_SelectedBusIndex = std::min(m_SelectedBusIndex, descs.size() - 1);

        ImGui::Separator();
        if (ImGui::Button("+ Add bus"))
        {
            uint32_t suffix = 1;
            String busName;
            do
            {
                busName = "Bus " + std::to_string(suffix++);
            } while (std::any_of(descs.begin(), descs.end(), [&](const AudioBusDesc& desc) { return desc.Name == busName; }));

            AudioBusDesc desc;
            desc.Name = busName;
            desc.Parent = descs.front().Name;
            descs.push_back(desc);
            mixer.Init();
            m_SelectedBusIndex = descs.size() - 1;
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Add a child of the master bus");

        size_t mutedCount = 0;
        size_t effectCount = 0;
        for (const AudioBusDesc& desc : descs)
        {
            mutedCount += desc.Muted ? 1 : 0;
            effectCount += desc.FirstEffect != AudioEffectType::None ? 1 : 0;
        }
        ImGui::SameLine();
        ImGui::TextDisabled("%zu buses  |  %zu muted  |  %zu effects", descs.size(), mutedCount, effectCount);

        ImGui::Spacing();
        const float availableWidth = ImGui::GetContentRegionAvail().x;
        if (availableWidth >= 520.0f)
        {
            const ImGuiTableFlags flags = ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp;
            if (ImGui::BeginTable("##mixerLayout", 2, flags))
            {
                ImGui::TableSetupColumn("Buses", ImGuiTableColumnFlags_WidthFixed, 210.0f);
                ImGui::TableSetupColumn("Bus settings", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::BeginChild("##busList", ImVec2(0.0f, 0.0f), false);
                RenderBusList(mixer);
                ImGui::EndChild();
                ImGui::TableNextColumn();
                ImGui::BeginChild("##busEditor", ImVec2(0.0f, 0.0f), false);
                RenderBus(mixer, m_SelectedBusIndex);
                ImGui::EndChild();
                ImGui::EndTable();
            }
        }
        else
        {
            ImGui::BeginChild("##busList", ImVec2(0.0f, std::min(180.0f, ImGui::GetContentRegionAvail().y * 0.35f)), true);
            RenderBusList(mixer);
            ImGui::EndChild();
            ImGui::Spacing();
            ImGui::BeginChild("##busEditor", ImVec2(0.0f, 0.0f), false);
            RenderBus(mixer, m_SelectedBusIndex);
            ImGui::EndChild();
        }

        RenderRemovalDialog(mixer);
        EndPanel();
    }

    void AudioMixerPanel::RenderBusList(AudioMixer& mixer)
    {
        const Vector<AudioBusDesc>& descs = mixer.GetBusDescs();
        ImGui::TextDisabled("Buses");
        ImGui::Separator();

        for (size_t i = 0; i < descs.size(); i++)
        {
            const AudioBusDesc& desc = descs[i];
            uint32_t depth = 0;
            StringView parent = desc.Parent;
            while (!parent.empty() && depth < descs.size())
            {
                const auto parentIt =
                  std::find_if(descs.begin(), descs.end(), [&](const AudioBusDesc& candidate) { return StringView(candidate.Name) == parent; });
                if (parentIt == descs.end())
                    break;
                parent = StringView(parentIt->Parent);
                depth++;
            }

            ImGui::PushID(static_cast<int>(i));
            ImGui::Indent(depth * 12.0f);
            String label = desc.Name;
            if (i == 0)
                label += "  [Master]";
            else if (desc.Muted)
                label += "  [Muted]";
            if (ImGui::Selectable(label.c_str(), m_SelectedBusIndex == i, ImGuiSelectableFlags_None, ImVec2(-1.0f, 0.0f)))
                m_SelectedBusIndex = i;
            if (ImGui::IsItemHovered())
            {
                if (i == 0)
                    ImGui::SetTooltip("Master output  |  Volume %.0f%%", desc.Volume * 100.0f);
                else
                    ImGui::SetTooltip("Routes to %s  |  Volume %.0f%%", desc.Parent.c_str(), desc.Volume * 100.0f);
            }
            ImGui::Unindent(depth * 12.0f);
            ImGui::PopID();
        }
    }

    void AudioMixerPanel::RenderBus(AudioMixer& mixer, size_t descIndex)
    {
        Vector<AudioBusDesc>& descs = mixer.GetBusDescs();
        if (descIndex >= descs.size())
        {
            ImGui::TextDisabled("Select a bus to edit it.");
            return;
        }

        AudioBusDesc& desc = descs[descIndex];
        ImGui::PushID(static_cast<int>(descIndex));
        const bool isMaster = descIndex == 0;
        ImGui::Text("%s", desc.Name.c_str());
        if (isMaster)
            ImGui::TextDisabled("Master output");
        else
            ImGui::TextDisabled("Routes to %s", desc.Parent.c_str());
        if (const Ref<AudioBus> runtimeBus = mixer.FindBus(desc.Name))
        {
            ImGui::SameLine();
            ImGui::TextDisabled(" |  Effective gain %.0f%%", runtimeBus->GetEffectiveGain() * 100.0f);
        }

        ImGui::SeparatorText("Routing");
        ImGui::Columns(2, "##routingColumns", false);
        ImGui::SetColumnWidth(0, std::clamp(ImGui::GetContentRegionAvail().x * 0.32f, 105.0f, 150.0f));
        if (!isMaster)
        {
            String name = desc.Name;
            if (UI::Property("Name", name) && !name.empty())
            {
                const bool duplicate =
                  std::any_of(descs.begin(), descs.end(), [&](const AudioBusDesc& other) { return &other != &desc && other.Name == name; });
                if (!duplicate)
                {
                    const String oldName = desc.Name;
                    desc.Name = name;
                    for (AudioBusDesc& other : descs)
                    {
                        if (other.Parent == oldName)
                            other.Parent = name;
                    }
                    mixer.Init();
                }
            }

            int parentIdx = 0;
            for (size_t i = 0; i < descIndex; i++)
            {
                if (descs[i].Name == desc.Parent)
                {
                    parentIdx = static_cast<int>(i);
                    break;
                }
            }
            if (UI::PropertyDropdown("Parent", descIndex, parentIdx, [&](size_t index) { return descs[index].Name.c_str(); }))
            {
                desc.Parent = descs[static_cast<size_t>(parentIdx)].Name;
                mixer.Init();
            }
        }
        else
        {
            ImGui::TextDisabled("Name");
            ImGui::NextColumn();
            ImGui::TextDisabled("Master");
            ImGui::NextColumn();
        }
        ImGui::Columns(1);

        ImGui::SeparatorText("Level");
        ImGui::Columns(2, "##levelColumns", false);
        ImGui::SetColumnWidth(0, std::clamp(ImGui::GetContentRegionAvail().x * 0.32f, 105.0f, 150.0f));
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
        ImGui::Columns(1);

        ImGui::SeparatorText("Effect");
        ImGui::Columns(2, "##effectColumns", false);
        ImGui::SetColumnWidth(0, std::clamp(ImGui::GetContentRegionAvail().x * 0.32f, 105.0f, 150.0f));
        int effectIdx = static_cast<int>(desc.FirstEffect);
        constexpr int effectCount = static_cast<int>(AudioEffectType::RingModulator) + 1;
        if (effectIdx >= effectCount)
            effectIdx = 0;
        if (UI::PropertyDropdown("Type",
                                 { "None", "Reverb", "Echo", "Distortion", "Chorus", "Equalizer", "Pitch Shifter", "Flanger",
                                   "Compressor", "Ring Modulator" },
                                 effectIdx))
        {
            desc.FirstEffect = static_cast<AudioEffectType>(effectIdx);
            mixer.Init();
        }

        if (desc.FirstEffect != AudioEffectType::None)
            RenderEffectControls(desc);
        ImGui::Columns(1);

        if (!isMaster)
        {
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.45f, 0.12f, 0.12f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.62f, 0.16f, 0.16f, 1.0f));
            if (ImGui::Button("Remove bus"))
            {
                m_PendingBusRemoval = descIndex;
                m_OpenRemovalPopup = true;
            }
            ImGui::PopStyleColor(2);
        }

        ImGui::PopID();
    }

    void AudioMixerPanel::RenderRemovalDialog(AudioMixer& mixer)
    {
        Vector<AudioBusDesc>& descs = mixer.GetBusDescs();
        if (m_PendingBusRemoval >= descs.size() || m_PendingBusRemoval == 0)
        {
            m_PendingBusRemoval = std::numeric_limits<size_t>::max();
            m_OpenRemovalPopup = false;
            return;
        }

        if (m_OpenRemovalPopup)
        {
            ImGui::OpenPopup("Remove bus?");
            m_OpenRemovalPopup = false;
        }

        if (ImGui::BeginPopupModal("Remove bus?", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            const String removedName = descs[m_PendingBusRemoval].Name;
            ImGui::Text("Remove \"%s\"?", removedName.c_str());
            ImGui::TextDisabled("Its child buses will route to Master.");
            ImGui::Spacing();
            if (ImGui::Button("Cancel", ImVec2(100.0f, 0.0f)))
            {
                m_PendingBusRemoval = std::numeric_limits<size_t>::max();
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.52f, 0.12f, 0.12f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.68f, 0.17f, 0.17f, 1.0f));
            if (ImGui::Button("Remove", ImVec2(100.0f, 0.0f)))
            {
                const size_t removedIndex = m_PendingBusRemoval;
                descs.erase(descs.begin() + removedIndex);
                for (AudioBusDesc& other : descs)
                {
                    if (other.Parent == removedName)
                        other.Parent = descs.front().Name;
                }
                mixer.Init();
                if (m_SelectedBusIndex == removedIndex)
                    m_SelectedBusIndex = 0;
                else if (m_SelectedBusIndex > removedIndex)
                    m_SelectedBusIndex--;
                m_PendingBusRemoval = std::numeric_limits<size_t>::max();
                ImGui::CloseCurrentPopup();
            }
            ImGui::PopStyleColor(2);
            ImGui::EndPopup();
        }
        else if (!ImGui::IsPopupOpen("Remove bus?"))
        {
            m_PendingBusRemoval = std::numeric_limits<size_t>::max();
        }
    }

    void AudioMixerPanel::RenderEffectControls(AudioBusDesc& desc)
    {
        switch (desc.FirstEffect)
        {
        case AudioEffectType::Reverb:
            RenderReverbControls(desc);
            break;
        case AudioEffectType::Echo:
            RenderEchoControls(desc);
            break;
        case AudioEffectType::Distortion:
            RenderDistortionControls(desc);
            break;
        case AudioEffectType::Chorus:
            RenderChorusControls(desc);
            break;
        case AudioEffectType::Equalizer:
            RenderEqualizerControls(desc);
            break;
        case AudioEffectType::PitchShifter:
            RenderPitchShifterControls(desc);
            break;
        case AudioEffectType::Flanger:
            RenderFlangerControls(desc);
            break;
        case AudioEffectType::Compressor:
            RenderCompressorControls(desc);
            break;
        case AudioEffectType::RingModulator:
            RenderRingModulatorControls(desc);
            break;
        default:
            break;
        }
    }

    // Pushes desc changes into the active mixer's runtime buses so parameter tweaks are audible
    // without rebuilding the bus tree.
    static void SyncActiveMixer()
    {
        if (AssetHandle<AudioMixer> mixer = AudioManager::TryGet()->GetActiveMixer())
            mixer->SyncRuntimeFromDescs();
    }

    void AudioMixerPanel::RenderReverbControls(AudioBusDesc& desc)
    {
        ImGui::TextUnformatted("Preset");
        ImGui::NextColumn();
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::BeginCombo("##reverbPreset", "Choose preset"))
        {
            for (uint8_t i = 0; i < static_cast<uint8_t>(ReverbEffect::Preset::Count); i++)
            {
                const ReverbEffect::Preset preset = static_cast<ReverbEffect::Preset>(i);
                if (ImGui::Selectable(ReverbEffect::GetPresetName(preset)))
                {
                    ReverbEffect effect;
                    effect.ApplyPreset(preset);
                    desc.Reverb.Density = effect.Density;
                    desc.Reverb.Diffusion = effect.Diffusion;
                    desc.Reverb.Gain = effect.Gain;
                    desc.Reverb.GainHF = effect.GainHF;
                    desc.Reverb.DecayTime = effect.DecayTime;
                    desc.Reverb.DecayHFRatio = effect.DecayHFRatio;
                    desc.Reverb.ReflectionsGain = effect.ReflectionsGain;
                    desc.Reverb.ReflectionsDelay = effect.ReflectionsDelay;
                    desc.Reverb.LateReverbGain = effect.LateReverbGain;
                    desc.Reverb.LateReverbDelay = effect.LateReverbDelay;
                    desc.Reverb.AirAbsorptionGainHF = effect.AirAbsorptionGainHF;
                    desc.Reverb.RoomRolloffFactor = effect.RoomRolloffFactor;
                    SyncActiveMixer();
                }
            }
            ImGui::EndCombo();
        }
        ImGui::NextColumn();

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
        int waveform = desc.Chorus.Waveform;
        if (UI::PropertyDropdown("Waveform", { "Sinusoid", "Triangle" }, waveform))
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
        int waveform = desc.Flanger.Waveform;
        if (UI::PropertyDropdown("Waveform", { "Sinusoid", "Triangle" }, waveform))
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
        int waveform = desc.RingModulator.Waveform;
        if (UI::PropertyDropdown("Waveform", { "Sinusoid", "Sawtooth", "Square" }, waveform))
        {
            desc.RingModulator.Waveform = waveform;
            changed = true;
        }
        if (changed)
            SyncActiveMixer();
    }

} // namespace Crowny
