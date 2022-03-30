#include "cwpch.h"

#include "Crowny/Audio/AudioManager.h"
#include "Crowny/Audio/AudioUtils.h"
#include "Crowny/Common/StringUtils.h"

#include <AL/al.h>
#include <glm/glm.hpp>

namespace Crowny
{

    AudioManager::AudioManager()
    {
        bool enumerated;
        if (alcIsExtensionPresent(nullptr, "ALC_ENUMERATE_ALL_EXT") != ALC_FALSE)
        {
            const ALCchar* defaultDevice = alcGetString(nullptr, ALC_DEFAULT_ALL_DEVICES_SPECIFIER);
            m_DefaultDevice.Name = defaultDevice;
            const ALCchar* devices = alcGetString(nullptr, ALC_ALL_DEVICES_SPECIFIER);
            Vector<char> deviceName;
            while (true)
            {
                if (*devices == 0)
                {
                    if (deviceName.empty())
                        break;
                    String name(deviceName.data(), deviceName.size());
                    m_Devices.push_back({ name });
                    deviceName.clear();
                    devices++;
                    continue;
                }

                deviceName.push_back(*devices);
                devices++;
            }
            enumerated = true;
        }
        else
        {
            m_Devices.push_back({ "" });
            enumerated = false;
        }

        m_ActiveDevice = m_DefaultDevice;
        String defaultDeviceName = m_DefaultDevice.Name;
        if (enumerated)
            m_Device = alcOpenDevice(defaultDeviceName.c_str());
        else
            m_Device = alcOpenDevice(nullptr);
        if (m_Device == nullptr)
            CW_ENGINE_ERROR("OpenAL device creation failed. Device: {0}", defaultDeviceName);

        ALCcontext* context = alcCreateContext(m_Device, nullptr);
        if (context)
            SetContext(context);
        alcMakeContextCurrent(context);
    }

    float AudioManager::GetGlobalSourceProgress(const String& name)
    {

        auto iterFind = m_ManualSources.find(name);
        if (iterFind != m_ManualSources.end())
        {
        CW_ENGINE_INFO("{0}, {1}", iterFind->second->GetTime(), iterFind->second->GetAudioClip()->GetLength());
            return iterFind->second->GetTime() / iterFind->second->GetAudioClip()->GetLength(); // TODO: Fix inconsistent setter/getter for clips
    }
        return 0;
    }

    void AudioManager::Play(const String& name, const AssetHandle<AudioClip>& clip, const glm::vec3& position, float volume)
    {
        Ref<AudioSource> source = CreateSource();
        source->SetClip(clip);
        // source->SetTransfrom(position);
        source->SetVolume(volume);
        source->Play();

        m_ManualSources[name] = source;
    }

    void AudioManager::StopManualSources()
    {
        for (auto& source : m_ManualSources)
            source.second->Stop();

        m_ManualSources.clear();
    }

    void AudioManager::OnUpdate()
    {
        // Clear sources that are finished playing
        for (auto kv : m_ManualSources)
        {
            if (kv.second->GetState() != AudioSourceState::Stopped)
                m_TempSources[kv.first] = kv.second;
        }

        std::swap(m_TempSources, m_ManualSources);
        m_TempSources.clear();
    }

    AudioManager::~AudioManager()
    {
        alcMakeContextCurrent(nullptr);
        alcDestroyContext(m_Context);
        m_Context = nullptr;

        if (m_Device != nullptr)
            alcCloseDevice(m_Device);
    }

    void AudioManager::SetVolume(float volume)
    {
        m_Volume = glm::clamp(volume, 0.0f, 1.0f);
        m_Listener->SetVolume(volume);
    }

    float AudioManager::GetVolume() const { return m_Volume; }

    void AudioManager::RegisterSource(AudioSource* source) { m_Sources.insert(source); }

    void AudioManager::UnregisterSource(AudioSource* source) { m_Sources.erase(source); }

    void AudioManager::RegisterListener(AudioListener* listener)
    {
        if (m_Listener != nullptr)
        {
            CW_ENGINE_ERROR("Listener already exists.");
            return;
        }
        m_Listener = listener;
    }

    void AudioManager::UnregisterListener(AudioListener* listener)
    {
        CW_ENGINE_ASSERT(m_Listener == listener);
        m_Listener = nullptr;
    }

    Ref<AudioSource> AudioManager::CreateSource() { return CreateRef<AudioSource>(); }

    Ref<AudioListener> AudioManager::CreateListener() { return CreateRef<AudioListener>(); }

    void AudioManager::SetPaused(bool paused)
    {
        if (m_IsPaused == paused)
            return;
        m_IsPaused = paused;
        for (auto& source : m_Sources)
            source->SetGlobalPause(paused);
    }

