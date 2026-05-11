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
        void SetActiveDevice(const AudioDevice& device);
        const AudioDevice& GetDefaultDevice() const { return m_DefaultDevice; }
        const Vector<AudioDevice>& GetAllDevices() const { return m_Devices; }
        void WriteToOpenALBuffer(uint32_t bufferId, uint8_t* samples, const AudioDataInfo& info);

        ALCdevice* GetDevice() const { return m_Device; }
        void SetContext(ALCcontext* context) { m_Context = context; }

        Ref<AudioListener> CreateListener();
        Ref<AudioSource> CreateSource();
        Ref<AudioClip> CreateClip();

        // EFX accessors. When EFX is unavailable, IsEFXAvailable() returns false and the buses/effects
        // become no-ops at the OpenAL layer, while bus gain still propagates to sources.
        const EFX& GetEFX() const { return m_EFX; }
        bool IsEFXAvailable() const { return m_EFX.Available; }

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
        void OnStartUp() override;
        void OnShutdown() override;

    private:
        bool IsExtSupported(const String& ext) const;
        ALCcontext* GetContext() const;

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
        UnorderedMap<String, Ref<AudioSource>> m_TempSources;

        EFX m_EFX;
        AssetHandle<AudioMixer> m_ActiveMixer;

        float m_DopplerFactor = 1.0f;
        float m_SpeedOfSound = 343.3f;
        AudioDistanceModel m_DistanceModel = AudioDistanceModel::InverseClamped;
    };

    extern AudioManager* gAudioManager;

} // namespace Crowny