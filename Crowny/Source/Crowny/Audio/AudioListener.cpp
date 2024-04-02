#include "cwpch.h"

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
        gAudio().RegisterListener(this);
        const float globalVolume = gAudio().GetVolume();
        alListenerf(AL_GAIN, globalVolume);
        alListener3f(AL_POSITION, 0.0f, 0.0f, 0.0f);
        alListener3f(AL_VELOCITY, 0.0f, 0.0f, 0.0f);
        const std::array<float, 6> orientation = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
        alListenerfv(AL_ORIENTATION, orientation.data());
    }

    AudioListener::~AudioListener() { gAudio().UnregisterListener(this); }

    void AudioListener::OnTransformChanged(const Transform& transform)
    {
        const glm::vec3& position = transform.GetPosition();
        alListener3f(AL_POSITION, position.x, position.y, position.z);
        const glm::mat4& worldTransform = transform.GetMatrix();
        std::array<float, 6> orientation = {
            -worldTransform[2].x, -worldTransform[2].y, -worldTransform[2].z,
            worldTransform[1].x,  worldTransform[1].y,  worldTransform[1].z
        }; // TODO: is this math correct? It's wrong surprise surprise
        alListenerfv(AL_ORIENTATION, orientation.data());
    }

    void AudioListener::SetVelocity(const glm::vec3& velocity)
    {
        alListener3f(AL_VELOCITY, velocity.x, velocity.y, velocity.z);
    }

    void AudioListener::SetVolume(float volume) { alListenerf(AL_GAIN, volume); }

} // namespace Crowny
