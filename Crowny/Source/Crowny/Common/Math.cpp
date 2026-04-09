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

    bool Math::DecomposeMatrix(const glm::mat4& transform, glm::vec3& translation, glm::quat& rotation, glm::vec3& scale)
    {
        glm::vec3 skew;
        glm::vec4 perspective;
        bool res = glm::decompose(transform, scale, rotation, translation, skew, perspective);
        rotation = glm::conjugate(rotation);
        return res;
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
    glm::mat4 Transform::GetInvMatrix() const { return glm::inverse(Math::ComposeMatrix(m_Position, m_Rotation, m_Scale)); }

    void Transform::MakeLocal(const Transform& parentTransform)
    {
        SetWorldPosition(m_Position, parentTransform);
        SetWorldRotation(m_Rotation, parentTransform);
        SetWorldScale(m_Scale, parentTransform);
    }

    void Transform::MakeWorld(const Transform& parentTransform)
    {
        glm::mat4 parentMatrix = parentTransform.GetMatrix();
        glm::mat4 localMatrix = GetMatrix();
        glm::mat4 worldMatrix = parentMatrix * localMatrix;

        Math::DecomposeMatrix(worldMatrix, m_Position, m_Rotation, m_Scale);
    }

    void Transform::SetWorldPosition(const glm::vec3& position, const Transform& parentTransform)
    {
        glm::mat4 invParent = glm::inverse(parentTransform.GetMatrix());
        glm::vec4 localPos = invParent * glm::vec4(position, 1.0f);
        m_Position = glm::vec3(localPos);
    }

    void Transform::SetWorldRotation(const glm::quat& rotation, const Transform& parentTransform)
    {
        glm::quat invParentRot = glm::inverse(parentTransform.GetRotation());
        m_Rotation = invParentRot * rotation;
    }

    void Transform::SetWorldScale(const glm::vec3& scale, const Transform& parentTransform)
    {
        glm::vec3 parentScale = parentTransform.GetScale();
        if (parentScale.x != 0.0f && parentScale.y != 0.0f && parentScale.z != 0.0f)
            m_Scale = scale / parentScale;
    }
} // namespace Crowny