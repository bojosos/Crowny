#pragma once

#include "Crowny/Assets/Asset.h"
#include "Crowny/Audio/AudioBus.h"
#include "Crowny/Common/Common.h"

namespace Crowny
{

    // Per-effect parameter blocks. Each block is always serialized (even when not the active
    // effect) so users can switch effects in the editor without losing the previously-tuned values.
    struct ReverbParams
    {
        float Density = 1.0f;
        float Diffusion = 1.0f;
        float Gain = 0.32f;
        float GainHF = 0.89f;
        float DecayTime = 1.49f;
        float DecayHFRatio = 0.83f;
        float ReflectionsGain = 0.05f;
        float ReflectionsDelay = 0.007f;
        float LateReverbGain = 1.26f;
        float LateReverbDelay = 0.011f;
        float AirAbsorptionGainHF = 0.994f;
        float RoomRolloffFactor = 0.0f;
    };

    struct EchoParams
    {
        float Delay = 0.1f;
        float LRDelay = 0.1f;
        float Damping = 0.5f;
        float Feedback = 0.5f;
        float Spread = -1.0f;
    };

    struct DistortionParams
    {
        float Edge = 0.2f;
        float Gain = 0.05f;
        float LowpassCutoff = 8000.0f;
        float EqCenter = 3600.0f;
        float EqBandwidth = 3600.0f;
    };

    struct ChorusParams
    {
        int Waveform = 1;
        int Phase = 90;
        float Rate = 1.1f;
        float Depth = 0.1f;
        float Feedback = 0.25f;
        float Delay = 0.016f;
    };

    struct EqualizerParams
    {
        float LowGain = 1.0f;
        float LowCutoff = 200.0f;
        float Mid1Gain = 1.0f;
        float Mid1Center = 500.0f;
        float Mid1Width = 1.0f;
        float Mid2Gain = 1.0f;
        float Mid2Center = 3000.0f;
        float Mid2Width = 1.0f;
        float HighGain = 1.0f;
        float HighCutoff = 6000.0f;
    };

    struct PitchShifterParams
    {
        int CoarseTune = 0;
        int FineTune = 0;
    };

    struct FlangerParams
    {
        int Waveform = 1;
        int Phase = 0;
        float Rate = 0.27f;
        float Depth = 1.0f;
        float Feedback = -0.5f;
        float Delay = 0.002f;
    };

    struct CompressorParams
    {
        bool Enabled = true;
    };

    struct RingModulatorParams
    {
        float Frequency = 440.0f;
        float HighpassCutoff = 800.0f;
        int Waveform = 0;
    };

    // Serialized bus definition. The runtime tree of Ref<AudioBus> is built from these on Init().
    struct AudioBusDesc
    {
        String Name;
        String Parent;        // empty for root/master
        float Volume = 1.0f;
        bool Muted = false;

        // Selects which effect is wired to the bus's aux slot. AudioEffectType::None = dry bus.
        AudioEffectType FirstEffect = AudioEffectType::None;

        ReverbParams Reverb;
        EchoParams Echo;
        DistortionParams Distortion;
        ChorusParams Chorus;
        EqualizerParams Equalizer;
        PitchShifterParams PitchShifter;
        FlangerParams Flanger;
        CompressorParams Compressor;
        RingModulatorParams RingModulator;
    };

    // AudioMixer is a project asset. It owns the serialised bus layout and the runtime
    // Ref<AudioBus> tree (built by Init()) that audio sources route to.
    class AudioMixer : public Asset
    {
    public:
        AudioMixer();

        AssetType GetAssetType() const override { return AssetType::AudioMixer; }
        static AssetType GetStaticType() { return AssetType::AudioMixer; }

        // Builds the runtime bus tree from m_BusDescs. Called by AssetManager after Load and
        // explicitly when the editor mutates the bus layout. Idempotent — rebuilds from scratch.
        void Init() override;

        // Bus definitions (the design-time data). The master bus is the first entry by convention.
        Vector<AudioBusDesc>& GetBusDescs() { return m_BusDescs; }
        const Vector<AudioBusDesc>& GetBusDescs() const { return m_BusDescs; }

        Ref<AudioBus> FindBus(const String& name) const;
        Ref<AudioBus> GetMasterBus() const { return m_MasterBus; }
        const Vector<Ref<AudioBus>>& GetBuses() const { return m_Buses; }

        // Re-evaluates solo across all buses. Called when any bus's solo state changes.
        void RefreshSolo();

        // Pushes the latest design-time values from m_BusDescs into the matching runtime buses
        // without rebuilding the tree. Useful while the user is dragging sliders in the editor.
        void SyncRuntimeFromDescs();

    private:
        CW_SERIALIZABLE(AudioMixer);

        Vector<AudioBusDesc> m_BusDescs;

        // Runtime — not serialised.
        Vector<Ref<AudioBus>> m_Buses;
        Ref<AudioBus> m_MasterBus;
    };

} // namespace Crowny
