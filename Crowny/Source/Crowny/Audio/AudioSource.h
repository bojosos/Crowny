#pragma once

#include "Crowny/Assets/AssetHandle.h"
#include "Crowny/Audio/AudioClip.h"
#include "Crowny/Common/RefCounted.h"

namespace Crowny
{
    class AudioBus;
    class AudioFilter;

    class AudioSource : public RefCounted
    {
    public:
        AudioSource();
        ~AudioSource();
        void OnTransformChanged(const Transform& transform);

        void SetClip(const AssetHandle<AudioClip>& clip);
        AssetHandle<AudioClip> GetAudioClip() const { return m_AudioClip; }
        void SetVolume(float volume);
        float GetVolume() const { return m_Volume; }
        void SetPitch(float pitch);
        float GetPitch() const { return m_Pitch; }
        void SetLooping(bool loop);
        bool GetLooping() const { return m_Loop; }
        void SetPriority(int32_t priority);
        int32_t GetPriority() const { return m_Priority; }

        void SetMinDistance(float distance);
        float GetMinDistance() const { return m_MinDistace; }
        void SetMaxDistance(float distance);
        float GetMaxDistance() const { return m_MaxDistance; }
        void SetAttenuation(float attenuation);
        float GetAttenuation() const { return m_Attenuation; }

        // Bus routing — when bus is set, the source's gain is multiplied by the bus chain's
        // effective gain, and the source's AL_AUXILIARY_SEND_FILTER points at the bus's aux slot.
        // Passing nullptr unroutes the source (direct-output, no bus contribution).
        void SetBus(const Ref<AudioBus>& bus);
        Ref<AudioBus> GetBus() const { return m_Bus; }

        // Per-source low/high pass filter on the direct path. Values are gain multipliers in [0, 1];
        // 1.0 = no attenuation. Both at 1.0 bypasses the filter entirely.
        void SetLowPassGain(float gainHF);
        float GetLowPassGain() const { return m_LowPassGain; }
        void SetHighPassGain(float gainLF);
        float GetHighPassGain() const { return m_HighPassGain; }

        // Directional cone. Inner/outer angles are in degrees (full cone, not half-angle); 360 =
        // omnidirectional and OpenAL skips cone math entirely. OuterGain is the gain applied
        // outside the outer cone; OuterGainHF is its high-frequency component (1.0 = no muffling).
        void SetConeInnerAngle(float degrees);
        float GetConeInnerAngle() const { return m_ConeInnerAngle; }
        void SetConeOuterAngle(float degrees);
        float GetConeOuterAngle() const { return m_ConeOuterAngle; }
        void SetConeOuterGain(float gain);
        float GetConeOuterGain() const { return m_ConeOuterGain; }
        void SetConeOuterGainHF(float gainHF);
        float GetConeOuterGainHF() const { return m_ConeOuterGainHF; }

        // Called by AudioBus when the bus chain's effective gain has changed. Recomputes AL_GAIN
        // from m_Volume * bus_gain. Not for use by client code.
        void RefreshEffectiveGain();

        void Play();
        void Pause();
        void Stop();
        AudioSourceState GetState() const;
        void SetGlobalPause(bool paused);
        void SetTime(float time);
        float GetTime() const;
        bool Is3D() const;

    private:
        bool RequiresStreaming() const;
        void Stream();
        bool FillStreamBuffer(uint32_t bufferId, uint32_t bufferIndex);
        void UpdateStreaming();
        void StartStreaming();
        void StopStreaming();
        void ReleaseOpenALResources();

    private:
        AssetHandle<AudioClip> m_AudioClip;
        float m_Volume = 1.0f;
        float m_Pitch = 1.0f;
        float m_MinDistace = 1.0f;
        float m_MaxDistance = 100.0f;
        int32_t m_Priority = 128;
        bool m_Loop = false;
        float m_Attenuation = 1.0f;
        bool m_GloballyPaused = false;
        bool m_ResumeAfterGlobalPause = false;

        bool m_IsStreaming = false;
        bool m_PlaybackRequested = false;
        static const uint32_t StreamBufferCount = 3;
        static const uint32_t StreamBufferSamples = 16384;
        uint32_t m_StreamBuffers[StreamBufferCount]{};
        uint32_t m_StreamBufferSampleCounts[StreamBufferCount]{};
        uint32_t m_StreamProcessedPosition = 0;
        uint32_t m_StreamQueuePosition = 0;
        uint32_t m_SourceID = 0;

        Ref<AudioBus> m_Bus;
        Scope<AudioFilter> m_Filter;
        float m_LowPassGain = 1.0f;
        float m_HighPassGain = 1.0f;

        glm::vec3 m_Velocity;
        glm::vec3 m_PrevPosition;
        bool m_HasPrevPosition = false;

        float m_ConeInnerAngle = 360.0f;
        float m_ConeOuterAngle = 360.0f;
        float m_ConeOuterGain = 0.0f;
        float m_ConeOuterGainHF = 1.0f;

        friend class AudioManager;
    };

} // namespace Crowny
