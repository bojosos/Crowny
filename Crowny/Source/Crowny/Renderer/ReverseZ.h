#pragma once

#include "Crowny/Common/Types.h"

#include <cmath>
#include <limits>

namespace Crowny
{
    class ReverseZ
    {
    public:
        static glm::mat4 Perspective(float verticalFovRadians, float aspectRatio, float nearPlane,
                                     float farPlane = std::numeric_limits<float>::infinity());
        static float LinearDepth(float deviceDepth, float nearPlane,
                                 float farPlane = std::numeric_limits<float>::infinity());
        static bool IsInfinite(float farPlane) { return !std::isfinite(farPlane); }
    };
} // namespace Crowny
