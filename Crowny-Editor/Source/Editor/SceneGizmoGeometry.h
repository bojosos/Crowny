#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

namespace Crowny::SceneGizmos
{
    // Editor camera matrices use OpenGL clip coordinates on both rendering backends.
    inline std::optional<glm::vec3> ProjectIcon(const glm::vec3& position, const glm::mat4& viewProjection, const glm::vec4& bounds)
    {
        const glm::vec4 clip = viewProjection * glm::vec4(position, 1.0f);
        if (!std::isfinite(clip.w) || clip.w <= 0.00001f || !std::isfinite(bounds.x) || !std::isfinite(bounds.y) || !std::isfinite(bounds.z) ||
            !std::isfinite(bounds.w) || bounds.z <= bounds.x || bounds.w <= bounds.y)
            return std::nullopt;
        const glm::vec3 ndc = glm::vec3(clip) / clip.w;
        if (!std::isfinite(ndc.x) || !std::isfinite(ndc.y) || !std::isfinite(ndc.z) || ndc.x < -1.0f || ndc.x > 1.0f || ndc.y < -1.0f ||
            ndc.y > 1.0f || ndc.z < -1.0f || ndc.z > 1.0f)
            return std::nullopt;
        return glm::vec3(bounds.x + (ndc.x + 1.0f) * 0.5f * (bounds.z - bounds.x), bounds.y + (1.0f - ndc.y) * 0.5f * (bounds.w - bounds.y), ndc.z);
    }

    // A spherical cap stays finite for audio cones wider than 180 degrees.
    inline glm::vec3 ConePoint(float radius, float halfAngle, float azimuth)
    {
        return radius * glm::vec3(std::sin(halfAngle) * std::cos(azimuth), std::sin(halfAngle) * std::sin(azimuth), -std::cos(halfAngle));
    }

    inline std::array<glm::vec3, 8> FrustumCorners(bool perspective, float verticalFov, float orthoSize, float aspect, float nearClip, float farClip)
    {
        std::array<glm::vec3, 8> corners;
        for (size_t plane = 0; plane < 2; ++plane)
        {
            const float distance = plane == 0 ? nearClip : farClip;
            const float height = perspective ? distance * std::tan(verticalFov * 0.5f) : orthoSize * 0.5f;
            const float width = height * aspect;
            corners[plane * 4 + 0] = { -width, -height, -distance };
            corners[plane * 4 + 1] = { width, -height, -distance };
            corners[plane * 4 + 2] = { width, height, -distance };
            corners[plane * 4 + 3] = { -width, height, -distance };
        }
        return corners;
    }
} // namespace Crowny::SceneGizmos
