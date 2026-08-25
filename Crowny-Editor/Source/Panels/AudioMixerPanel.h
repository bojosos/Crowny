#pragma once

#include "Crowny/Assets/AssetHandle.h"
#include "Crowny/Audio/AudioMixer.h"
#include "Panels/EditorPanelRegistration.h"
#include "Panels/ImGuiPanel.h"

#include <limits>

namespace Crowny
{

    // Editor panel for the active AudioMixer asset. Displays the bus tree with per-bus volume/mute/
    // solo, plus the first effect's parameters. Edits write back to the AudioBusDesc and call
    // AudioMixer::SyncRuntimeFromDescs() so the change is audible immediately.
    class AudioMixerPanel : public ImGuiPanel
    {
    public:
        inline static constexpr EditorPanelRegistration<AudioMixerPanel> Registration{ "Audio Mixer", "View/Audio Mixer", "", false };

        AudioMixerPanel(const String& name);
        ~AudioMixerPanel() = default;

        void Render() override;

    private:
        void RenderBusList(AudioMixer& mixer);
        void RenderBus(AudioMixer& mixer, size_t descIndex);
        void RenderRemovalDialog(AudioMixer& mixer);
        void RenderEffectControls(AudioBusDesc& desc);
        void RenderReverbControls(AudioBusDesc& desc);
        void RenderEchoControls(AudioBusDesc& desc);
        void RenderDistortionControls(AudioBusDesc& desc);
        void RenderChorusControls(AudioBusDesc& desc);
        void RenderEqualizerControls(AudioBusDesc& desc);
        void RenderPitchShifterControls(AudioBusDesc& desc);
        void RenderFlangerControls(AudioBusDesc& desc);
        void RenderCompressorControls(AudioBusDesc& desc);
        void RenderRingModulatorControls(AudioBusDesc& desc);

        size_t m_SelectedBusIndex = 0;
        size_t m_PendingBusRemoval = std::numeric_limits<size_t>::max();
        bool m_OpenRemovalPopup = false;
    };

} // namespace Crowny
