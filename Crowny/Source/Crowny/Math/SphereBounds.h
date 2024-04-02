#pragma once

#include <glm/gtx/norm.hpp>

namespace Crowny
{
    class SphereBounds
    {
    public:
        SphereBounds();
        SphereBounds(const glm::vec3& center, float radius);

        float GetRadius() const { return m_Radius; }
        const glm::vec3& GetCenter() const { return m_Center; }
        bool Contains(const glm::vec3& point) const { return glm::length2(point - m_Center) <= m_Radius * m_Radius; }

    private:
        glm::vec3 m_Center;
        float m_Radius;
    };
} // namespace Crowny