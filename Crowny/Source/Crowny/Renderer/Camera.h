#pragma once

#include "Crowny/Common/Math.h"
#include "Crowny/Common/Timestep.h"
#include "Crowny/Math/Frustum.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace Crowny
{
	class Camera
	{
	public:
		Camera(const glm::mat4& projectionMatrix);
		~Camera();

		void Focus();
		void Update(Timestep ts);

		const glm::vec3& GetPosition() const { return m_Position; }
		void SetPosition(const glm::vec3& position) { m_Position = position; }

		const glm::vec3& GetRotation() const { return m_Rotation; }
		void SetRotation(const glm::vec3& rotation) { m_Rotation = rotation; }

		const glm::mat4& GetProjectionMatrix() const { return m_ProjectionMatrix; }
		const glm::mat4& GetViewMatrix() const { return m_ViewMatrix; }
		const glm::mat4& GetViewProjectionMatrix() const { return m_ViewProjectionMatrix; }

		void SetProjectionMatrix(const glm::mat4& matrix) { m_ProjectionMatrix = matrix; }

		void OnResize(float width, float height);

		float GetSpeed() const { return m_Speed; }
		void SetSpeed(float speed) { m_Speed = speed; }

		void Translate(const glm::vec3& translation) { m_Position += translation; }
		void Rotate(const glm::vec3& rotation) { m_Rotation += rotation; }

		void Translate(float x, float y, float z) { m_Position += glm::vec3(x, y, z); }
		void Rotate(float x, float y, float z) { m_Rotation += glm::vec3(x, y, z); }


		const glm::vec3 GetForwardDirection() const { return Math::GetForwardDirection(m_Rotation); }
		const glm::vec3 GetRightDirection() const { return Math::GetRightDirection(m_Rotation); }
		Ref<ViewFrustum> GetFrustum() const { return m_Frustum; }

	private:
#ifdef MC_WEB
		glm::vec2 lastPos = { 0.0f, 0.0f };
#endif
		Ref<ViewFrustum> m_Frustum;
		glm::vec3 up = { 0.0f, 1.0f, 0.0f };

		glm::mat4 m_ProjectionMatrix;
		glm::mat4 m_ViewMatrix;
		glm::mat4 m_ViewProjectionMatrix;

		glm::vec3 m_Position;
		glm::vec3 m_Rotation; 

		float m_RotationSpeed;
		float m_MouseSensitivity;
		float m_Speed, m_SprintSpeed;
		float m_Pitch, m_Yaw;
		bool m_MouseWasGrabbed;
	
	public:
		static Camera& GetCurrentCamera();
	private:
		static uint32_t s_ActiveCameraIndex;
		static std::vector<Camera*> s_Cameras;

	};

}