#pragma once

namespace Crowny
{
    class AABox
    {
    public:
        AABox();
        AABox(const glm::vec3& minBounds, const glm::vec3& maxBounds);

        const glm::vec3& GetMin() const { return m_MinBounds; }
        const glm::vec3& GetMax() const { return m_MaxBounds; }
        void Merge(const AABox& other);
        void Merge(const glm::vec3& point);
        glm::vec3 GetCenter() const;
        void SetExtents(const glm::vec3& minBounds, const glm::vec3& maxBounds);
        bool Contains(const glm::vec3& point) const;

    private:
        glm::vec3 m_MinBounds;
        glm::vec3 m_MaxBounds;
    };
} // namespace Crowny