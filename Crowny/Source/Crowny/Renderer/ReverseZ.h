#pragma once

#include "Crowny/Common/Types.h"

#include <cmath>
#include <limits>

namespace Crowny
{
    class ReverseZ
    {
    public:
        // Engine cameras use [-1, 1] clip depth. Shaders on both backends consume
        // [0, 1], with near=1 when reverseDepth is enabled.
        static glm::mat4 ConvertClipDepth(const glm::mat4& projection, bool reverseDepth);
        static glm::mat4 Perspective(float verticalFovRadians, float aspectRatio, float nearPlane,
                                     float farPlane = std::numeric_limits<float>::infinity());
        static float LinearDepth(float deviceDepth, float nearPlane, float farPlane = std::numeric_limits<float>::infinity());
        static bool IsInfinite(float farPlane) { return !std::isfinite(farPlane); }
    };
} // namespace Crowny
