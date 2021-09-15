#pragma once

#include "Crowny/Audio/AudioListener.h"
#include "Crowny/Audio/AudioSource.h"
#include "Crowny/Common/Module.h"

#include <AL/alc.h>

namespace Crowny
{

    struct AudioDevice
    {
        std::string Name;
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
        const std::vector<AudioDevice>& GetAllDevices() const { return m_Devices; }
        void WriteToOpenALBuffer(uint32_t bufferId, uint8_t* samples, const AudioDataInfo& info);

        ALCdevice* GetDevice() { return m_Device; }
        void SetContext(ALCcontext* context) { m_Context = context; }

        Ref<AudioListener> CreateListener();
        Ref<AudioSource> CreateSource();
        Ref<AudioClip> CreateClip();

        void RegisterListener(AudioListener* listener);
        void UnregisterListener(AudioListener* listener);
        void RegisterSource(AudioSource* source);
        void UnregisterSource(AudioSource* source);

    private:
        bool IsExtSupported(const std::string& ext) const;
        ALCcontext* GetContext() const;

    private:
        float m_Volume = 1.0f;
        bool m_IsPaused = false;

        ALCdevice* m_Device = nullptr;
        std::vector<AudioDevice> m_Devices;
        AudioDevice m_DefaultDevice;
        AudioDevice m_ActiveDevice;

        AudioListener* m_Listener = nullptr;
        ALCcontext* m_Context = nullptr;
        std::unordered_set<AudioSource*> m_Sources;
    };

    AudioManager& gAudio();

} // namespace Crowny