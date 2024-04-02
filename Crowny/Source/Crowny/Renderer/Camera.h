#pragma once

#include <glm/glm.hpp>

namespace Crowny
{

    class Camera
    {
    public:
        Camera() = default;
        Camera(const glm::mat4& proj) : m_Projection(proj) {}

        virtual ~Camera() = default;

        virtual glm::vec3 GetPosition() const { return glm::vec3(0.0f); }
        const glm::mat4& GetProjection() const { return m_Projection; }

    protected:
        glm::mat4 m_Projection = glm::mat4(1.0f);
    };

} // namespace Crowny
