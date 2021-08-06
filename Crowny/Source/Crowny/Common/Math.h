#pragma once

#include <glm/glm.hpp>

namespace Crowny
{
    class Math
    {
    public:
        static float Intbound(float s, float ds);
        static float Signum(float x);
        static float Mod(float value, int modulus);
        static glm::vec3 GetForwardDirection(const glm::vec3& rotation);
        static glm::vec3 GetRightDirection(const glm::vec3& rotation);
        static bool DecomposeMatrix(const glm::mat4& transform, glm::vec3& outTransform, glm::vec3& outRotation,
                                    glm::vec3& outScale);
    };
} // namespace Crowny