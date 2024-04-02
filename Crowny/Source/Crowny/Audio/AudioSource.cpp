#include "cwpch.h"

#include "Crowny/Audio/AudioManager.h"
#include "Crowny/Audio/AudioSource.h"
#include "Crowny/Audio/AudioUtils.h"
#include "Crowny/Ecs/Components.h"

#include <AL/al.h>

namespace Crowny
{

    AudioSource::AudioSource()
    {
        alGenSources(1, &m_SourceID);

        alSourcef(m_SourceID, AL_PITCH, m_Pitch);
        alSourcef(m_SourceID, AL_REFERENCE_DISTANCE, m_MinDistace);
        alSourcef(m_SourceID, AL_ROLLOFF_FACTOR, m_Attenuation);

        if (RequiresStreaming())
            alSourcei(m_SourceID, AL_LOOPING, false);
        else
            alSourcei(m_SourceID, AL_LOOPING, m_Loop);

        if (Is3D())
        {
            alSourcei(m_SourceID, AL_SOURCE_RELATIVE, false);
            alSource3f(m_SourceID, AL_POSITION, 0.0f, 0.0f, 0.0f);
            alSource3f(m_SourceID, AL_VELOCITY, 0.0f, 0.0f, 0.0f);
        }
        else
        {
            alSourcei(m_SourceID, AL_SOURCE_RELATIVE, true);
            alSource3f(m_SourceID, AL_POSITION, 0.0f, 0.0f, 0.0f);
            alSource3f(m_SourceID, AL_VELOCITY, 0.0f, 0.0f, 0.0f);
        }

        if (m_IsStreaming)
        {
            uint32_t oaBuffer = 0;
            // if (m_AudioClip->IsLoaded())
            {
                oaBuffer = m_AudioClip->GetOpenALBuffer();
            }

            alSourcei(m_SourceID, AL_BUFFER, oaBuffer);
        }

        if (m_SavedState != AudioSourceState::Stopped)
            Play();
        if (m_SavedState == AudioSourceState::Paused)
            Pause();
        gAudio().RegisterSource(this);
    }

    AudioSource::~AudioSource()
    {
        Stop();
        alSourcei(m_SourceID, AL_BUFFER, 0);
        alDeleteSources(1, &m_SourceID);
    }

    void AudioSource::OnTransformChanged(const Transform& transform)
    {
        if (Is3D())
        {
            const glm::vec3& position = transform.GetPosition();
            alSource3f(m_SourceID, AL_POSITION, position.x, position.y, position.z);
        }
        else
        {
            alSource3f(m_SourceID, AL_POSITION, 0.0f, 0.0f, 0.0f);
        }
        // alSource3f(m_SourceID, AL_VELOCITY, m_Velocity.x, m_Velocity.y, m_Velocity.z);
    }

    void AudioSource::SetGlobalPause(bool paused)
    {
        if (m_GloballyPaused == paused)
            return;
        m_GloballyPaused = paused;
        if (GetState() == AudioSourceState::Playing)
        {
            if (paused)
            {
                alSourcePause(m_SourceID);
            }
        }
        else
        {
            Play();
        }
    }

    void AudioSource::SetVolume(float volume)
    {
        m_Volume = glm::clamp(volume, 0.0f, 1.0f);
        alSourcef(m_SourceID, AL_GAIN, m_Volume);
    }

    void AudioSource::SetClip(const AssetHandle<AudioClip>& clip)
    {
        Stop();
        m_AudioClip = clip;

        alSourcei(m_SourceID, AL_SOURCE_RELATIVE, !Is3D());
        if (!RequiresStreaming())
        {
            uint32_t oaBuffer = 0;
            if (m_AudioClip != nullptr)
            {
                oaBuffer = m_AudioClip->GetOpenALBuffer();
            }

            alSourcei(m_SourceID, AL_BUFFER, oaBuffer);
        }
        SetLooping(m_Loop);
        Play();
    }

    void AudioSource::Play()
    {
        if (m_GloballyPaused)
            return;
        if (RequiresStreaming())
        {
            if (!m_IsStreaming)
            {
                Stream();
            }
        }
        alSourcePlay(m_SourceID);
    }

    void AudioSource::Pause() { alSourcePause(m_SourceID); }

    void AudioSource::Stop()
    {
        alSourceStop(m_SourceID);
        alSourcef(m_SourceID, AL_SEC_OFFSET, 0.0f);
        if (m_IsStreaming)
            StopStreaming();
    }

