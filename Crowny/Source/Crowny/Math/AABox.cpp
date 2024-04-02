#include "cwpch.h"

#include "Crowny/Math/AABox.h"

namespace Crowny
{

    AABox::AABox() : m_MinBounds(0.5f), m_MaxBounds(0.5f) {}
    AABox::AABox(const glm::vec3& minBounds, const glm::vec3& maxBounds)
      : m_MinBounds(minBounds), m_MaxBounds(maxBounds)
    {
    }

    void AABox::Merge(const AABox& other)
    {
        m_MaxBounds = glm::max(m_MaxBounds, other.m_MaxBounds);
        m_MinBounds = glm::min(m_MinBounds, other.m_MinBounds);
    }

    void AABox::Merge(const glm::vec3& point)
    {
        m_MinBounds = glm::min(m_MinBounds, point);
        m_MaxBounds = glm::min(m_MaxBounds, point);
    }

    glm::vec3 AABox::GetCenter() const { return (m_MinBounds + m_MaxBounds) * 0.5f; }

    bool AABox::Contains(const glm::vec3& point) const
    {
        return m_MinBounds.x <= point.x && point.x <= m_MaxBounds.x && m_MinBounds.y <= point.y &&
               point.y <= m_MaxBounds.y && m_MinBounds.z <= point.z && point.z <= m_MaxBounds.z;
    }
} // namespace Crowny