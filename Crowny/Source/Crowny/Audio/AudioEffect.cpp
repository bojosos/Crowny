#include "cwpch.h"

#include "Crowny/Audio/AudioEffect.h"
#include "Crowny/Audio/AudioManager.h"

#include <AL/efx.h>

#include <algorithm>

namespace Crowny
{

    AudioEffect::~AudioEffect()
    {
        if (m_EffectId != 0 && gAudioManager && gAudioManager->IsEFXAvailable())
            gAudioManager->GetEFX().DeleteEffects(1, &m_EffectId);
    }

    ReverbEffect::ReverbEffect()
    {
        if (gAudioManager == nullptr || !gAudioManager->IsEFXAvailable())
            return;

        const EFX& efx = gAudioManager->GetEFX();
        // Prefer EAXREVERB when available; standard AL_REVERB shares the same parameter subset
        // we expose so Apply() uses AL_REVERB_* tokens regardless.
        const ALenum type = efx.HasEAXReverb ? AL_EFFECT_EAXREVERB : AL_EFFECT_REVERB;
        efx.GenEffects(1, &m_EffectId);
        efx.Effecti(m_EffectId, AL_EFFECT_TYPE, type);
        if (alGetError() != AL_NO_ERROR)
        {
            efx.DeleteEffects(1, &m_EffectId);
            m_EffectId = 0;
        }
    }

    void ReverbEffect::Apply()
    {
        if (!IsValid() || !gAudioManager->IsEFXAvailable())
            return;
        const EFX& efx = gAudioManager->GetEFX();

        // Reuse AL_REVERB_* tokens — they map to the equivalent EAX fields when the effect was
        // allocated as AL_EFFECT_EAXREVERB (the parameter enums for the shared params are aliased).
        auto setf = [&](ALenum p, float v, float lo, float hi) { efx.Effectf(m_EffectId, p, std::clamp(v, lo, hi)); };

        setf(AL_REVERB_DENSITY, Density, 0.0f, 1.0f);
        setf(AL_REVERB_DIFFUSION, Diffusion, 0.0f, 1.0f);
        setf(AL_REVERB_GAIN, Gain, 0.0f, 1.0f);
        setf(AL_REVERB_GAINHF, GainHF, 0.0f, 1.0f);
        setf(AL_REVERB_DECAY_TIME, DecayTime, 0.1f, 20.0f);
        setf(AL_REVERB_DECAY_HFRATIO, DecayHFRatio, 0.1f, 2.0f);
        setf(AL_REVERB_REFLECTIONS_GAIN, ReflectionsGain, 0.0f, 3.16f);
        setf(AL_REVERB_REFLECTIONS_DELAY, ReflectionsDelay, 0.0f, 0.3f);
        setf(AL_REVERB_LATE_REVERB_GAIN, LateReverbGain, 0.0f, 10.0f);
        setf(AL_REVERB_LATE_REVERB_DELAY, LateReverbDelay, 0.0f, 0.1f);
        setf(AL_REVERB_AIR_ABSORPTION_GAINHF, AirAbsorptionGainHF, 0.892f, 1.0f);
        setf(AL_REVERB_ROOM_ROLLOFF_FACTOR, RoomRolloffFactor, 0.0f, 10.0f);
    }

    void ReverbEffect::ApplyPreset(Preset preset)
    {
        switch (preset)
        {
        case Preset::Generic:
            Density = 1.0f; Diffusion = 1.0f; Gain = 0.32f; GainHF = 0.89f;
            DecayTime = 1.49f; DecayHFRatio = 0.83f;
            ReflectionsGain = 0.05f; ReflectionsDelay = 0.007f;
            LateReverbGain = 1.26f; LateReverbDelay = 0.011f;
            AirAbsorptionGainHF = 0.994f; RoomRolloffFactor = 0.0f;
            break;
        case Preset::Room:
            Density = 1.0f; Diffusion = 1.0f; Gain = 0.32f; GainHF = 0.59f;
            DecayTime = 0.40f; DecayHFRatio = 0.83f;
            ReflectionsGain = 0.15f; ReflectionsDelay = 0.002f;
            LateReverbGain = 1.06f; LateReverbDelay = 0.003f;
            AirAbsorptionGainHF = 0.994f; RoomRolloffFactor = 0.0f;
            break;
        case Preset::Hall:
            Density = 1.0f; Diffusion = 1.0f; Gain = 0.32f; GainHF = 0.81f;
            DecayTime = 3.00f; DecayHFRatio = 0.59f;
            ReflectionsGain = 0.05f; ReflectionsDelay = 0.020f;
            LateReverbGain = 1.41f; LateReverbDelay = 0.029f;
            AirAbsorptionGainHF = 0.994f; RoomRolloffFactor = 0.0f;
            break;
        case Preset::Cave:
            Density = 1.0f; Diffusion = 1.0f; Gain = 0.32f; GainHF = 1.0f;
            DecayTime = 2.91f; DecayHFRatio = 1.30f;
            ReflectionsGain = 0.50f; ReflectionsDelay = 0.015f;
            LateReverbGain = 0.71f; LateReverbDelay = 0.022f;
            AirAbsorptionGainHF = 0.994f; RoomRolloffFactor = 0.0f;
            break;
        case Preset::Underwater:
            Density = 0.36f; Diffusion = 1.0f; Gain = 0.32f; GainHF = 0.01f;
            DecayTime = 1.49f; DecayHFRatio = 0.10f;
            ReflectionsGain = 0.59f; ReflectionsDelay = 0.007f;
            LateReverbGain = 7.07f; LateReverbDelay = 0.011f;
            AirAbsorptionGainHF = 0.994f; RoomRolloffFactor = 0.0f;
            break;
        default:
            break;
        }
        Apply();
    }

