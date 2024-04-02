#include "cwpch.h"

#include "Crowny/Common/Math.h"
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/ext/matrix_transform.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/quaternion.hpp>

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

    glm::mat4 Math::ComposeMatrix(const glm::vec3& position, const glm::quat& rotation, const glm::vec3& scale)
    {
        glm::mat4 rotationMatrix = glm::toMat4(glm::quat(rotation));
        return glm::translate(glm::mat4(1.0f), position) * rotationMatrix * glm::scale(glm::mat4(1.0f), scale);
    }

    Transform::Transform() : m_Position(0.0f), m_Rotation(1.0f, 0.0f, 0.0f, 0.0f), m_Scale(1.0f) {}

    Transform::Transform(const glm::vec3& position, const glm::quat& rotation, const glm::vec3& scale)
      : m_Position(position), m_Rotation(rotation), m_Scale(scale)
    {
    }

    glm::mat4 Transform::GetMatrix() const { return Math::ComposeMatrix(m_Position, m_Rotation, m_Scale); }

    // TODO: There should be a way faster way of doing this.
    glm::mat4 Transform::GetInvMatrix() const
    {
        return glm::inverse(Math::ComposeMatrix(m_Position, m_Rotation, m_Scale));
    }

    void Transform::MakeLocal(const Transform& parentTransform)
    {
        SetWorldPosition(m_Position, parentTransform);
        SetWorldRotation(m_Rotation, parentTransform);
        SetWorldPosition(m_Scale, parentTransform);
    }

    void Transform::MakeWorld(const Transform& parentTransform)
    {
        const glm::quat parentRotation = parentTransform.GetRotation();
        m_Rotation = parentRotation * m_Rotation;
        const glm::vec3 parentScale = parentTransform.GetScale();
        m_Scale = parentScale * m_Scale;
        m_Position = glm::rotate(parentRotation, parentScale * m_Position);
        m_Position += parentTransform.GetPosition();
    }

    void Transform::SetWorldPosition(const glm::vec3& position, const Transform& parentTransform)
    {
        glm::vec3 invScale = parentTransform.GetScale();
        if (invScale.x != 0.0f)
            invScale.x = 1.0f / invScale.x;
        if (invScale.y != 0.0f)
            invScale.y = 1.0f / invScale.y;
        if (invScale.z != 0.0f)
            invScale.z = 1.0f / invScale.z;
        glm::quat invRotation = glm::inverse(parentTransform.GetRotation());
        m_Position = glm::rotate(invRotation, position - parentTransform.GetPosition()) * invScale;
    }

    void Transform::SetWorldRotation(const glm::quat& rotation, const Transform& parentTransform)
    {
        // Is this the right order of operations?
        glm::quat invRotation = glm::inverse(parentTransform.GetRotation());
        m_Rotation = invRotation * rotation;
    }

    void Transform::SetWorldScale(const glm::vec3& scale, const Transform& parentTransform)
    {
        /*
        const glm::mat4& parentMatrix = parentTransform.GetMatrix();
        glm::mat3 rotationScale = parentMatrix;
        rotationScale = glm::inverse(rotationScale);

        glm::mat3 identity(1.0f);
        glm::mat3 scaleMatrix;
        scaleMatrix[0] = identity[0] * scale[0];
        scaleMatrix[1] = identity[1] * scale[1];
        scaleMatrix[2] = identity[2] * scale[2];
        glm::quat rotation;
        glm::vec3 scale;
        glm::decompose(scaleMatrix, scale, rotation)
        */
        const glm::mat4& parentMatrix = parentTransform.GetMatrix();
        glm::vec3 parentScale;
        parentScale.x = glm::length(glm::vec3(parentMatrix[0]));
        parentScale.y = glm::length(glm::vec3(parentMatrix[1]));
        parentScale.z = glm::length(glm::vec3(parentMatrix[2]));

        if (parentScale.x != 0.0f && parentScale.y != 0.0f && parentScale.z != 0.0f)
            m_Scale = scale / parentScale;
    }
} // namespace Crowny