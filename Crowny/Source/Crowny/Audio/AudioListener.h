#pragma once

namespace Crowny
{

    struct TransformComponent;

    class AudioListener
    {
    public:
        AudioListener();
        ~AudioListener();
        void SetTransform(const TransformComponent& transform);
        void SetVelocity(const glm::vec3& velocity);
        void SetVolume(float volume);

    private:
    };

} // namespace Crowny