    const char* ReverbEffect::GetPresetName(Preset preset)
    {
        switch (preset)
        {
        case Preset::Generic: return "Generic";
        case Preset::Room: return "Room";
        case Preset::Hall: return "Hall";
        case Preset::Cave: return "Cave";
        case Preset::Underwater: return "Underwater";
        default: return "Unknown";
        }
    }

    // Helper: allocate an AL effect of the given type. Mirrors ReverbEffect's pattern but stays a
    // free function in this TU so it doesn't leak into the header.
    static void AllocateEffect(ALuint& effectId, ALenum type)
    {
        if (gAudioManager == nullptr || !gAudioManager->IsEFXAvailable())
            return;
        const EFX& efx = gAudioManager->GetEFX();
        efx.GenEffects(1, &effectId);
        efx.Effecti(effectId, AL_EFFECT_TYPE, type);
        if (alGetError() != AL_NO_ERROR)
        {
            efx.DeleteEffects(1, &effectId);
            effectId = 0;
        }
    }

    EchoEffect::EchoEffect() { AllocateEffect(m_EffectId, AL_EFFECT_ECHO); }

    void EchoEffect::Apply()
    {
        if (!IsValid())
            return;
        const EFX& efx = gAudioManager->GetEFX();
        efx.Effectf(m_EffectId, AL_ECHO_DELAY, std::clamp(Delay, 0.0f, 0.207f));
        efx.Effectf(m_EffectId, AL_ECHO_LRDELAY, std::clamp(LRDelay, 0.0f, 0.404f));
        efx.Effectf(m_EffectId, AL_ECHO_DAMPING, std::clamp(Damping, 0.0f, 0.99f));
        efx.Effectf(m_EffectId, AL_ECHO_FEEDBACK, std::clamp(Feedback, 0.0f, 1.0f));
        efx.Effectf(m_EffectId, AL_ECHO_SPREAD, std::clamp(Spread, -1.0f, 1.0f));
    }

    DistortionEffect::DistortionEffect() { AllocateEffect(m_EffectId, AL_EFFECT_DISTORTION); }

    void DistortionEffect::Apply()
    {
        if (!IsValid())
            return;
        const EFX& efx = gAudioManager->GetEFX();
        efx.Effectf(m_EffectId, AL_DISTORTION_EDGE, std::clamp(Edge, 0.0f, 1.0f));
        efx.Effectf(m_EffectId, AL_DISTORTION_GAIN, std::clamp(Gain, 0.01f, 1.0f));
        efx.Effectf(m_EffectId, AL_DISTORTION_LOWPASS_CUTOFF, std::clamp(LowpassCutoff, 80.0f, 24000.0f));
        efx.Effectf(m_EffectId, AL_DISTORTION_EQCENTER, std::clamp(EqCenter, 80.0f, 24000.0f));
        efx.Effectf(m_EffectId, AL_DISTORTION_EQBANDWIDTH, std::clamp(EqBandwidth, 80.0f, 24000.0f));
    }

    ChorusEffect::ChorusEffect() { AllocateEffect(m_EffectId, AL_EFFECT_CHORUS); }

    void ChorusEffect::Apply()
    {
        if (!IsValid())
            return;
        const EFX& efx = gAudioManager->GetEFX();
        efx.Effecti(m_EffectId, AL_CHORUS_WAVEFORM, std::clamp(Waveform, 0, 1));
        efx.Effecti(m_EffectId, AL_CHORUS_PHASE, std::clamp(Phase, -180, 180));
        efx.Effectf(m_EffectId, AL_CHORUS_RATE, std::clamp(Rate, 0.0f, 10.0f));
        efx.Effectf(m_EffectId, AL_CHORUS_DEPTH, std::clamp(Depth, 0.0f, 1.0f));
        efx.Effectf(m_EffectId, AL_CHORUS_FEEDBACK, std::clamp(Feedback, -1.0f, 1.0f));
        efx.Effectf(m_EffectId, AL_CHORUS_DELAY, std::clamp(Delay, 0.0f, 0.016f));
    }

