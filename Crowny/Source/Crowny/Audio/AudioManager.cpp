#include "cwpch.h"

#include "Crowny/Audio/AudioBus.h"
#include "Crowny/Audio/AudioListener.h"
#include "Crowny/Audio/AudioManager.h"
#include "Crowny/Audio/AudioMixer.h"
#include "Crowny/Audio/AudioUtils.h"
#include "Crowny/Common/StringUtils.h"

#include <AL/al.h>
#include <glm/glm.hpp>

namespace Crowny
{
    void AudioManager::OnShutdown()
    {
        EnsureContextCurrent();
        StopManualSources();
        m_ActiveMixer = {};
        if (!m_Sources.empty())
            CW_ENGINE_WARN("Audio manager is shutting down with {0} live source(s).", m_Sources.size());
        for (AudioSource* source : m_Sources)
            source->ReleaseOpenALResources();
        m_Sources.clear();
        m_EFX.Reset();
    }

    AudioManager::AudioManager()
    {
        const bool enumerateAll = alcIsExtensionPresent(nullptr, "ALC_ENUMERATE_ALL_EXT") != ALC_FALSE;
        const bool enumerateBasic = alcIsExtensionPresent(nullptr, "ALC_ENUMERATION_EXT") != ALC_FALSE;
        if (enumerateAll || enumerateBasic)
        {
            const ALCenum defaultSpecifier = enumerateAll ? ALC_DEFAULT_ALL_DEVICES_SPECIFIER : ALC_DEFAULT_DEVICE_SPECIFIER;
            const ALCenum deviceSpecifier = enumerateAll ? ALC_ALL_DEVICES_SPECIFIER : ALC_DEVICE_SPECIFIER;
            if (const ALCchar* defaultDevice = alcGetString(nullptr, defaultSpecifier))
                m_DefaultDevice.Name = defaultDevice;

            if (const ALCchar* devices = alcGetString(nullptr, deviceSpecifier))
            {
                while (*devices != '\0')
                {
                    const String name(devices);
                    m_Devices.push_back({ name });
                    devices += name.size() + 1;
                }
            }
        }

        if (m_Devices.empty())
            m_Devices.push_back({ m_DefaultDevice.Name });

        m_ActiveDevice = m_DefaultDevice;
        const String defaultDeviceName = m_DefaultDevice.Name;
        if (enumerateAll || enumerateBasic)
            m_Device = alcOpenDevice(defaultDeviceName.empty() ? nullptr : defaultDeviceName.c_str());
        else
            m_Device = alcOpenDevice(nullptr);
        if (m_Device == nullptr)
        {
            CW_ENGINE_ERROR("OpenAL device creation failed. Device: {0}", defaultDeviceName);
            return;
        }

        // Only pass the EFX context attribute to devices that advertise it. Implementations are
        // allowed to reject unknown attributes instead of ignoring them.
        const ALCint attrs[] = { ALC_MAX_AUXILIARY_SENDS, 4, 0 };
        const bool advertisesEFX = alcIsExtensionPresent(m_Device, "ALC_EXT_EFX") != ALC_FALSE;
        m_Context = alcCreateContext(m_Device, advertisesEFX ? attrs : nullptr);
        if (m_Context == nullptr && advertisesEFX)
        {
            alcGetError(m_Device);
            m_Context = alcCreateContext(m_Device, nullptr);
        }
        if (m_Context == nullptr || alcMakeContextCurrent(m_Context) == ALC_FALSE)
        {
            CW_ENGINE_ERROR("OpenAL context creation failed. Device: {0}", defaultDeviceName);
            if (m_Context != nullptr)
                alcDestroyContext(m_Context);
            m_Context = nullptr;
            alcCloseDevice(m_Device);
            m_Device = nullptr;
            return;
        }

        ApplyGlobalSettings();
        RefreshEFXCapability();
    }

    bool AudioManager::IsContextCurrent() const { return m_Context != nullptr && alcGetCurrentContext() == m_Context; }

    bool AudioManager::EnsureContextCurrent()
    {
        if (!IsAvailable())
            return false;
        if (IsContextCurrent())
            return true;
        if (alcMakeContextCurrent(m_Context) != ALC_FALSE)
            return true;
        CW_ENGINE_ERROR("OpenAL context could not be made current; audio resources will be released with the context.");
        return false;
    }

