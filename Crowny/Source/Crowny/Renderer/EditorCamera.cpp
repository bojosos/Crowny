#include "cwpch.h"

#include "Crowny/Input/Input.h"
#include "Crowny/Renderer/EditorCamera.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

namespace Crowny
{

    EditorCamera::EditorCamera(float fov, float aspectRatio, float nearPlane, float farPlane)
      : m_Fov(fov), m_ViewMatrix(1.0f), m_AspectRatio(aspectRatio), m_NearClip(nearPlane), m_FarClip(farPlane),
        Camera(glm::perspective(glm::radians(fov), aspectRatio, nearPlane, farPlane))
    {
    }

    void EditorCamera::OnUpdate(Timestep ts)
    {
        (void)ts;
        const bool altPressed = Input::IsKeyPressed(Key::LeftAlt) || Input::IsKeyPressed(Key::RightAlt);
        const glm::vec2 mouse = Input::GetMousePosition();
        if (altPressed)
        {
            const glm::vec2 delta = m_AltWasPressed ? (mouse - m_InitialMousePosition) * 0.003f : glm::vec2(0.0f);
            m_InitialMousePosition = mouse;

            if (Input::IsMouseButtonPressed(Mouse::ButtonMiddle))
                MousePan(delta);
            else if (Input::IsMouseButtonPressed(Mouse::ButtonLeft))
                MouseRotate(delta);
            else if (Input::IsMouseButtonPressed(Mouse::ButtonRight))
                MouseZoom(delta.y);
        }
        else
            m_InitialMousePosition = mouse;
        m_AltWasPressed = altPressed;

        UpdateView();
    }

    void EditorCamera::OnEvent(Event& e)
    {
        EventDispatcher dispatcher(e);
        dispatcher.Dispatch<MouseScrolledEvent>(CW_BIND_EVENT_FN(EditorCamera::OnMouseScroll));
    }

    void EditorCamera::UpdateProjection()
    {
        if (m_ViewportWidth <= 0.0f || m_ViewportHeight <= 0.0f)
            return;
        m_AspectRatio = m_ViewportWidth / m_ViewportHeight;
        const float fov = glm::radians(glm::clamp(m_Fov, 1.0f, 179.0f));
        const float nearClip = std::max(m_NearClip, 0.0001f);
        const float farClip = std::max(m_FarClip, nearClip + 0.0001f);
        m_Projection = glm::perspective(fov, m_AspectRatio, nearClip, farClip);
    }

    void EditorCamera::UpdateView()
    {
        m_Position = CalculatePosition();

        const glm::quat orientation = GetOrientation();
        m_ViewMatrix = glm::translate(glm::mat4(1.0f), m_Position) * glm::toMat4(orientation);
        m_ViewMatrix = glm::inverse(m_ViewMatrix);
    }

    bool EditorCamera::OnMouseScroll(MouseScrolledEvent& e)
    {
        const float delta = e.GetYOffset() * 0.1f;
        MouseZoom(delta);
        UpdateView();
        return true;
    }

    void EditorCamera::MousePan(const glm::vec2& delta)
    {
        auto [xSpeed, ySpeed] = PanSpeed();
        m_FocalPoint += -GetRightDirection() * delta.x * xSpeed * m_Distance;
        m_FocalPoint += GetUpDirection() * delta.y * ySpeed * m_Distance;
    }

    void EditorCamera::MouseRotate(const glm::vec2& delta)
    {
        const float yawSign = GetUpDirection().y < 0 ? -1.0f : 1.0f;
        m_Yaw += yawSign * delta.x * RotationSpeed();
        m_Pitch += delta.y * RotationSpeed();
    }

    void EditorCamera::MouseZoom(float delta)
    {
        m_Distance -= delta * ZoomSpeed();
        if (m_Distance < 1.0f)
        {
            m_FocalPoint += GetForwardDirection();
            m_Distance = 1.0f;
        }
    }

    glm::vec3 EditorCamera::CalculatePosition() const { return m_FocalPoint - GetForwardDirection() * m_Distance; }

    Pair<float, float> EditorCamera::PanSpeed() const
    {
        float x = std::min(m_ViewportWidth / 1000.0f, 2.4f);
        float xFactor = 0.0366f * x * x - 0.1778f * x + 0.3021f;

        float y = std::min(m_ViewportHeight / 1000.0f, 2.4f);
        float yFactor = 0.0366f * y * y - 0.1778f * y + 0.3021f;

        return std::make_pair(xFactor, yFactor);
    }

    float EditorCamera::RotationSpeed() const { return 0.8f; }

    float EditorCamera::ZoomSpeed() const
    {
        float distance = m_Distance * 0.2f;
        distance = std::max(distance, 0.0f);
        float speed = distance * distance;
        speed = std::min(speed, 100.0f);
        return speed;
    }

    void EditorCamera::Focus(const glm::vec3& focalPoint)
    {
        m_FocalPoint = focalPoint;
        if (m_Distance > 10.0f)
        {
            m_Distance -= m_Distance - 10.0f;
            m_Position = m_FocalPoint - GetForwardDirection() * m_Distance;
        }
        m_Position = m_FocalPoint - GetForwardDirection() * m_Distance;
        UpdateView();
    }

    glm::vec3 EditorCamera::GetUpDirection() const { return glm::rotate(GetOrientation(), glm::vec3(0.0f, 1.0f, 0.0f)); }

    glm::vec3 EditorCamera::GetRightDirection() const { return glm::rotate(GetOrientation(), glm::vec3(1.0f, 0.0f, 0.0f)); }

    glm::vec3 EditorCamera::GetForwardDirection() const { return glm::rotate(GetOrientation(), glm::vec3(0.0f, 0.0f, -1.0f)); }

    glm::quat EditorCamera::GetOrientation() const { return glm::quat(glm::vec3(-m_Pitch, -m_Yaw, m_Roll)); }

} // namespace Crowny
