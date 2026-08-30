#pragma once

#include "Crowny/Assets/AssetHandle.h"
#include "Crowny/Audio/AudioSource.h"
#include "Crowny/Audio/EFXLoader.h"
#include "Crowny/Common/Module.h"

#include <AL/alc.h>

namespace Crowny
{

    class AudioMixer;
    class AudioBus;

    struct AudioDevice
    {
        String Name;
    };

    // Mirrors OpenAL's distance model enum but in a stable order we serialize. The clamped
    // variants cap volume at the min/max distance, which is almost always what games want.
    enum class AudioDistanceModel : uint8_t
    {
        None = 0,
        Inverse = 1,
        InverseClamped = 2, // OpenAL default
        Linear = 3,
        LinearClamped = 4,
        Exponent = 5,
        ExponentClamped = 6,
    };

    class AudioManager : public Module<AudioManager>
    {
    public:
        AudioManager();
        ~AudioManager();
        void SetVolume(float volume);
        float GetVolume() const;
        void SetPaused(bool paused);
        bool IsPaused() const { return m_IsPaused; }
        bool SetActiveDevice(const AudioDevice& device);
        const AudioDevice& GetDefaultDevice() const { return m_DefaultDevice; }
        const AudioDevice& GetActiveDevice() const { return m_ActiveDevice; }
        const Vector<AudioDevice>& GetAllDevices() const { return m_Devices; }
        bool WriteToOpenALBuffer(uint32_t bufferId, const uint8_t* samples, const AudioDataInfo& info);
        bool IsAvailable() const { return m_Device != nullptr && m_Context != nullptr; }
        bool IsContextCurrent() const;

        ALCdevice* GetDevice() const { return m_Device; }
        void SetContext(ALCcontext* context) { m_Context = context; }

        Ref<AudioListener> CreateListener();
        Ref<AudioSource> CreateSource();
        Ref<AudioClip> CreateClip();

        // EFX accessors. When EFX is unavailable, IsEFXAvailable() returns false and the buses/effects
        // become no-ops at the OpenAL layer, while bus gain still propagates to sources.
        const EFX& GetEFX() const { return m_EFX; }
        bool IsEFXAvailable() const { return m_EFX.Available; }
        EFXLoadStatus GetEFXStatus() const { return m_EFX.Status; }

        // Sets the currently active audio mixer. Sources without an explicit bus will route to the
        // master bus of this mixer. Passing a null handle reverts to direct-output (no bus routing).
        void SetActiveMixer(const AssetHandle<AudioMixer>& mixer);
        const AssetHandle<AudioMixer>& GetActiveMixer() const { return m_ActiveMixer; }
        Ref<AudioBus> FindBus(const String& name) const;

        void SetDopplerFactor(float factor);
        float GetDopplerFactor() const { return m_DopplerFactor; }
        void SetSpeedOfSound(float speed);
        float GetSpeedOfSound() const { return m_SpeedOfSound; }
        void SetDistanceModel(AudioDistanceModel model);
        AudioDistanceModel GetDistanceModel() const { return m_DistanceModel; }

        float GetGlobalSourceProgress(const String& name) const;

        void RegisterListener(AudioListener* listener);
        void UnregisterListener(AudioListener* listener);
        void RegisterSource(AudioSource* source);
        void UnregisterSource(AudioSource* source);

        void Play(const String& name, const AssetHandle<AudioClip>& clip, const glm::vec3& position = glm::vec3(0.0f), float volume = 1.0f);
        void OnUpdate();
        void StopManualSources();

    protected:
        void OnShutdown() override;

    private:
        bool IsExtSupported(const String& ext) const;
        ALCcontext* GetContext() const;
        bool WriteToOpenALBuffer(uint32_t bufferId, const uint8_t* samples, const AudioDataInfo& info, AudioStreamScratch& scratch);
        void ApplyGlobalSettings();
        bool EnsureContextCurrent();
        void RefreshPCMCapabilities();
        void RefreshEFXCapability();

    private:
        float m_Volume = 1.0f;
        bool m_IsPaused = false;

        ALCdevice* m_Device = nullptr;
        Vector<AudioDevice> m_Devices;
        AudioDevice m_DefaultDevice;
        AudioDevice m_ActiveDevice;

        AudioListener* m_Listener = nullptr;
        ALCcontext* m_Context = nullptr;
        Set<AudioSource*> m_Sources;

        UnorderedMap<String, Ref<AudioSource>> m_ManualSources;

        EFX m_EFX;
        bool m_EFXFallbackReported = false;
        bool m_EFXAvailableReported = false;
        AssetHandle<AudioMixer> m_ActiveMixer;

        AudioPCMCapabilities m_PCMCapabilities;
        bool m_IntegerPCMFallbackReported = false;

        float m_DopplerFactor = 1.0f;
        float m_SpeedOfSound = 343.3f;
        AudioDistanceModel m_DistanceModel = AudioDistanceModel::InverseClamped;

        friend class AudioSource;
    };
} // namespace Crowny
