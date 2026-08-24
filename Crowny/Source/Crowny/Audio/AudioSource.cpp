#include "cwpch.h"

#include "Crowny/Audio/AudioBus.h"
#include "Crowny/Audio/AudioFilter.h"
#include "Crowny/Audio/AudioManager.h"
#include "Crowny/Audio/AudioMixer.h"
#include "Crowny/Audio/AudioSource.h"
#include "Crowny/Audio/AudioUtils.h"
#include "Crowny/Common/Time.h"
#include "Crowny/Ecs/Components.h"

#include <AL/al.h>
#include <AL/efx.h>

namespace Crowny
{

    AudioSource::AudioSource()
    {
        m_Filter = CreateScope<AudioFilter>();
        if (AudioManager::TryGet() == nullptr || !AudioManager::TryGet()->IsAvailable())
            return;

        alGenSources(1, &m_SourceID);
        if (m_SourceID == 0)
            return;

        alSourcef(m_SourceID, AL_PITCH, m_Pitch);
        alSourcef(m_SourceID, AL_GAIN, m_Volume);
        alSourcef(m_SourceID, AL_REFERENCE_DISTANCE, m_MinDistace);
        alSourcef(m_SourceID, AL_MAX_DISTANCE, m_MaxDistance);
        alSourcef(m_SourceID, AL_ROLLOFF_FACTOR, m_Attenuation);
        alSourcei(m_SourceID, AL_LOOPING, AL_FALSE);
        alSourcei(m_SourceID, AL_SOURCE_RELATIVE, AL_FALSE);
        alSource3f(m_SourceID, AL_POSITION, 0.0f, 0.0f, 0.0f);
        alSource3f(m_SourceID, AL_VELOCITY, 0.0f, 0.0f, 0.0f);
        AudioManager::TryGet()->RegisterSource(this);

        // Auto-route to the active mixer's master bus, if any. The user can override with SetBus().
        if (const AssetHandle<AudioMixer>& activeMixer = AudioManager::TryGet()->GetActiveMixer())
            SetBus(activeMixer->GetMasterBus());
    }

    AudioSource::~AudioSource()
    {
        if (m_Bus)
            m_Bus->UnregisterSource(this);
        ReleaseOpenALResources();
        if (AudioManager::TryGet())
            AudioManager::TryGet()->UnregisterSource(this);
    }

    void AudioSource::ReleaseOpenALResources()
    {
        if (m_SourceID == 0)
            return;

        AudioManager* manager = AudioManager::TryGet();
        if (manager != nullptr && manager->EnsureContextCurrent())
        {
            Stop();
            m_Filter->Detach(m_SourceID);
            alSourcei(m_SourceID, AL_BUFFER, 0);
            alDeleteSources(1, &m_SourceID);
        }

        m_SourceID = 0;
        m_IsStreaming = false;
        m_PlaybackRequested = false;
        m_ResumeAfterGlobalPause = false;
        std::memset(m_StreamBuffers, 0, sizeof(m_StreamBuffers));
        std::memset(m_StreamBufferSampleCounts, 0, sizeof(m_StreamBufferSampleCounts));
    }

    void AudioSource::OnTransformChanged(const Transform& transform)
    {
        if (m_SourceID == 0)
            return;
        const glm::vec3 position = transform.GetPosition();
        if (Is3D())
            alSource3f(m_SourceID, AL_POSITION, position.x, position.y, position.z);
        else
            alSource3f(m_SourceID, AL_POSITION, 0.0f, 0.0f, 0.0f);

        // Forward direction for cone math. Push always so a non-cone source flipping to a cone
        // doesn't lag a frame; OpenAL ignores AL_DIRECTION when both cone angles are 360.
        const glm::mat4& world = transform.GetMatrix();
        const glm::vec3 forward = glm::normalize(-glm::vec3(world[2]));
        alSource3f(m_SourceID, AL_DIRECTION, forward.x, forward.y, forward.z);

        if (!m_HasPrevPosition)
        {
            m_PrevPosition = position;
            m_HasPrevPosition = true;
            return;
        }

        const float dt = Time::GetDeltaTime();
        if (dt > 1e-6f)
        {
            m_Velocity = (position - m_PrevPosition) / dt;
            alSource3f(m_SourceID, AL_VELOCITY, m_Velocity.x, m_Velocity.y, m_Velocity.z);
            m_PrevPosition = position;
        }
    }

    void AudioSource::SetGlobalPause(bool paused)
    {
        if (m_GloballyPaused == paused)
            return;
        if (paused)
        {
            m_ResumeAfterGlobalPause = AudioUtils::ShouldResumeAfterGlobalPause(GetState());
            if (m_ResumeAfterGlobalPause && m_SourceID != 0)
                alSourcePause(m_SourceID);
        }
        m_GloballyPaused = paused;
        if (!paused && m_ResumeAfterGlobalPause)
        {
            m_ResumeAfterGlobalPause = false;
            Play();
        }
    }

