#include "cwpch.h"

#include "Crowny/Input/Input.h"
#include "Crowny/Renderer/EditorCamera.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

namespace Crowny
{
    namespace
    {
        constexpr float MinimumFov = 1.0f;
        constexpr float MaximumFov = 179.0f;
        constexpr float MinimumNearClip = 0.0001f;
        constexpr float MinimumClipRange = 0.0001f;
        constexpr float DefaultAspectRatio = 1280.0f / 720.0f;

        float FiniteOr(float value, float fallback) { return std::isfinite(value) ? value : fallback; }

        bool IsFinite(const glm::vec3& value) { return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z); }
    } // namespace

    EditorCamera::EditorCamera() : EditorCamera(45.0f, DefaultAspectRatio, 0.1f, 1000.0f) {}

    EditorCamera::EditorCamera(float fov, float aspectRatio, float nearPlane, float farPlane)
      : m_Fov(fov), m_AspectRatio(aspectRatio), m_NearClip(nearPlane), m_FarClip(farPlane)
    {
        UpdateProjection();
        UpdateView();
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
        m_Fov = glm::clamp(FiniteOr(m_Fov, 45.0f), MinimumFov, MaximumFov);
        m_AspectRatio = FiniteOr(m_AspectRatio, DefaultAspectRatio);
        if (m_AspectRatio <= 0.0f)
            m_AspectRatio = DefaultAspectRatio;
        m_NearClip = std::max(FiniteOr(m_NearClip, 0.1f), MinimumNearClip);
        m_FarClip = std::max(FiniteOr(m_FarClip, 1000.0f), m_NearClip + MinimumClipRange);
        m_Projection = glm::perspective(glm::radians(m_Fov), m_AspectRatio, m_NearClip, m_FarClip);
        m_ViewProjectionDirty = true;
    }

    void EditorCamera::UpdateView() const
    {
        if (!m_ViewDirty)
            return;

        m_Position = CalculatePosition();

        const glm::quat orientation = GetOrientation();
        m_ViewMatrix = glm::translate(glm::mat4(1.0f), m_Position) * glm::toMat4(orientation);
        m_ViewMatrix = glm::inverse(m_ViewMatrix);
        m_ViewDirty = false;
        m_ViewProjectionDirty = true;
    }

    void EditorCamera::MarkViewDirty()
    {
        m_ViewDirty = true;
        m_ViewProjectionDirty = true;
    }

    void EditorCamera::SetViewportSize(float width, float height)
    {
        if (!std::isfinite(width) || !std::isfinite(height) || width <= 0.0f || height <= 0.0f)
            return;

        const float aspectRatio = width / height;
        if (!std::isfinite(aspectRatio) || aspectRatio <= 0.0f)
            return;
        if (width == m_ViewportWidth && height == m_ViewportHeight && aspectRatio == m_AspectRatio)
            return;

        m_ViewportWidth = width;
        m_ViewportHeight = height;
        if (aspectRatio != m_AspectRatio)
        {
            m_AspectRatio = aspectRatio;
            UpdateProjection();
        }
    }

    const glm::mat4& EditorCamera::GetViewMatrix() const
    {
        UpdateView();
        return m_ViewMatrix;
    }

    const glm::mat4& EditorCamera::GetViewProjection() const
    {
        UpdateView();
        if (m_ViewProjectionDirty)
        {
            m_ViewProjection = m_Projection * m_ViewMatrix;
            m_ViewProjectionDirty = false;
        }
        return m_ViewProjection;
    }

    glm::vec3 EditorCamera::GetPosition() const
    {
        UpdateView();
        return m_Position;
    }

    void EditorCamera::SetPosition(const glm::vec3& position)
    {
        if (!IsFinite(position))
            return;
        m_Position = position;
        m_FocalPoint = position + GetForwardDirection() * m_Distance;
        MarkViewDirty();
    }

    void EditorCamera::SetFocalPoint(const glm::vec3& focalPoint)
    {
        if (!IsFinite(focalPoint) || focalPoint == m_FocalPoint)
            return;
        m_FocalPoint = focalPoint;
        MarkViewDirty();
    }

    void EditorCamera::SetYaw(float yaw)
    {
        if (!std::isfinite(yaw) || yaw == m_Yaw)
            return;
        m_Yaw = yaw;
        MarkViewDirty();
    }

    void EditorCamera::SetRoll(float roll)
    {
        if (!std::isfinite(roll) || roll == m_Roll)
            return;
        m_Roll = roll;
        MarkViewDirty();
    }

    void EditorCamera::SetPitch(float pitch)
    {
        if (!std::isfinite(pitch) || pitch == m_Pitch)
            return;
        m_Pitch = pitch;
        MarkViewDirty();
    }

    void EditorCamera::SetDistance(float distance)
    {
        if (!std::isfinite(distance))
            return;
        distance = std::max(distance, 0.1f);
        if (distance == m_Distance)
            return;
        m_Distance = distance;
        MarkViewDirty();
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
        if (delta.x == 0.0f && delta.y == 0.0f)
            return;
        auto [xSpeed, ySpeed] = PanSpeed();
        m_FocalPoint += -GetRightDirection() * delta.x * xSpeed * m_Distance;
        m_FocalPoint += GetUpDirection() * delta.y * ySpeed * m_Distance;
        MarkViewDirty();
    }

    void EditorCamera::MouseRotate(const glm::vec2& delta)
    {
        if (delta.x == 0.0f && delta.y == 0.0f)
            return;
        const float yawSign = GetUpDirection().y < 0 ? -1.0f : 1.0f;
        m_Yaw += yawSign * delta.x * RotationSpeed();
        m_Pitch += delta.y * RotationSpeed();
        MarkViewDirty();
    }

    void EditorCamera::MouseZoom(float delta)
    {
        if (delta == 0.0f)
            return;
        m_Distance -= delta * ZoomSpeed();
        if (m_Distance < 1.0f)
        {
            m_FocalPoint += GetForwardDirection();
            m_Distance = 1.0f;
        }
        MarkViewDirty();
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
        if (!IsFinite(focalPoint))
            return;
        m_FocalPoint = focalPoint;
        if (m_Distance > 10.0f)
            m_Distance = 10.0f;
        MarkViewDirty();
        UpdateView();
    }

    glm::vec3 EditorCamera::GetUpDirection() const { return glm::rotate(GetOrientation(), glm::vec3(0.0f, 1.0f, 0.0f)); }

    glm::vec3 EditorCamera::GetRightDirection() const { return glm::rotate(GetOrientation(), glm::vec3(1.0f, 0.0f, 0.0f)); }

    glm::vec3 EditorCamera::GetForwardDirection() const { return glm::rotate(GetOrientation(), glm::vec3(0.0f, 0.0f, -1.0f)); }

    glm::quat EditorCamera::GetOrientation() const { return glm::quat(glm::vec3(-m_Pitch, -m_Yaw, m_Roll)); }

} // namespace Crowny
