#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace Crowny
{
	
	class CameraComponent;
	class Entity;

	enum class CameraProjection
	{
		Orthographic, Perspective
	};

	class Camera
	{
	public:
		template <class Component>
		friend void ComponentEditorWidget(Entity& entity);
		friend void ComponentEditorWidget<CameraComponent>(Entity& e);

		Camera() = default;
		Camera(const glm::mat4& projection);
		~Camera() = default;

		const glm::mat4& GetProjectionMatrix() const { return m_ProjectionMatrix; }
		void SetProjectionMatrix(const glm::mat4& matrix) { m_ProjectionMatrix = matrix; }

	private:
		glm::vec3 m_BackgroundColor{ 0.0f, 0.3f, 0.3f };
		glm::vec2 m_ClippingPlanes{ 0.3f, 1000.0f };
		
		CameraProjection m_Projection = CameraProjection::Orthographic;
		glm::vec4 m_ViewportRectangle{ 0.0f, 0.0f, 1.0f, 1.0f };
		bool m_HDR = false;
		bool m_MSAA = false;
		bool m_OcclusionCulling = false;
		bool m_FixedAspectRatio = false;

		glm::mat4 m_ProjectionMatrix;
	};

}