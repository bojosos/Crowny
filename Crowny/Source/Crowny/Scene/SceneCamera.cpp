#include "cwpch.h"

#include "Crowny/Scene/SceneCamera.h"

#include <glm/ext/matrix_clip_space.hpp>

namespace Crowny
{

    SceneCamera::SceneCamera() { RecalculateProjection(); }

    void SceneCamera::SetPerspective(float verticalFov, float nearPlane, float farPlane)
    {
        m_ProjectionType = CameraProjection::Perspective;
        m_PerspectiveFOV = verticalFov;
        m_PerspectiveNear = nearPlane;
        m_PerspectiveFar = farPlane;
        RecalculateProjection();
    }

    void SceneCamera::SetOrthographic(float size, float nearPlane, float farPlane)
    {
        m_ProjectionType = CameraProjection::Orthographic;
        m_OrthographicSize = size;
        m_OrthographicNear = nearPlane;
        m_OrthographicFar = farPlane;
        RecalculateProjection();
    }

    void SceneCamera::SetViewportSize(uint32_t width, uint32_t height)
    {
        m_AspectRatio = (float)width / (float)height;
        RecalculateProjection();
    }

    void SceneCamera::RecalculateProjection()
    {
        if (m_ProjectionType == CameraProjection::Perspective)
            m_Projection = glm::perspective(m_PerspectiveFOV, m_AspectRatio, m_PerspectiveNear, m_PerspectiveFar);
        else
        {
            float left = -m_OrthographicSize * m_AspectRatio * 0.5f;
            float right = m_OrthographicSize * m_AspectRatio * 0.5f;
            float bot = -m_OrthographicSize * 0.5f;
            float top = m_OrthographicSize * 0.5f;
            m_Projection = glm::ortho(left, right, bot, top, m_OrthographicNear, m_OrthographicFar);
        }
    }

}; // namespace Crowny