    EqualizerEffect::EqualizerEffect() { AllocateEffect(m_EffectId, AL_EFFECT_EQUALIZER); }

    void EqualizerEffect::Apply()
    {
        if (!IsValid())
            return;
        const EFX& efx = gAudioManager->GetEFX();
        efx.Effectf(m_EffectId, AL_EQUALIZER_LOW_GAIN, std::clamp(LowGain, 0.126f, 7.943f));
        efx.Effectf(m_EffectId, AL_EQUALIZER_LOW_CUTOFF, std::clamp(LowCutoff, 50.0f, 800.0f));
        efx.Effectf(m_EffectId, AL_EQUALIZER_MID1_GAIN, std::clamp(Mid1Gain, 0.126f, 7.943f));
        efx.Effectf(m_EffectId, AL_EQUALIZER_MID1_CENTER, std::clamp(Mid1Center, 200.0f, 3000.0f));
        efx.Effectf(m_EffectId, AL_EQUALIZER_MID1_WIDTH, std::clamp(Mid1Width, 0.01f, 1.0f));
        efx.Effectf(m_EffectId, AL_EQUALIZER_MID2_GAIN, std::clamp(Mid2Gain, 0.126f, 7.943f));
        efx.Effectf(m_EffectId, AL_EQUALIZER_MID2_CENTER, std::clamp(Mid2Center, 1000.0f, 8000.0f));
        efx.Effectf(m_EffectId, AL_EQUALIZER_MID2_WIDTH, std::clamp(Mid2Width, 0.01f, 1.0f));
        efx.Effectf(m_EffectId, AL_EQUALIZER_HIGH_GAIN, std::clamp(HighGain, 0.126f, 7.943f));
        efx.Effectf(m_EffectId, AL_EQUALIZER_HIGH_CUTOFF, std::clamp(HighCutoff, 4000.0f, 16000.0f));
    }

    PitchShifterEffect::PitchShifterEffect() { AllocateEffect(m_EffectId, AL_EFFECT_PITCH_SHIFTER); }

    void PitchShifterEffect::Apply()
    {
        if (!IsValid())
            return;
        const EFX& efx = gAudioManager->GetEFX();
        efx.Effecti(m_EffectId, AL_PITCH_SHIFTER_COARSE_TUNE, std::clamp(CoarseTune, -12, 12));
        efx.Effecti(m_EffectId, AL_PITCH_SHIFTER_FINE_TUNE, std::clamp(FineTune, -50, 50));
    }

    FlangerEffect::FlangerEffect() { AllocateEffect(m_EffectId, AL_EFFECT_FLANGER); }

    void FlangerEffect::Apply()
    {
        if (!IsValid())
            return;
        const EFX& efx = gAudioManager->GetEFX();
        efx.Effecti(m_EffectId, AL_FLANGER_WAVEFORM, std::clamp(Waveform, 0, 1));
        efx.Effecti(m_EffectId, AL_FLANGER_PHASE, std::clamp(Phase, -180, 180));
        efx.Effectf(m_EffectId, AL_FLANGER_RATE, std::clamp(Rate, 0.0f, 10.0f));
        efx.Effectf(m_EffectId, AL_FLANGER_DEPTH, std::clamp(Depth, 0.0f, 1.0f));
        efx.Effectf(m_EffectId, AL_FLANGER_FEEDBACK, std::clamp(Feedback, -1.0f, 1.0f));
        efx.Effectf(m_EffectId, AL_FLANGER_DELAY, std::clamp(Delay, 0.0f, 0.004f));
    }

    CompressorEffect::CompressorEffect() { AllocateEffect(m_EffectId, AL_EFFECT_COMPRESSOR); }

    void CompressorEffect::Apply()
    {
        if (!IsValid())
            return;
        gAudioManager->GetEFX().Effecti(m_EffectId, AL_COMPRESSOR_ONOFF, Enabled ? 1 : 0);
    }

    RingModulatorEffect::RingModulatorEffect() { AllocateEffect(m_EffectId, AL_EFFECT_RING_MODULATOR); }

    void RingModulatorEffect::Apply()
    {
        if (!IsValid())
            return;
        const EFX& efx = gAudioManager->GetEFX();
        efx.Effectf(m_EffectId, AL_RING_MODULATOR_FREQUENCY, std::clamp(Frequency, 0.0f, 8000.0f));
        efx.Effectf(m_EffectId, AL_RING_MODULATOR_HIGHPASS_CUTOFF, std::clamp(HighpassCutoff, 0.0f, 24000.0f));
        efx.Effecti(m_EffectId, AL_RING_MODULATOR_WAVEFORM, std::clamp(Waveform, 0, 2));
    }

} // namespace Crowny
