#include "cwpch.h"

#include "Crowny/Renderer/EditorCamera.h"
#include "Crowny/Input/Input.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

namespace Crowny
{
    
	EditorCamera::EditorCamera(float fov, float aspectRatio, float near, float far)
		: m_Fov(fov), m_AspectRatio(aspectRatio), m_NearClip(near), m_FarClip(far), Camera(glm::perspective(glm::radians(fov), aspectRatio, near, far))
	{

	}

	void EditorCamera::OnUpdate(Timestep ts)
    {
		if (Input::IsKeyPressed(Key::LeftAlt))
		{
			glm::vec2 mouse = Input::GetMousePosition();
			glm::vec2 delta = (mouse - m_InitialMousePosition) * 0.003f;
			m_InitialMousePosition = mouse;

			if (Input::IsMouseButtonPressed(Mouse::ButtonMiddle))
				MousePan(delta);
			else if (Input::IsMouseButtonPressed(Mouse::ButtonLeft))
				MouseRoate(delta);
			else if (Input::IsMouseButtonPressed(Mouse::ButtonRight))
				MouseZoom(delta.y);
		}
		
		UpdateView();
	}

	void EditorCamera::OnEvent(Event& e)
    {
		EventDispatcher dispatcher(e);
		dispatcher.Dispatch<MouseScrolledEvent>(CW_BIND_EVENT_FN(EditorCamera::OnMouseScroll));
	}

	void EditorCamera::UpdateProjection()
    {
		m_AspectRatio = m_ViewportWidth / m_ViewportHeight;
		m_Projection = glm::perspective(glm::radians(m_Fov), m_AspectRatio, m_NearClip, m_FarClip);
	}

	void EditorCamera::UpdateView()
    {
		m_Position = CalcualtePosition();
		
		glm::quat orietantion = GetOrientation();
		m_ViewMatrix = glm::translate(glm::mat4(1.0f), m_Position) * glm::toMat4(orietantion);
		m_ViewMatrix = glm::inverse(m_ViewMatrix);
	}

	bool EditorCamera::OnMouseScroll(MouseScrolledEvent& e)
    {
		float delta = e.GetYOffset() * 0.1f;
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

	void EditorCamera::MouseRoate(const glm::vec2& delta)
    {
		float yawSign = GetUpDirection().y < 0 ? -1.0f : 1.0f;
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
	
	glm::vec3 EditorCamera::CalcualtePosition() const
	{
		return m_FocalPoint - GetForwardDirection() * m_Distance;
	}
	
	std::pair<float, float> EditorCamera::PanSpeed() const
	{
		float x = std::min(m_ViewportWidth / 1000.0f, 2.4f);
		float xFactor = 0.0366f * x * x - 0.1778f * x + 0.3021f;
		
		float y = std::min(m_ViewportWidth / 1000.0f, 2.4f);
		float yFactor = 0.0366f * y * y - 0.1778f * y + 0.3021f;
		
		return std::make_pair(xFactor, yFactor);
	}
	
	float EditorCamera::RotationSpeed() const
	{
		return 0.8f;
	}
	
	float EditorCamera::ZoomSpeed() const
	{
		float distance = m_Distance * 0.2f;
		distance = std::max(distance, 0.0f);
		float speed = distance * distance;
		speed = std::min(speed, 100.0f);
		return speed;
	}
	
	glm::vec3 EditorCamera::GetUpDirection() const
	{
		return glm::rotate(GetOrientation(), glm::vec3(0.0f, 1.0f, 0.0f));
	}
	
	glm::vec3 EditorCamera::GetRightDirection() const
	{
		return glm::rotate(GetOrientation(), glm::vec3(1.0f, 0.0f, 0.0f));
	}
	
	glm::vec3 EditorCamera::GetForwardDirection() const
	{
		return glm::rotate(GetOrientation(), glm::vec3(0.0f, 0.0f, -1.0f));
	}
	
	glm::quat EditorCamera::GetOrientation() const
	{
		return glm::quat(glm::vec3(-m_Pitch, -m_Yaw, 0.0f));
	}
	
}