    void AudioSource::SetVolume(float volume)
    {
        m_Volume = glm::clamp(volume, 0.0f, 1.0f);
        RefreshEffectiveGain();
    }

    void AudioSource::RefreshEffectiveGain()
    {
        const float busGain = m_Bus ? m_Bus->GetEffectiveGain() : 1.0f;
        if (m_SourceID != 0)
            alSourcef(m_SourceID, AL_GAIN, m_Volume * busGain);
    }

    void AudioSource::SetBus(const Ref<AudioBus>& bus)
    {
        if (m_Bus == bus)
            return;
        if (m_Bus)
            m_Bus->UnregisterSource(this);

        m_Bus = bus;

        if (m_Bus)
            m_Bus->RegisterSource(this);

        // Re-route the source's aux send to the new bus's aux slot. When EFX is unavailable, or the
        // bus has no aux slot allocated, this clears the send (AL_EFFECTSLOT_NULL).
        if (m_SourceID != 0 && AudioManager::TryGet() && AudioManager::TryGet()->IsEFXAvailable())
        {
            const ALuint slot = m_Bus ? m_Bus->GetAuxSlot() : 0;
            alSource3i(m_SourceID, AL_AUXILIARY_SEND_FILTER, static_cast<ALint>(slot), 0, AL_FILTER_NULL);
        }

        RefreshEffectiveGain();
    }

    void AudioSource::SetLowPassGain(float gainHF)
    {
        m_LowPassGain = glm::clamp(gainHF, 0.0f, 1.0f);
        if (m_SourceID != 0)
            m_Filter->Apply(m_SourceID, m_LowPassGain, m_HighPassGain);
    }

    void AudioSource::SetHighPassGain(float gainLF)
    {
        m_HighPassGain = glm::clamp(gainLF, 0.0f, 1.0f);
        if (m_SourceID != 0)
            m_Filter->Apply(m_SourceID, m_LowPassGain, m_HighPassGain);
    }

    void AudioSource::SetConeInnerAngle(float degrees)
    {
        m_ConeInnerAngle = glm::clamp(degrees, 0.0f, 360.0f);
        if (m_SourceID != 0)
            alSourcef(m_SourceID, AL_CONE_INNER_ANGLE, m_ConeInnerAngle);
    }

    void AudioSource::SetConeOuterAngle(float degrees)
    {
        m_ConeOuterAngle = glm::clamp(degrees, 0.0f, 360.0f);
        if (m_SourceID != 0)
            alSourcef(m_SourceID, AL_CONE_OUTER_ANGLE, m_ConeOuterAngle);
    }

    void AudioSource::SetConeOuterGain(float gain)
    {
        m_ConeOuterGain = glm::clamp(gain, 0.0f, 1.0f);
        if (m_SourceID != 0)
            alSourcef(m_SourceID, AL_CONE_OUTER_GAIN, m_ConeOuterGain);
    }

    void AudioSource::SetConeOuterGainHF(float gainHF)
    {
        m_ConeOuterGainHF = glm::clamp(gainHF, 0.0f, 1.0f);
        if (m_SourceID != 0 && AudioManager::TryGet() && AudioManager::TryGet()->IsEFXAvailable())
            alSourcef(m_SourceID, AL_CONE_OUTER_GAINHF, m_ConeOuterGainHF);
    }

    void AudioSource::SetClip(const AssetHandle<AudioClip>& clip)
    {
        if (m_AudioClip.GetHandleData() == clip.GetHandleData())
            return;
        const AudioSourceState previousState = GetState();
        const bool resumeAfterGlobalPause = m_ResumeAfterGlobalPause;
        Stop();
        m_AudioClip = clip;
        m_StreamProcessedPosition = 0;
        m_StreamQueuePosition = 0;

        if (m_SourceID == 0)
            return;
        alSourcei(m_SourceID, AL_SOURCE_RELATIVE, Is3D() ? AL_FALSE : AL_TRUE);
        alSourcei(m_SourceID, AL_BUFFER, 0);
        if (!RequiresStreaming())
        {
            uint32_t oaBuffer = m_AudioClip ? m_AudioClip->GetOpenALBuffer() : 0;
            if (oaBuffer == static_cast<uint32_t>(-1))
                oaBuffer = 0;
            alSourcei(m_SourceID, AL_BUFFER, oaBuffer);
        }
        SetLooping(m_Loop);

        if (previousState == AudioSourceState::Playing || resumeAfterGlobalPause)
            Play();
        else if (previousState == AudioSourceState::Paused)
        {
            Play();
            Pause();
        }
    }

