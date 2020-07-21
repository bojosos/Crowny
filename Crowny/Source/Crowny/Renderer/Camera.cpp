#include "cwpch.h"

#include "Camera.h"
#include "Crowny/Input/Input.h"
#include "Crowny/Application/Application.h"

namespace Crowny
{

	std::vector<Camera*> Camera::s_Cameras;
	uint32_t Camera::s_ActiveCameraIndex = 0;

	Camera& Camera::GetCurrentCamera()
	{
		if (s_Cameras.size() <= s_ActiveCameraIndex)
		{
			CW_ENGINE_ERROR("Invalid active camera index");
		}

		return *s_Cameras[s_ActiveCameraIndex];
	}

	Camera::Camera(const CameraProperties& properties) : m_Properties(properties), m_MouseSensitivity(0.002f), m_RotationSpeed(60.0f), m_Speed(0.04f), m_SprintSpeed(m_Speed * 6.0f), m_MouseWasGrabbed(false)
	{
		m_ViewMatrix = glm::mat4(1.0f);
		m_Rotation = { 0.0f, 0.0f, 0.0f };

		m_Yaw = 0.0f;
		m_Pitch = 0.0f;
		m_Frustum = CreateRef<ViewFrustum>();

		switch (m_Properties.Projection)
		{
		case(CameraProjection::Orthographic): m_ProjectionMatrix = glm::ortho(0.0f, 1280.0f, 720.0f, 0.0f); break;
		case(CameraProjection::Perspective): m_ProjectionMatrix = glm::perspective((float)m_Properties.Fov, (float)Application::Get().GetWindow().GetWidth() / (float)Application::Get().GetWindow().GetHeight(), m_Properties.ClippingPlanes.x, m_Properties.ClippingPlanes.y); break;
		}

		s_Cameras.push_back(this);
	}

	void Camera::OnResize(float width, float height)
	{
		m_ProjectionMatrix = glm::ortho(0.0f, width, height, 0.0f);
	}

	void Camera::Focus()
	{
		Input::SetMouseGrabbed(true);
		Application::Get().GetWindow().SetCursor(Cursor::NO_CURSOR);
	}

	void Camera::Update(Timestep ts)
	{
#ifndef MC_WEB
		glm::vec2 windowSize = { Application::Get().GetWindow().GetWidth(),  Application::Get().GetWindow().GetHeight() };
		glm::vec2 windowCenter = glm::vec2((float)(int32_t)(windowSize.x / 2.0f), (float)(int32_t)(windowSize.y / 2.0f));
#endif
		if (Input::IsMouseButtonPressed(MouseCode::ButtonRight))
		{
			if (!Input::IsMouseGrabbed())
			{
				Input::SetMouseGrabbed(true);
				Application::Get().GetWindow().SetCursor(Cursor::NO_CURSOR);
			}
		}

		if (Input::IsMouseGrabbed())
		{
			// FPS Controller
			glm::vec2 mousePos { Input::GetMousePosition().first, Input::GetMousePosition().second };
#ifdef MC_WEB
			mousePos.x -= lastPos.x;
			mousePos.y -= lastPos.y;
			lastPos = Input::GetMousePosition();
#else
			mousePos.x -= windowCenter.x;
			mousePos.y -= windowCenter.y;
#endif
			if (m_MouseWasGrabbed)
			{
				m_Rotation.y += mousePos.x * m_MouseSensitivity * 60;// *m_RotationSpeed;// *(float)ts;
				m_Rotation.x += mousePos.y * m_MouseSensitivity * 60;// *m_RotationSpeed;// *(float)ts;
			}
			
			if (m_Rotation.x < -80.0f) {
				m_Rotation.x = -79.9f;
			}
			else if (m_Rotation.x > 85.0f) {
				m_Rotation.x = 84.9f;
			}
			m_MouseWasGrabbed = true;

#ifndef MC_WEB
			Input::SetMousePosition(windowCenter);
#endif

			glm::vec3 forward = GetForwardDirection();
			glm::vec3 right = GetRightDirection();
			
			float speed = (Input::IsKeyPressed(KeyCode::LeftControl) ? m_SprintSpeed : m_Speed);// *(float)ts;

			if (Input::IsKeyPressed(KeyCode::W))
				m_Position += forward * speed;
			if (Input::IsKeyPressed(KeyCode::S))
				m_Position -= forward * speed;
			if (Input::IsKeyPressed(KeyCode::D))
				m_Position += right * speed;
			if (Input::IsKeyPressed(KeyCode::A))
				m_Position -= right * speed;
			if (Input::IsKeyPressed(KeyCode::Space))
				m_Position += up * speed;
			if (Input::IsKeyPressed(KeyCode::LeftShift))
				m_Position -= up * speed;

			m_ViewMatrix = glm::mat4(1.0f);

			m_ViewMatrix = glm::rotate(m_ViewMatrix, glm::radians(m_Rotation.x), { 1.0f, 0.0f, 0.0f });
			m_ViewMatrix = glm::rotate(m_ViewMatrix, glm::radians(m_Rotation.y), { 0.0f, 1.0f, 0.0f });
			m_ViewMatrix = glm::rotate(m_ViewMatrix, glm::radians(m_Rotation.z), { 0.0f, 0.0f, 1.0f });

			m_ViewMatrix = glm::translate(m_ViewMatrix, -m_Position);
		}

		if (Input::IsKeyPressed(KeyCode::Escape))
		{
			Input::SetMouseGrabbed(false);
			Application::Get().GetWindow().SetCursor(Cursor::POINTER);
			m_MouseWasGrabbed = false;
		}

		m_Frustum->Update(m_ProjectionMatrix * m_ViewMatrix);
		m_ViewProjectionMatrix = m_ProjectionMatrix * m_ViewMatrix;
	}

};