    void AudioSource::SetPriority(int32_t priority) { m_Priority = priority; }

    void AudioSource::SetTime(float time)
    {
        AudioSourceState state = GetState();
        Stop();
        bool requiresStream = RequiresStreaming();
        float cTime;
        if (!requiresStream)
            cTime = time;
        else
        {
            m_StreamProcessedPosition = (uint32_t)(time * m_AudioClip->GetFrequency() * m_AudioClip->GetNumChannels());
            m_StreamQueuePosition = m_StreamProcessedPosition;
            cTime = 0.0f;
        }

        alSourcef(m_SourceID, AL_SEC_OFFSET, cTime);
        if (state != AudioSourceState::Stopped)
            Play();
        if (state == AudioSourceState::Paused)
            Pause();
    }

    float AudioSource::GetTime() const
    {
        bool requiresStream = RequiresStreaming();
        float time;
        if (!requiresStream)
        {
            alGetSourcef(m_SourceID, AL_SEC_OFFSET, &time);
            return time;
        }
        else
        {
            float timeOffset = 0.0f;
            timeOffset = (float)m_StreamProcessedPosition / m_AudioClip->GetFrequency() / m_AudioClip->GetNumChannels();
            alGetSourcef(m_SourceID, AL_SEC_OFFSET, &time);
            return timeOffset + time;
        }
    }

    void AudioSource::SetPitch(float pitch)
    {
        m_Pitch = pitch;
        alSourcef(m_SourceID, AL_PITCH, pitch);
    }

    void AudioSource::SetMinDistance(float distance)
    {
        m_MinDistace = distance;

        alSourcef(m_SourceID, AL_REFERENCE_DISTANCE, distance);
    }

    void AudioSource::SetMaxDistance(float distance)
    {
        m_MaxDistance = distance;
        alSourcef(m_SourceID, AL_MAX_DISTANCE, distance);
    }

    void AudioSource::SetAttenuation(float attenuation)
    {
        m_Attenuation = attenuation;
        alSourcef(m_SourceID, AL_ROLLOFF_FACTOR, attenuation);
    }

    void AudioSource::SetLooping(bool loop)
    {
        m_Loop = loop;
        if (RequiresStreaming())
            loop = false;

        alSourcei(m_SourceID, AL_LOOPING, loop);
    }

    AudioSourceState AudioSource::GetState() const
    {
        ALint state;
        alGetSourcei(m_SourceID, AL_SOURCE_STATE, &state);
        switch (state)
        {
        case AL_PLAYING:
            return AudioSourceState::Playing;
        case AL_PAUSED:
            return AudioSourceState::Paused;
        case AL_INITIAL:
        case AL_STOPPED:
        default:
            return AudioSourceState::Stopped;
        }
    }

    void AudioSource::StartStreaming()
    {
        CW_ENGINE_ASSERT(!m_IsStreaming);
        alGenBuffers(StreamBufferCount, m_StreamBuffers);
        // gAudio().StartStreaming(this);
        std::memset(&m_BusyBuffers, 0, sizeof(m_BusyBuffers));
        m_IsStreaming = true;
    }

    void AudioSource::StopStreaming()
    {
        CW_ENGINE_ASSERT(m_IsStreaming);
        m_IsStreaming = false;
        // gAudio().StopStreaming(this);

        int32_t numQueuedBuffers;
        alGetSourcei(m_SourceID, AL_BUFFERS_QUEUED, &numQueuedBuffers);
        uint32_t buff;
        for (int32_t j = 0; j < numQueuedBuffers; j++)
            alSourceUnqueueBuffers(m_SourceID, 1, &buff);

        alDeleteBuffers(StreamBufferCount, m_StreamBuffers);
    }

    void AudioSource::Stream() {}

    bool AudioSource::Is3D() const
    {
        if (m_AudioClip == nullptr)
            return true;

        return m_AudioClip->Is3D();
    }

    bool AudioSource::RequiresStreaming() const
    {
        if (m_AudioClip == nullptr)
            return false;
        AudioReadMode readMode = m_AudioClip->GetDesc().ReadMode;
        bool isCompressed =
          readMode == AudioReadMode::LoadCompressed && m_AudioClip->GetDesc().Format != AudioFormat::PCM;
        return (readMode == AudioReadMode::Stream) || isCompressed;
    }
} // namespace Crowny
