#pragma once

#include "Crowny/Assets/AssetHandle.h"
#include "Crowny/Audio/AudioMixer.h"
#include "Panels/ImGuiPanel.h"

namespace Crowny
{

    // Editor panel for the active AudioMixer asset. Displays the bus tree with per-bus volume/mute/
    // solo, plus the first effect's parameters. Edits write back to the AudioBusDesc and call
    // AudioMixer::SyncRuntimeFromDescs() so the change is audible immediately.
    class AudioMixerPanel : public ImGuiPanel
    {
    public:
        AudioMixerPanel(const String& name);
        ~AudioMixerPanel() = default;

        void Render() override;

    private:
        void RenderBus(AudioMixer& mixer, size_t descIndex);
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
    };

} // namespace Crowny
