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
        static glm::mat4 ComposeMatrix(const glm::vec3& position, const glm::quat& rotation, const glm::vec3& scale);
    };

    class Transform
    {
    public:
        Transform();
        Transform(const glm::vec3& position, const glm::quat& rotation, const glm::vec3& scale);
        void SetPosition(const glm::vec3& position) { m_Position = position; }
        const glm::vec3 GetPosition() const { return m_Position; }
        void SetRotation(const glm::quat& rotation) { m_Rotation = rotation; }
        const glm::quat& GetRotation() const { return m_Rotation; }
        void SetScale(const glm::vec3& scale) { m_Scale = scale; }
        const glm::vec3& GetScale() const { return m_Scale; }

        void SetWorldPosition(const glm::vec3& position, const Transform& parentTransform);
        void SetWorldRotation(const glm::quat& rotation, const Transform& parentTransform);
        void SetWorldScale(const glm::vec3& scale, const Transform& parentTransform);

        glm::mat4 GetMatrix() const;
        glm::mat4 GetInvMatrix() const;
        void MakeLocal(const Transform& parentTransform);
        void MakeWorld(const Transform& parentTransform);

    private:
        glm::vec3 m_Position;
        glm::quat m_Rotation;
        glm::vec3 m_Scale;
    };
} // namespace Crowny