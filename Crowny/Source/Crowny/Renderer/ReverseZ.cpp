#include "cwpch.h"

#include "Crowny/Renderer/ReverseZ.h"

namespace Crowny
{
    glm::mat4 ReverseZ::Perspective(float verticalFovRadians, float aspectRatio, float nearPlane, float farPlane)
    {
        verticalFovRadians = std::clamp(verticalFovRadians, glm::radians(1.0f), glm::radians(179.0f));
        aspectRatio = std::max(aspectRatio, 0.0001f);
        nearPlane = std::max(nearPlane, 0.0001f);
        const float reciprocalTanHalfFov = 1.0f / std::tan(verticalFovRadians * 0.5f);

        glm::mat4 projection(0.0f);
        projection[0][0] = reciprocalTanHalfFov / aspectRatio;
        projection[1][1] = reciprocalTanHalfFov;
        projection[2][3] = -1.0f;
        if (IsInfinite(farPlane))
        {
            projection[3][2] = nearPlane;
        }
        else
        {
            farPlane = std::max(farPlane, nearPlane + 0.0001f);
            projection[2][2] = nearPlane / (farPlane - nearPlane);
            projection[3][2] = nearPlane * farPlane / (farPlane - nearPlane);
        }
        return projection;
    }

    float ReverseZ::LinearDepth(float deviceDepth, float nearPlane, float farPlane)
    {
        nearPlane = std::max(nearPlane, 0.0001f);
        deviceDepth = std::clamp(deviceDepth, 0.0f, 1.0f);
        if (IsInfinite(farPlane))
            return deviceDepth > 0.0f ? nearPlane / deviceDepth : std::numeric_limits<float>::infinity();
        farPlane = std::max(farPlane, nearPlane + 0.0001f);
        const float coefficient = nearPlane / (farPlane - nearPlane);
        const float numerator = nearPlane * farPlane / (farPlane - nearPlane);
        return numerator / std::max(deviceDepth + coefficient, std::numeric_limits<float>::min());
    }
} // namespace Crowny