    void AudioManager::RefreshEFXCapability()
    {
        const bool available = m_EFX.Load(m_Device);
        if (available)
        {
            if (!m_EFXAvailableReported)
            {
                CW_ENGINE_INFO("OpenAL EFX enabled: {0} auxiliary send(s), EAX reverb: {1}.", m_EFX.MaxAuxiliarySends,
                               m_EFX.HasEAXReverb);
                m_EFXAvailableReported = true;
            }
            return;
        }

        if (m_EFXFallbackReported)
            return;
        if (m_EFX.Status == EFXLoadStatus::MissingEntrypoint)
            CW_ENGINE_WARN("OpenAL EFX unavailable (missing {0}); effects are disabled, core playback remains available.",
                           m_EFX.MissingEntrypoint != nullptr ? m_EFX.MissingEntrypoint : "unknown entrypoint");
        else
            CW_ENGINE_WARN("OpenAL EFX unavailable ({0}); effects are disabled, core playback remains available.",
                           EFX::GetStatusName(m_EFX.Status));
        m_EFXFallbackReported = true;
    }

    void AudioManager::ApplyGlobalSettings()
    {
        if (!IsAvailable())
            return;
        alDopplerFactor(m_DopplerFactor);
        alSpeedOfSound(m_SpeedOfSound);
        SetDistanceModel(m_DistanceModel);
        if (m_Listener != nullptr)
            m_Listener->SetVolume(m_Volume);
    }

    void AudioManager::SetDopplerFactor(float factor)
    {
        m_DopplerFactor = std::max(0.0f, factor);
        if (IsAvailable())
            alDopplerFactor(m_DopplerFactor);
    }

    void AudioManager::SetSpeedOfSound(float speed)
    {
        m_SpeedOfSound = std::max(0.001f, speed);
        if (IsAvailable())
            alSpeedOfSound(m_SpeedOfSound);
    }

    void AudioManager::SetDistanceModel(AudioDistanceModel model)
    {
        m_DistanceModel = model;
        if (!IsAvailable())
            return;
        ALenum alModel = AL_INVERSE_DISTANCE_CLAMPED;
        switch (model)
        {
        case AudioDistanceModel::None:
            alModel = AL_NONE;
            break;
        case AudioDistanceModel::Inverse:
            alModel = AL_INVERSE_DISTANCE;
            break;
        case AudioDistanceModel::InverseClamped:
            alModel = AL_INVERSE_DISTANCE_CLAMPED;
            break;
        case AudioDistanceModel::Linear:
            alModel = AL_LINEAR_DISTANCE;
            break;
        case AudioDistanceModel::LinearClamped:
            alModel = AL_LINEAR_DISTANCE_CLAMPED;
            break;
        case AudioDistanceModel::Exponent:
            alModel = AL_EXPONENT_DISTANCE;
            break;
        case AudioDistanceModel::ExponentClamped:
            alModel = AL_EXPONENT_DISTANCE_CLAMPED;
            break;
        }
        alDistanceModel(alModel);
    }

    float AudioManager::GetGlobalSourceProgress(const String& name) const
    {
        const auto iterFind = m_ManualSources.find(name);
        if (iterFind != m_ManualSources.end() && iterFind->second->GetAudioClip())
        {
            const float length = iterFind->second->GetAudioClip()->GetLength();
            return length > 0.0f ? glm::clamp(iterFind->second->GetTime() / length, 0.0f, 1.0f) : 0.0f;
        }
        return 0;
    }

    void AudioManager::Play(const String& name, const AssetHandle<AudioClip>& clip, const glm::vec3& position, float volume)
    {
        if (!clip)
            return;
        Ref<AudioSource> source = CreateSource();
        source->SetClip(clip);
        Transform transform;
        transform.SetPosition(position);
        source->OnTransformChanged(transform);
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
        for (AudioSource* source : m_Sources)
            source->UpdateStreaming();

        // Remove finished one-shot sources in place. Rebuilding a second string-keyed
        // map here used to copy and rehash every live source on every audio update.
        for (auto iter = m_ManualSources.begin(); iter != m_ManualSources.end();)
        {
            if (iter->second->GetState() == AudioSourceState::Stopped)
                iter = m_ManualSources.erase(iter);
            else
                ++iter;
        }
    }

    AudioManager::~AudioManager()
    {
        if (m_Context != nullptr)
        {
            if (IsContextCurrent())
                alcMakeContextCurrent(nullptr);
            alcDestroyContext(m_Context);
            m_Context = nullptr;
        }

        if (m_Device != nullptr)
        {
            alcCloseDevice(m_Device);
            m_Device = nullptr;
        }
    }

