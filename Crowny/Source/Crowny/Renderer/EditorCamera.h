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
        EditorCamera();
        EditorCamera(float fov, float aspectRatio, float nearPlane, float farPlane);

        void OnUpdate(Timestep ts);
        void OnEvent(Event& e);

        float GetDistance() const { return m_Distance; }

        void SetViewportSize(float width, float height);

        const glm::mat4& GetViewMatrix() const;
        const glm::mat4& GetViewProjection() const;

        glm::vec3 GetUpDirection() const;
        glm::vec3 GetRightDirection() const;
        glm::vec3 GetForwardDirection() const;
        glm::vec3 GetPosition() const override;
        glm::quat GetOrientation() const;

        void SetPosition(const glm::vec3& position);
        void SetFocalPoint(const glm::vec3& focalPoint);
        void SetYaw(float yaw);
        void SetRoll(float roll);
        void SetPitch(float pitch);
        void SetDistance(float distance);

        float GetPitch() const { return m_Pitch; }
        float GetYaw() const { return m_Yaw; }
        float GetRoll() const { return m_Roll; }

        const glm::vec3& GetFocalPoint() const { return m_FocalPoint; }

        void Focus(const glm::vec3& focalPoint);

    private:
        void UpdateProjection();
        void UpdateView() const;
        void MarkViewDirty();

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

        mutable glm::mat4 m_ViewMatrix = glm::mat4(1.0f);
        mutable glm::mat4 m_ViewProjection = glm::mat4(1.0f);
        mutable glm::vec3 m_Position = { 0.0f, 0.0f, 0.0f };
        glm::vec3 m_FocalPoint = { 0.0f, 0.0f, 0.0f };

        glm::vec2 m_InitialMousePosition = { 0.0f, 0.0f };

        float m_Distance = 10.0f;
        float m_Pitch = 0.0f, m_Yaw = 0.0f;
        float m_Roll = 0.0f;
        bool m_AltWasPressed = false;
        mutable bool m_ViewDirty = true;
        mutable bool m_ViewProjectionDirty = true;

        float m_ViewportWidth = 1280.0f, m_ViewportHeight = 720.0f;
    };
} // namespace Crowny
