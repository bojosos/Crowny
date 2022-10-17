#include "cwpch.h"

#include "Crowny/Common/Math.h"
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>

namespace Crowny
{
    float Math::Intbound(float s, float ds)
    {
        if (ds < 0.0f)
        {
            return Intbound(-s, -ds);
        }
        else
        {
            s = Mod(s, 1);
            return (1.0f - s) / (float)ds;
        }
    }

    float Math::Signum(float x) { return x > 0.0f ? 1.0f : x < 0.0f ? -1.0f : 0.0f; }

    float Math::Mod(float value, int modulus) { return (float)fmod((float)fmod(value, modulus) + modulus, modulus); }

    glm::vec3 Math::GetForwardDirection(const glm::vec3& rotation)
    {
        float yaw = glm::radians(rotation.y + 90);
        float pitch = glm::radians(rotation.x);
        float x = glm::cos(yaw) * glm::cos(pitch);
        float y = glm::sin(pitch);
        float z = glm::cos(pitch) * glm::sin(yaw);

        return { -x, -y, -z };
    }

    glm::vec3 Math::GetRightDirection(const glm::vec3& rotation)
    {
        float yaw = glm::radians(rotation.y);
        float x = glm::cos(yaw);
        float y = 0;
        float z = glm::sin(yaw);

        return { x, y, z };
    }

    bool Math::DecomposeMatrix(const glm::mat4& transform, glm::vec3& translation, glm::vec3& rotation,
                               glm::vec3& scale)
    {
        using namespace glm;

        mat4 LocalMatrix(transform);

        if (epsilonEqual(LocalMatrix[3][3], 0.0f, epsilon<float>()))
            return false;

        if (epsilonNotEqual(LocalMatrix[0][3], 0.0f, epsilon<float>()) ||
            epsilonNotEqual(LocalMatrix[1][3], 0.0f, epsilon<float>()) ||
            epsilonNotEqual(LocalMatrix[2][3], 0.0f, epsilon<float>()))
        {
            LocalMatrix[0][3] = LocalMatrix[1][3] = LocalMatrix[2][3] = 0.0f;
            LocalMatrix[3][3] = 1.0f;
        }

        translation = vec3(LocalMatrix[3]);
        LocalMatrix[3] = vec4(0, 0, 0, LocalMatrix[3].w);

        vec3 Row[3];
        for (length_t i = 0; i < 3; ++i)
            for (length_t j = 0; j < 3; ++j)
                Row[i][j] = LocalMatrix[i][j];

        scale.x = length(Row[0]);
        Row[0] = detail::scale(Row[0], 1.0f);
        scale.y = length(Row[1]);
        Row[1] = detail::scale(Row[1], 1.0f);
        scale.z = length(Row[2]);
        Row[2] = detail::scale(Row[2], 1.0f);

        rotation.y = asin(-Row[0][2]);
        if (cos(rotation.y) != 0)
        {
            rotation.x = atan2(Row[1][2], Row[2][2]);
            rotation.z = atan2(Row[0][1], Row[0][0]);
        }
        else
        {
            rotation.x = atan2(-Row[2][0], Row[1][1]);
            rotation.z = 0;
        }

        return true;
    }
} // namespace Crowny