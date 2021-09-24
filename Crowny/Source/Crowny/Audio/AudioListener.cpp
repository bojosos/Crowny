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
        float globalVolume = gAudio().GetVolume();
        alListenerf(AL_GAIN, globalVolume);
        alListener3f(AL_POSITION, 0.0f, 0.0f, 0.0f);
        alListener3f(AL_VELOCITY, 0.0f, 0.0f, 0.0f);
        std::array<float, 6> orientation = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
        alListenerfv(AL_ORIENTATION, orientation.data());
    }

    AudioListener::~AudioListener() { gAudio().UnregisterListener(this); }

    void AudioListener::SetTransform(const TransformComponent& transform)
    {
        alListener3f(AL_POSITION, transform.Position.x, transform.Position.y, transform.Position.z);
        glm::mat4 tran = transform.GetTransform();
        std::array<float, 6> orientation = {
            -tran[2].x, -tran[2].y, -tran[2].z, tran[1].x, tran[1].y, tran[1].z
        }; // TODO: is this math correct?
        alListenerfv(AL_ORIENTATION, orientation.data());
    }

    void AudioListener::SetVelocity(const glm::vec3& velocity)
    {
        alListener3f(AL_VELOCITY, velocity.x, velocity.y, velocity.z);
    }

    void AudioListener::SetVolume(float volume) { alListenerf(AL_GAIN, volume); }

} // namespace Crowny
