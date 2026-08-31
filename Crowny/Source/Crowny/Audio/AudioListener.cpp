#include "cwpch.h"

#include "Crowny/Application/Application.h"
#include "Crowny/Audio/AudioListener.h"
#include "Crowny/Audio/AudioManager.h"
#include "Crowny/Ecs/Components.h"

#include <AL/al.h>
#include <AL/alc.h>
#include <glm/glm.hpp>
#include <glm/gtx/rotate_vector.hpp>

namespace Crowny
{

    AudioListener::AudioListener()
    {
        if (AudioManager::TryGet() == nullptr || !AudioManager::TryGet()->IsAvailable())
            return;
        AudioManager::TryGet()->RegisterListener(this);
        const float globalVolume = AudioManager::TryGet()->GetVolume();
        alListenerf(AL_GAIN, globalVolume);
        alListener3f(AL_POSITION, 0.0f, 0.0f, 0.0f);
        alListener3f(AL_VELOCITY, 0.0f, 0.0f, 0.0f);
        const std::array<float, 6> orientation = { 0.0f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f };
        alListenerfv(AL_ORIENTATION, orientation.data());
    }

    AudioListener::~AudioListener()
    {
        if (AudioManager::TryGet())
            AudioManager::TryGet()->UnregisterListener(this);
    }

    void AudioListener::OnTransformChanged(const Transform& transform)
    {
        if (AudioManager::TryGet() == nullptr || !AudioManager::TryGet()->IsAvailable())
            return;
        const glm::vec3 position = transform.GetPosition();
        alListener3f(AL_POSITION, position.x, position.y, position.z);

        const glm::mat4 worldTransform = transform.GetMatrix();
        const glm::vec3 forward = glm::normalize(-glm::vec3(worldTransform[2]));
        const glm::vec3 up = glm::normalize(glm::vec3(worldTransform[1]));
        const std::array<float, 6> orientation = { forward.x, forward.y, forward.z, up.x, up.y, up.z };
        alListenerfv(AL_ORIENTATION, orientation.data());

        if (!m_HasPrevPosition)
        {
            m_PrevPosition = position;
            m_HasPrevPosition = true;
            return;
        }

        const Application* application = Application::TryGet();
        const float dt = application != nullptr ? application->GetTime().GetUnscaledDeltaTime() : 0.0f;
        if (dt > 1e-6f)
        {
            m_Velocity = (position - m_PrevPosition) / dt;
            alListener3f(AL_VELOCITY, m_Velocity.x, m_Velocity.y, m_Velocity.z);
            m_PrevPosition = position;
        }
    }

    void AudioListener::SetVelocity(const glm::vec3& velocity)
    {
        m_Velocity = velocity;
        if (AudioManager::TryGet() && AudioManager::TryGet()->IsAvailable())
            alListener3f(AL_VELOCITY, velocity.x, velocity.y, velocity.z);
    }

    void AudioListener::SetVolume(float volume)
    {
        if (AudioManager::TryGet() && AudioManager::TryGet()->IsAvailable())
            alListenerf(AL_GAIN, glm::clamp(volume, 0.0f, 1.0f));
    }

} // namespace Crowny