    void AudioSource::Play()
    {
        if (!m_AudioClip || m_SourceID == 0)
            return;
        m_PlaybackRequested = true;
        if (m_GloballyPaused)
        {
            m_ResumeAfterGlobalPause = true;
            return;
        }
        if (RequiresStreaming())
        {
            if (!m_IsStreaming)
                StartStreaming();
            Stream();
        }
        alSourcePlay(m_SourceID);
    }

    void AudioSource::Pause()
    {
        m_PlaybackRequested = false;
        m_ResumeAfterGlobalPause = false;
        if (m_SourceID != 0)
            alSourcePause(m_SourceID);
    }

    void AudioSource::Stop()
    {
        m_PlaybackRequested = false;
        m_ResumeAfterGlobalPause = false;
        if (m_SourceID == 0)
            return;
        alSourceStop(m_SourceID);
        if (m_IsStreaming)
            StopStreaming();
        else
            alSourcef(m_SourceID, AL_SEC_OFFSET, 0.0f);
        m_StreamProcessedPosition = 0;
        m_StreamQueuePosition = 0;
    }

    void AudioSource::SetPriority(int32_t priority) { m_Priority = glm::clamp(priority, 0, 255); }

    void AudioSource::SetTime(float time)
    {
        if (!m_AudioClip)
            return;
        const AudioSourceState state = GetState();
        const bool resumeAfterGlobalPause = m_ResumeAfterGlobalPause;
        time = glm::clamp(time, 0.0f, m_AudioClip->GetLength());
        Stop();
        const bool requiresStream = RequiresStreaming();
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
        if (state == AudioSourceState::Playing || resumeAfterGlobalPause)
            Play();
        else if (state == AudioSourceState::Paused)
        {
            Play();
            Pause();
        }
    }

    float AudioSource::GetTime() const
    {
        if (!m_AudioClip || m_SourceID == 0)
            return 0.0f;
        const bool requiresStream = RequiresStreaming();
        float time;
        if (!requiresStream)
        {
            alGetSourcef(m_SourceID, AL_SEC_OFFSET, &time);
            return time;
        }
        else
        {
            const float timeOffset = (float)m_StreamProcessedPosition / m_AudioClip->GetFrequency() / m_AudioClip->GetNumChannels();
            alGetSourcef(m_SourceID, AL_SEC_OFFSET, &time);
            return timeOffset + time;
        }
    }

    void AudioSource::SetPitch(float pitch)
    {
        m_Pitch = std::max(0.001f, pitch);
        if (m_SourceID != 0)
            alSourcef(m_SourceID, AL_PITCH, m_Pitch);
    }

    void AudioSource::SetMinDistance(float distance)
    {
        m_MinDistace = std::max(0.0f, distance);
        if (m_MaxDistance < m_MinDistace)
            m_MaxDistance = m_MinDistace;
        if (m_SourceID != 0)
        {
            alSourcef(m_SourceID, AL_REFERENCE_DISTANCE, m_MinDistace);
            alSourcef(m_SourceID, AL_MAX_DISTANCE, m_MaxDistance);
        }
    }

    void AudioSource::SetMaxDistance(float distance)
    {
        m_MaxDistance = std::max(distance, m_MinDistace);
        if (m_SourceID != 0)
            alSourcef(m_SourceID, AL_MAX_DISTANCE, m_MaxDistance);
    }

    void AudioSource::SetAttenuation(float attenuation)
    {
        m_Attenuation = std::max(0.0f, attenuation);
        if (m_SourceID != 0)
            alSourcef(m_SourceID, AL_ROLLOFF_FACTOR, m_Attenuation);
    }

    void AudioSource::SetLooping(bool loop)
    {
        m_Loop = loop;
        if (RequiresStreaming())
            loop = false;

        if (m_SourceID != 0)
            alSourcei(m_SourceID, AL_LOOPING, loop ? AL_TRUE : AL_FALSE);
    }

    AudioSourceState AudioSource::GetState() const
    {
        if (m_GloballyPaused && m_ResumeAfterGlobalPause)
            return AudioSourceState::Paused;
        if (m_SourceID == 0)
            return AudioSourceState::Stopped;
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
        std::memset(m_StreamBufferSampleCounts, 0, sizeof(m_StreamBufferSampleCounts));
        m_IsStreaming = true;
    }

    void AudioSource::StopStreaming()
    {
        CW_ENGINE_ASSERT(m_IsStreaming);
        m_IsStreaming = false;

        int32_t numQueuedBuffers = 0;
        alGetSourcei(m_SourceID, AL_BUFFERS_QUEUED, &numQueuedBuffers);
        uint32_t buff;
        for (int32_t j = 0; j < numQueuedBuffers; j++)
            alSourceUnqueueBuffers(m_SourceID, 1, &buff);

        alDeleteBuffers(StreamBufferCount, m_StreamBuffers);
        std::memset(m_StreamBuffers, 0, sizeof(m_StreamBuffers));
        std::memset(m_StreamBufferSampleCounts, 0, sizeof(m_StreamBufferSampleCounts));
    }

