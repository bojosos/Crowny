#pragma once

#include "Crowny/Common/Common.h"
#include "Crowny/Common/RefCounted.h"

#include <AL/al.h>

namespace Crowny
{

    enum class AudioEffectType : uint8_t
    {
        None = 0,
        Reverb = 1,
        Echo = 2,
        Distortion = 3,
        Chorus = 4,
        Equalizer = 5,
        PitchShifter = 6,
        Flanger = 7,
        Compressor = 8,
        RingModulator = 9,
    };

    // Owns one OpenAL effect object. Subclasses generate the effect in their constructor and push
    // typed parameters onto it via Apply(). When EFX is unavailable the effect id stays 0 and
    // Apply() is a no-op so the rest of the audio system keeps working unmodified.
    class AudioEffect : public RefCounted
    {
    public:
        AudioEffect() = default;
        virtual ~AudioEffect();

        virtual AudioEffectType GetType() const = 0;
        const String& GetName() const { return m_Name; }
        void SetName(const String& name) { m_Name = name; }

        // Pushes current parameters into the OpenAL effect object. The owning bus must rebind the
        // effect to its aux slot afterwards so OpenAL picks up the new state.
        virtual void Apply() = 0;

        ALuint GetEffectId() const { return m_EffectId; }
        bool IsValid() const { return m_EffectId != 0; }

    protected:
        String m_Name;
        ALuint m_EffectId = 0;
    };

    class ReverbEffect : public AudioEffect
    {
    public:
        // Approximations of common Unity/Godot presets, mapped to standard reverb parameters.
        enum class Preset : uint8_t
        {
            Generic,
            Room,
            Hall,
            Cave,
            Underwater,
            Count
        };

        ReverbEffect();

        AudioEffectType GetType() const override { return AudioEffectType::Reverb; }
        void Apply() override;

        // Parameters mirror AL_REVERB_* (which is the subset that maps cleanly to AL_EAXREVERB).
        // Ranges are clamped to OpenAL's documented min/max in Apply().
        float Density = 1.0f;
        float Diffusion = 1.0f;
        float Gain = 0.32f;
        float GainHF = 0.89f;
        float DecayTime = 1.49f;       // seconds
        float DecayHFRatio = 0.83f;
        float ReflectionsGain = 0.05f;
        float ReflectionsDelay = 0.007f;
        float LateReverbGain = 1.26f;
        float LateReverbDelay = 0.011f;
        float AirAbsorptionGainHF = 0.994f;
        float RoomRolloffFactor = 0.0f;

        void ApplyPreset(Preset preset);
        static const char* GetPresetName(Preset preset);
    };

    class EchoEffect : public AudioEffect
    {
    public:
        EchoEffect();
        AudioEffectType GetType() const override { return AudioEffectType::Echo; }
        void Apply() override;

        float Delay = 0.1f;       // seconds, [0, 0.207]
        float LRDelay = 0.1f;     // seconds, [0, 0.404]
        float Damping = 0.5f;     // [0, 0.99]
        float Feedback = 0.5f;    // [0, 1]
        float Spread = -1.0f;     // [-1, 1]
    };

    class DistortionEffect : public AudioEffect
    {
    public:
        DistortionEffect();
        AudioEffectType GetType() const override { return AudioEffectType::Distortion; }
        void Apply() override;

        float Edge = 0.2f;           // [0, 1]
        float Gain = 0.05f;          // [0.01, 1]
        float LowpassCutoff = 8000.0f; // Hz, [80, 24000]
        float EqCenter = 3600.0f;    // Hz, [80, 24000]
        float EqBandwidth = 3600.0f; // Hz, [80, 24000]
    };

    class ChorusEffect : public AudioEffect
    {
    public:
        ChorusEffect();
        AudioEffectType GetType() const override { return AudioEffectType::Chorus; }
        void Apply() override;

        int Waveform = 1;       // 0 = sinusoid, 1 = triangle
        int Phase = 90;         // degrees, [-180, 180]
        float Rate = 1.1f;      // Hz, [0, 10]
        float Depth = 0.1f;     // [0, 1]
        float Feedback = 0.25f; // [-1, 1]
        float Delay = 0.016f;   // seconds, [0, 0.016]
    };

    // 4-band parametric EQ. Gain values are linear multipliers ~10^(dB/20) — the EFX spec uses
    // [0.126, 7.943] which covers roughly ±18 dB around unity.
    class EqualizerEffect : public AudioEffect
    {
    public:
        EqualizerEffect();
        AudioEffectType GetType() const override { return AudioEffectType::Equalizer; }
        void Apply() override;

        float LowGain = 1.0f;       // [0.126, 7.943]
        float LowCutoff = 200.0f;   // Hz, [50, 800]
        float Mid1Gain = 1.0f;      // [0.126, 7.943]
        float Mid1Center = 500.0f;  // Hz, [200, 3000]
        float Mid1Width = 1.0f;     // [0.01, 1]
        float Mid2Gain = 1.0f;      // [0.126, 7.943]
        float Mid2Center = 3000.0f; // Hz, [1000, 8000]
        float Mid2Width = 1.0f;     // [0.01, 1]
        float HighGain = 1.0f;      // [0.126, 7.943]
        float HighCutoff = 6000.0f; // Hz, [4000, 16000]
    };

    // Coarse/fine pitch shift without time stretching. Coarse is semitones, fine is cents.
    class PitchShifterEffect : public AudioEffect
    {
    public:
        PitchShifterEffect();
        AudioEffectType GetType() const override { return AudioEffectType::PitchShifter; }
        void Apply() override;

        int CoarseTune = 0; // semitones, [-12, 12]
        int FineTune = 0;   // cents, [-50, 50]
    };

    // Like chorus but with a feedback path that produces metallic / jet-sweep tones.
    class FlangerEffect : public AudioEffect
    {
    public:
        FlangerEffect();
        AudioEffectType GetType() const override { return AudioEffectType::Flanger; }
        void Apply() override;

        int Waveform = 1;        // 0 = sinusoid, 1 = triangle
        int Phase = 0;           // degrees, [-180, 180]
        float Rate = 0.27f;      // Hz, [0, 10]
        float Depth = 1.0f;      // [0, 1]
        float Feedback = -0.5f;  // [-1, 1]
        float Delay = 0.002f;    // seconds, [0, 0.004]
    };

    // OpenAL's compressor is fixed-parameter — it's just an on/off switch in the EFX spec.
    // Useful for taming peaks on a music bus; full sidechain compression is out of scope.
    class CompressorEffect : public AudioEffect
    {
    public:
        CompressorEffect();
        AudioEffectType GetType() const override { return AudioEffectType::Compressor; }
        void Apply() override;

        bool Enabled = true;
    };

    // Multiplies the signal by a low-frequency carrier waveform; metallic / robotic tones.
    class RingModulatorEffect : public AudioEffect
    {
    public:
        RingModulatorEffect();
        AudioEffectType GetType() const override { return AudioEffectType::RingModulator; }
        void Apply() override;

        float Frequency = 440.0f;       // Hz, [0, 8000]
        float HighpassCutoff = 800.0f;  // Hz, [0, 24000]
        int Waveform = 0;               // 0 = sinusoid, 1 = sawtooth, 2 = square
    };

} // namespace Crowny
