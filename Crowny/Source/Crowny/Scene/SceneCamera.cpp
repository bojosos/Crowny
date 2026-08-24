#include "cwpch.h"

#include "Crowny/Scene/SceneCamera.h"

#include <glm/ext/matrix_clip_space.hpp>

namespace Crowny
{
    namespace
    {
        constexpr float MinimumPerspectiveNear = 0.0001f;
        constexpr float MinimumClipRange = 0.0001f;
        constexpr float MinimumOrthographicSize = 0.0001f;
        constexpr float DegreesToRadians = 0.01745329251994329577f;
        constexpr float MinimumFov = DegreesToRadians;
        constexpr float MaximumFov = 179.0f * DegreesToRadians;

        float FiniteOr(float value, float fallback) { return std::isfinite(value) ? value : fallback; }
    } // namespace

    SceneCamera::SceneCamera() { RecalculateProjection(); }

    SceneCamera::SceneCamera(const glm::mat4& projection) : Camera(projection) {}

    void SceneCamera::SetPerspective(float verticalFov, float nearPlane, float farPlane)
    {
        m_ProjectionType = CameraProjection::Perspective;
        m_PerspectiveFOV = glm::clamp(FiniteOr(verticalFov, glm::radians(45.0f)), MinimumFov, MaximumFov);
        m_PerspectiveNear = std::max(FiniteOr(nearPlane, 0.01f), MinimumPerspectiveNear);
        m_PerspectiveFar = std::max(FiniteOr(farPlane, 1000.0f), m_PerspectiveNear + MinimumClipRange);
        RecalculateProjection();
    }

    void SceneCamera::SetOrthographic(float size, float nearPlane, float farPlane)
    {
        m_ProjectionType = CameraProjection::Orthographic;
        m_OrthographicSize = std::max(FiniteOr(size, 10.0f), MinimumOrthographicSize);
        m_OrthographicNear = FiniteOr(nearPlane, -1.0f);
        m_OrthographicFar = std::max(FiniteOr(farPlane, 1.0f), m_OrthographicNear + MinimumClipRange);
        RecalculateProjection();
    }

    void SceneCamera::SetViewportSize(uint32_t width, uint32_t height)
    {
        if (width == 0 || height == 0)
            return;
        m_AspectRatio = static_cast<float>(width) / static_cast<float>(height);
        RecalculateProjection();
    }

    void SceneCamera::SetPerspectiveVerticalFOV(float fov)
    {
        m_PerspectiveFOV = glm::clamp(FiniteOr(fov, m_PerspectiveFOV), MinimumFov, MaximumFov);
        RecalculateProjection();
    }

    void SceneCamera::SetPerspectiveNearClip(float nearPlane)
    {
        m_PerspectiveNear = std::max(FiniteOr(nearPlane, m_PerspectiveNear), MinimumPerspectiveNear);
        m_PerspectiveFar = std::max(m_PerspectiveFar, m_PerspectiveNear + MinimumClipRange);
        RecalculateProjection();
    }

    void SceneCamera::SetPerspectiveFarClip(float farPlane)
    {
        m_PerspectiveFar = std::max(FiniteOr(farPlane, m_PerspectiveFar), m_PerspectiveNear + MinimumClipRange);
        RecalculateProjection();
    }

    void SceneCamera::SetOrthographicSize(float size)
    {
        m_OrthographicSize = std::max(FiniteOr(size, m_OrthographicSize), MinimumOrthographicSize);
        RecalculateProjection();
    }

    void SceneCamera::SetOrthographicNearClip(float nearPlane)
    {
        m_OrthographicNear = FiniteOr(nearPlane, m_OrthographicNear);
        m_OrthographicFar = std::max(m_OrthographicFar, m_OrthographicNear + MinimumClipRange);
        RecalculateProjection();
    }

    void SceneCamera::SetOrthographicFarClip(float farPlane)
    {
        m_OrthographicFar = std::max(FiniteOr(farPlane, m_OrthographicFar), m_OrthographicNear + MinimumClipRange);
        RecalculateProjection();
    }

    void SceneCamera::SetAspectRatio(float value)
    {
        value = FiniteOr(value, m_AspectRatio);
        if (value <= 0.0f)
            return;
        m_AspectRatio = value;
        RecalculateProjection();
    }

    void SceneCamera::SetViewportRect(const glm::vec4& rect)
    {
        glm::vec4 result;
        result.x = glm::clamp(FiniteOr(rect.x, 0.0f), 0.0f, 1.0f);
        result.y = glm::clamp(FiniteOr(rect.y, 0.0f), 0.0f, 1.0f);
        result.z = glm::clamp(FiniteOr(rect.z, 1.0f), 0.0f, 1.0f - result.x);
        result.w = glm::clamp(FiniteOr(rect.w, 1.0f), 0.0f, 1.0f - result.y);
        m_ViewportRectangle = result;
    }

    void SceneCamera::RecalculateProjection()
    {
        if (m_ProjectionType == CameraProjection::Perspective)
            m_Projection = glm::perspective(m_PerspectiveFOV, m_AspectRatio, m_PerspectiveNear, m_PerspectiveFar);
        else
        {
            const float left = -m_OrthographicSize * m_AspectRatio * 0.5f;
            const float right = m_OrthographicSize * m_AspectRatio * 0.5f;
            const float bot = -m_OrthographicSize * 0.5f;
            const float top = m_OrthographicSize * 0.5f;
            m_Projection = glm::ortho(left, right, bot, top, m_OrthographicNear, m_OrthographicFar);
        }
    }

}; // namespace Crowny
