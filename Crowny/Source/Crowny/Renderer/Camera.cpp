#include "cwpch.h"

#include "Crowny/Renderer/Camera.h"

namespace Crowny
{
	Camera::Camera(const glm::mat4& projection) : m_ProjectionMatrix(projection)
	{
		
	}

	void Camera::SetViewport(uint32_t width, uint32_t height)
	{
		if (m_Projection == CameraProjection::Orthographic)
			m_ProjectionMatrix = glm::ortho(0.0f, (float)width, (float)height, 0.0f);

		if (m_Projection == CameraProjection::Perspective)
		{
			m_ProjectionMatrix = glm::perspective(glm::radians((float)m_Fov), (float)width / (float)height, m_ClippingPlanes.x, m_ClippingPlanes.y);
		}
	}

	Camera::Camera()
	{
	}

};