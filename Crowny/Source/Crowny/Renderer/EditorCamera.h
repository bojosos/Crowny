#pragma once

#include "Crowny/Common/Timestep.h"
#include "Crowny/Events/MouseEvent.h"
#include "Crowny/Renderer/Camera.h"

#include <glm/glm.hpp>

namespace Crowny
{

    class EditorCamera : public Camera
    {
    public:
        EditorCamera() = default;
        EditorCamera(float fov, float aspectRatio, float nearPlane, float farPlane);

        void OnUpdate(Timestep ts);
        void OnEvent(Event& e);

        float GetDistance() const { return m_Distance; }

        void SetViewportSize(float width, float height)
        {
            if (!std::isfinite(width) || !std::isfinite(height) || width <= 0.0f || height <= 0.0f)
                return;
            m_ViewportWidth = width;
            m_ViewportHeight = height;
            UpdateProjection();
        }

        const glm::mat4& GetViewMatrix() const { return m_ViewMatrix; }
        glm::mat4 GetViewProjection() const { return m_Projection * m_ViewMatrix; }

        glm::vec3 GetUpDirection() const;
        glm::vec3 GetRightDirection() const;
        glm::vec3 GetForwardDirection() const;
        virtual glm::vec3 GetPosition() const { return m_Position; }
        glm::quat GetOrientation() const;

        void SetPosition(const glm::vec3& position) { m_Position = position; }
        void SetFocalPoint(const glm::vec3& focalPoint) { m_FocalPoint = focalPoint; }
        void SetYaw(float yaw) { m_Yaw = yaw; }
        void SetRoll(float roll) { m_Roll = roll; }
        void SetPitch(float pitch) { m_Pitch = pitch; }
        void SetDistance(float distance)
        {
            if (std::isfinite(distance))
                m_Distance = std::max(distance, 0.1f);
        }

        float GetPitch() const { return m_Pitch; }
        float GetYaw() const { return m_Yaw; }
        float GetRoll() const { return m_Roll; }

        const glm::vec3& GetFocalPoint() const { return m_FocalPoint; }

        void Focus(const glm::vec3& focalPoint);

    private:
        void UpdateProjection();
        // TODO: Dirty flag for this.
        void UpdateView();

        bool OnMouseScroll(MouseScrolledEvent& e);
        void MousePan(const glm::vec2& delta);
        void MouseRotate(const glm::vec2& delta);
        void MouseZoom(float delta);

        glm::vec3 CalculatePosition() const;

        Pair<float, float> PanSpeed() const;
        float RotationSpeed() const;
        float ZoomSpeed() const;

    private:
        float m_Fov = 45.0f, m_AspectRatio = 1.778f, m_NearClip = 0.1f, m_FarClip = 1000.0f;

        glm::mat4 m_ViewMatrix;
        glm::vec3 m_Position = { 0.0f, 0.0f, 0.0f };
        glm::vec3 m_FocalPoint = { 0.0f, 0.0f, 0.0f };

        glm::vec2 m_InitialMousePosition = { 0.0f, 0.0f };

        float m_Distance = 10.0f;
        float m_Pitch = 0.0f, m_Yaw = 0.0f;
        float m_Roll = 0.0f;
        bool m_AltWasPressed = false;

        float m_ViewportWidth = 1280.0f, m_ViewportHeight = 720.0f;
    };
} // namespace Crowny
