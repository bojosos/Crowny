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

    private:
        glm::vec3 m_Velocity{ 0.0f };
        glm::vec3 m_PrevPosition{ 0.0f };
        bool m_HasPrevPosition = false;
    };

} // namespace Crowny