    void AudioManager::SetVolume(float volume)
    {
        m_Volume = glm::clamp(volume, 0.0f, 1.0f);
        if (m_Listener != nullptr)
            m_Listener->SetVolume(m_Volume);
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

    bool AudioManager::SetActiveDevice(const AudioDevice& device)
    {
        if (!IsAvailable() || !EnsureContextCurrent())
            return false;
        if (device.Name == m_ActiveDevice.Name)
            return true;
        if (!device.Name.empty())
        {
            const auto match =
              std::find_if(m_Devices.begin(), m_Devices.end(), [&](const AudioDevice& candidate) { return candidate.Name == device.Name; });
            if (match == m_Devices.end())
            {
                CW_ENGINE_ERROR("Cannot select unknown OpenAL device: {0}", device.Name);
                return false;
            }
        }

        if (alcIsExtensionPresent(m_Device, "ALC_SOFT_reopen_device") == ALC_FALSE)
        {
            CW_ENGINE_ERROR("Cannot switch OpenAL devices safely: ALC_SOFT_reopen_device is unavailable.");
            return false;
        }

        using ReopenDeviceProc = ALCboolean(ALC_APIENTRY*)(ALCdevice*, const ALCchar*, const ALCint*);
        const auto reopenDevice = reinterpret_cast<ReopenDeviceProc>(alcGetProcAddress(m_Device, "alcReopenDeviceSOFT"));
        if (reopenDevice == nullptr)
        {
            CW_ENGINE_ERROR("Cannot switch OpenAL devices: alcReopenDeviceSOFT could not be loaded.");
            return false;
        }

        const ALCint attrs[] = { ALC_MAX_AUXILIARY_SENDS, 4, 0 };
        const ALCchar* name = device.Name.empty() ? nullptr : device.Name.c_str();
        if (reopenDevice(m_Device, name, attrs) == ALC_FALSE)
        {
            // A backend may support reopen but reject the optional EFX send-count attribute.
            if (reopenDevice(m_Device, name, nullptr) == ALC_FALSE)
            {
                CW_ENGINE_ERROR("OpenAL device switch failed. Device: {0}", device.Name);
                return false;
            }
        }

        m_ActiveDevice = device;
        ApplyGlobalSettings();
        RefreshEFXCapability();
        return true;
    }

    void AudioManager::SetActiveMixer(const AssetHandle<AudioMixer>& mixer) { m_ActiveMixer = mixer; }

    Ref<AudioBus> AudioManager::FindBus(const String& name) const
    {
        if (!m_ActiveMixer)
            return nullptr;
        return m_ActiveMixer->FindBus(name);
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

    bool AudioManager::WriteToOpenALBuffer(uint32_t bufferId, const uint8_t* samples, const AudioDataInfo& info)
    {
        if (!IsAvailable() || bufferId == 0 || samples == nullptr || info.NumSamples == 0 || info.SampleRate == 0)
            return false;

        const bool supportedChannels = info.NumChannels == 1 || info.NumChannels == 2 || info.NumChannels == 4 || info.NumChannels == 6 ||
                                       info.NumChannels == 7 || info.NumChannels == 8;
        if (!supportedChannels || AudioUtils::GetBufferSize(info.NumSamples, info.BitDepth) == 0)
        {
            CW_ENGINE_ERROR("Unsupported PCM format: {0} channels, {1}-bit.", info.NumChannels, info.BitDepth);
            return false;
        }

        const bool multichannel = info.NumChannels > 2;
        if (multichannel && !IsExtSupported("AL_EXT_MCFORMATS"))
        {
            CW_ENGINE_ERROR("OpenAL device does not support {0}-channel buffers.", info.NumChannels);
            return false;
        }

        const void* uploadData = samples;
        uint32_t uploadBitDepth = info.BitDepth;
        uint32_t uploadSize = AudioUtils::GetBufferSize(info.NumSamples, info.BitDepth);
        Vector<uint8_t> convertedBytes;
        Vector<float> convertedFloats;

        if (info.BitDepth == 8)
        {
            convertedBytes.resize(info.NumSamples);
            AudioUtils::ConvertSigned8ToUnsigned(samples, convertedBytes.data(), info.NumSamples);
            uploadData = convertedBytes.data();
        }
        else if (info.BitDepth > 16 && IsExtSupported("AL_EXT_float32"))
        {
            convertedFloats.resize(info.NumSamples);
            AudioUtils::ConvertToFloat(samples, info.BitDepth, convertedFloats.data(), info.NumSamples);
            uploadData = convertedFloats.data();
            uploadBitDepth = 32;
            uploadSize = info.NumSamples * sizeof(float);
        }
        else if (info.BitDepth > 16)
        {
            CW_ENGINE_WARN("OpenAL float buffers are unavailable. Converting {0}-bit PCM to 16-bit.", info.BitDepth);
            convertedBytes.resize(info.NumSamples * sizeof(int16_t));
            AudioUtils::ConvertBitDepth(samples, info.BitDepth, convertedBytes.data(), 16, info.NumSamples);
            uploadData = convertedBytes.data();
            uploadBitDepth = 16;
            uploadSize = static_cast<uint32_t>(convertedBytes.size());
        }

        const ALenum format = AudioUtils::GetOpenALFormat(info.NumChannels, uploadBitDepth);
        if (format == AL_NONE)
            return false;
        alGetError();
        alBufferData(bufferId, format, uploadData, static_cast<ALsizei>(uploadSize), static_cast<ALsizei>(info.SampleRate));
        return alGetError() == AL_NO_ERROR;
    }

} // namespace Crowny