    bool AudioSource::FillStreamBuffer(uint32_t bufferId, uint32_t bufferIndex)
    {
        if (!m_AudioClip || m_AudioClip->GetNumSamples() == 0)
            return false;

        const uint32_t targetSamples = std::min(StreamBufferSamples, m_AudioClip->GetNumSamples());
        const uint32_t bytesPerSample = m_AudioClip->GetDesc().BitDepth / 8;
        Vector<uint8_t> samples(targetSamples * bytesPerSample);
        uint32_t samplesRead = 0;
        if (m_StreamQueuePosition >= m_AudioClip->GetNumSamples())
        {
            if (!m_Loop)
                return false;
            m_StreamQueuePosition = 0;
        }
        while (samplesRead < targetSamples)
        {
            const uint32_t count = std::min(targetSamples - samplesRead, m_AudioClip->GetNumSamples() - m_StreamQueuePosition);
            const uint32_t read = m_AudioClip->GetSamples(samples.data() + samplesRead * bytesPerSample, m_StreamQueuePosition, count);
            samplesRead += read;
            m_StreamQueuePosition += read;
            if (read < count || m_StreamQueuePosition >= m_AudioClip->GetNumSamples())
            {
                if (!m_Loop)
                    break;
                m_StreamQueuePosition = 0;
            }
            if (read == 0)
                break;
        }

        if (samplesRead == 0)
            return false;

        AudioDataInfo info = { samplesRead, m_AudioClip->GetFrequency(), m_AudioClip->GetNumChannels(), m_AudioClip->GetDesc().BitDepth };
        if (!AudioManager::TryGet()->WriteToOpenALBuffer(bufferId, samples.data(), info))
            return false;
        m_StreamBufferSampleCounts[bufferIndex] = samplesRead;
        return true;
    }

    void AudioSource::Stream()
    {
        if (!m_IsStreaming)
            return;

        ALint processed = 0;
        alGetSourcei(m_SourceID, AL_BUFFERS_PROCESSED, &processed);
        while (processed-- > 0)
        {
            uint32_t bufferId = 0;
            alSourceUnqueueBuffers(m_SourceID, 1, &bufferId);
            for (uint32_t i = 0; i < StreamBufferCount; i++)
            {
                if (m_StreamBuffers[i] != bufferId)
                    continue;
                m_StreamProcessedPosition += m_StreamBufferSampleCounts[i];
                if (m_Loop && m_AudioClip->GetNumSamples() > 0)
                    m_StreamProcessedPosition %= m_AudioClip->GetNumSamples();
                m_StreamBufferSampleCounts[i] = 0;
                if (FillStreamBuffer(bufferId, i))
                    alSourceQueueBuffers(m_SourceID, 1, &bufferId);
                break;
            }
        }

        ALint queued = 0;
        alGetSourcei(m_SourceID, AL_BUFFERS_QUEUED, &queued);
        for (uint32_t i = 0; i < StreamBufferCount && queued < static_cast<ALint>(StreamBufferCount); i++)
        {
            if (m_StreamBufferSampleCounts[i] != 0)
                continue;
            if (!FillStreamBuffer(m_StreamBuffers[i], i))
                break;
            alSourceQueueBuffers(m_SourceID, 1, &m_StreamBuffers[i]);
            queued++;
        }
    }

    void AudioSource::UpdateStreaming()
    {
        if (!m_IsStreaming || m_GloballyPaused)
            return;
        Stream();
        ALint queued = 0;
        alGetSourcei(m_SourceID, AL_BUFFERS_QUEUED, &queued);
        if (m_PlaybackRequested && queued == 0)
        {
            m_PlaybackRequested = false;
            StopStreaming();
            m_StreamProcessedPosition = 0;
            m_StreamQueuePosition = 0;
        }
        else if (m_PlaybackRequested)
        {
            ALint alState = AL_STOPPED;
            alGetSourcei(m_SourceID, AL_SOURCE_STATE, &alState);
            if (alState != AL_PLAYING)
                alSourcePlay(m_SourceID);
        }
    }

    bool AudioSource::Is3D() const
    {
        if (!m_AudioClip)
            return true;

        return m_AudioClip->Is3D();
    }

    bool AudioSource::RequiresStreaming() const
    {
        if (!m_AudioClip)
            return false;
        const AudioReadMode readMode = m_AudioClip->GetDesc().ReadMode;
        const bool isCompressed = readMode == AudioReadMode::LoadCompressed && m_AudioClip->GetDesc().Format != AudioFormat::PCM;
        return (readMode == AudioReadMode::Stream) || isCompressed;
    }
} // namespace Crowny
