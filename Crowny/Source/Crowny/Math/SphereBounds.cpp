#include "cwpch.h"

#include "Crowny/Math/SphereBounds.h"

namespace Crowny
{

    SphereBounds::SphereBounds() : m_Center(0.0f), m_Radius(1.0f) {}
    SphereBounds::SphereBounds(const glm::vec3& center, float radius) : m_Center(center), m_Radius(radius) {}

} // namespace Crowny