    void AudioManager::SetActiveDevice(const AudioDevice& device)
    {
        if (m_Devices.size() == 1)
            return;

        alcMakeContextCurrent(nullptr);
        if (m_Context != nullptr)
            alcDestroyContext(m_Context);
        m_Context = nullptr;

        if (m_Device != nullptr)
            alcCloseDevice(m_Device);
        m_ActiveDevice = device;
        m_Device = alcOpenDevice(device.Name.c_str());
        if (m_Device == nullptr)
            CW_ENGINE_ERROR("OpenAL device creation failed. Device: {0}", device.Name);
    }

    bool AudioManager::IsExtSupported(const String& ext) const
    {
        if (m_Device == nullptr)
            return false;
        if (ext.length() > 2 && ext.substr(0, 3) == "ALC")
            return alcIsExtensionPresent(m_Device, ext.c_str()) != ALC_FALSE;
        else
            return alIsExtensionPresent(ext.c_str()) != AL_FALSE;
    }

    ALCcontext* AudioManager::GetContext() const { return m_Context; }

    void AudioManager::WriteToOpenALBuffer(uint32_t bufferId, uint8_t* samples, const AudioDataInfo& info)
    {
        if (info.NumChannels <= 2) // stereo or mono
        {
            if (info.BitDepth > 16)
            {
                if (IsExtSupported("AL_EXT_float32"))
                {
                    uint32_t bufferSize = info.NumSamples * sizeof(float);
                    float* sampleBufferFloats = new float[bufferSize];
                    AudioUtils::ConvertToFloat(samples, info.BitDepth, sampleBufferFloats, info.NumSamples);
                    ALenum format = AudioUtils::GetOpenALFormat(info.NumChannels, info.BitDepth);
                    alBufferData(bufferId, format, sampleBufferFloats, bufferSize, info.SampleRate);
                    delete[] sampleBufferFloats;
                }
                else
                {
                    CW_ENGINE_WARN("Audio data will be truncated. OpenAL does not support floats.");
                    uint32_t bufferSize = info.NumSamples * 2;
                    uint8_t* samples16 = new uint8_t[bufferSize];
                    AudioUtils::ConvertBitDepth(samples, info.BitDepth, samples16, 16, info.NumSamples);
                    ALenum format = AudioUtils::GetOpenALFormat(info.NumChannels, 16);
                    alBufferData(bufferId, format, samples16, bufferSize, info.SampleRate);
                    delete[] samples16;
                }
            }
            else if (info.BitDepth == 8)
            {
                uint32_t bufferSize = info.NumSamples * 2;
                uint8_t* sampleBuffer = new uint8_t[bufferSize];
                for (uint32_t i = 0; i < info.NumSamples; i++)
                    sampleBuffer[i] = ((int8_t*)samples)[i] + 128;
                ALenum format = AudioUtils::GetOpenALFormat(info.NumChannels, 16);
                alBufferData(bufferId, format, sampleBuffer, bufferSize, info.SampleRate);
                delete[] sampleBuffer;
            }
            else
            {
                ALenum format = AudioUtils::GetOpenALFormat(info.NumChannels, info.BitDepth);
                alBufferData(bufferId, format, samples, info.NumSamples * (info.BitDepth / 8), info.SampleRate);
            }
        }
        else // fancy audio
        {
            if (info.BitDepth == 24)
            {
                uint32_t bufferSize = info.NumSamples * sizeof(int32_t);
                uint8_t* samples32 = new uint8_t[bufferSize];
                AudioUtils::ConvertBitDepth(samples, info.BitDepth, samples32, 32, info.NumSamples);
                ALenum format = AudioUtils::GetOpenALFormat(info.NumChannels, 32);
                alBufferData(bufferId, format, samples32, bufferSize, info.SampleRate);
                delete[] samples32;
            }
            else if (info.BitDepth == 8)
            {
                uint32_t bufferSize = info.NumSamples * (info.BitDepth / 8);
                uint8_t* sampleBuffer = new uint8_t[bufferSize];
                for (uint32_t i = 0; i < info.NumSamples; i++)
                    sampleBuffer[i] = ((int8_t*)samples)[i] + 128;
                ALenum format = AudioUtils::GetOpenALFormat(info.NumChannels, 16);
                alBufferData(bufferId, format, sampleBuffer, bufferSize, info.SampleRate);
                delete[] sampleBuffer;
            }
            else
            {
                ALenum format = AudioUtils::GetOpenALFormat(info.NumChannels, info.BitDepth);
                alBufferData(bufferId, format, samples, info.NumChannels * (info.BitDepth / 8), info.SampleRate);
            }
        }
    }

    AudioManager& gAudio() { return AudioManager::Get(); }

} // namespace Crowny