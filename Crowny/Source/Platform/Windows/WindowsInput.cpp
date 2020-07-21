#include "cwpch.h"

#include "Crowny/Input/Input.h"
#include "Crowny/Application/Application.h"

#include <glm/glm.hpp>
#include <glfw/glfw3.h>

namespace Crowny
{
	bool Input::s_Grabbed = false;

	bool Input::IsKeyPressed(KeyCode key)
	{
		auto window = static_cast<GLFWwindow*>(Application::Get().GetWindow().GetNativeWindow());
		auto state = glfwGetKey(window, static_cast<int32_t>(key));
		return state == GLFW_PRESS || state == GLFW_REPEAT;
	}

	bool Input::IsMouseButtonPressed(MouseCode button)
	{
		auto window = static_cast<GLFWwindow*>(Application::Get().GetWindow().GetNativeWindow());
		auto state = glfwGetMouseButton(window, static_cast<int32_t>(button));
		return state == GLFW_PRESS;
	}

	std::pair<float, float> Input::GetMousePosition()
	{
		auto window = static_cast<GLFWwindow*>(Application::Get().GetWindow().GetNativeWindow());
		double xpos, ypos;
		glfwGetCursorPos(window, &xpos, &ypos);

		return { (float)xpos, (float)ypos };
	}

	float Input::GetMouseX()
	{
		std::pair<float, float> pos = GetMousePosition();
		return pos.first;
	}

	float Input::GetMouseY()
	{
		std::pair<float, float> pos = GetMousePosition();
		return pos.second;
	}

	void Input::SetMousePosition(const glm::vec2& pos)
	{
		auto window = static_cast<GLFWwindow*>(Application::Get().GetWindow().GetNativeWindow());
		glfwSetCursorPos(window, pos.x, pos.y);
	}

	void Input::SetMouseGrabbed(bool grabbed)
	{
		s_Grabbed = grabbed;
	}

	bool Input::IsMouseGrabbed()
	{
		return s_Grabbed;
	}

}
