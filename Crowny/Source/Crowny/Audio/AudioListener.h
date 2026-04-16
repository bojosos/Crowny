#pragma once

#include "Crowny/Common/RefCounted.h"

namespace Crowny
{

    class TransformComponent;

    class AudioListener : public RefCounted
    {
    public:
        AudioListener();
        ~AudioListener();
        void OnTransformChanged(const Transform& transform);
        void SetVelocity(const glm::vec3& velocity);
        void SetVolume(float volume);
    };

} // namespace Crowny