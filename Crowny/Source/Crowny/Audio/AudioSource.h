#pragma once

#include "Crowny/Assets/AssetHandle.h"
#include "Crowny/Audio/AudioClip.h"

namespace Crowny
{
    class AudioSource
    {
    public:
        AudioSource();
        ~AudioSource();
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
        void Play();
        void Pause();
        void Stop();
        AudioSourceState GetState() const;
        void SetGlobalPause(bool paused);
        void SetTime(float time);
        float GetTime() const;

    private:
        bool RequiresStreaming() const;
        bool Is3D() const;
        void Stream();
        void StartStreaming();
        void StopStreaming();

    private:
        AssetHandle<AudioClip> m_AudioClip;
        float m_Volume = 1.0f;
        float m_Pitch = 1.0f;
        float m_MinDistace = 1.0f;
        float m_MaxDistance = 100.0f;
        int32_t m_Priority;
        bool m_Loop = false;
        float m_Attenuation;
        bool m_GloballyPaused = false;
        bool m_Paused = true;

        AudioSourceState m_SavedState = AudioSourceState::Stopped;
        float m_SavedTime = 0.0f;

        bool m_IsStreaming = false;
        static const uint32_t StreamBufferCount = 3;
        uint32_t m_StreamBuffers[StreamBufferCount];
        uint32_t m_BusyBuffers[StreamBufferCount];
        uint32_t m_StreamProcessedPosition = 0;
        uint32_t m_StreamQueuePosition = 0;
        uint32_t m_SourceID;

        glm::vec3 m_Velocity;
    };

} // namespace Crowny