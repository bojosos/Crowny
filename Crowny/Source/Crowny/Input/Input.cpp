#include "cwpch.h"

#include "Crowny/Input/Input.h"
#include "Crowny/Application.h"

#include <GLFW/glfw3.h>

namespace Crowny
{
	Scope<Input> Input::s_Instance = CreateScope<Input>();

	bool Input::IsKeyPressedImpl(KeyCode key)
	{
		auto window = static_cast<GLFWwindow*>(Application::Get().GetWindow().GetNativeWindow());
		auto state = glfwGetKey(window, static_cast<int32_t>(key));
		return state == GLFW_PRESS || state == GLFW_REPEAT;
	}

	bool Input::IsMouseButtonPressedImpl(MouseCode button)
	{
		auto window = static_cast<GLFWwindow*>(Application::Get().GetWindow().GetNativeWindow());
		auto state = glfwGetMouseButton(window, static_cast<int32_t>(button));
		return state == GLFW_PRESS;
	}

	glm::vec2 Input::GetMousePositionImpl()
	{
		auto window = static_cast<GLFWwindow*>(Application::Get().GetWindow().GetNativeWindow());
		double xpos, ypos;
		glfwGetCursorPos(window, &xpos, &ypos);

		return { (float)xpos, (float)ypos };
	}

	float Input::GetMouseXImpl()
	{
		glm::vec2 pos = GetMousePositionImpl();
		return pos.x;
	}

	float Input::GetMouseYImpl()
	{
		glm::vec2 pos = GetMousePositionImpl();
		return pos.y;
	}

	void Input::SetMousePositionImpl(const glm::vec2& pos)
	{
		auto window = static_cast<GLFWwindow*>(Application::Get().GetWindow().GetNativeWindow());
		glfwSetCursorPos(window, pos.x, pos.y);
	}

	void Input::SetMouseCursorImpl(CursorType cursor)
	{
		auto window = static_cast<GLFWwindow*>(Application::Get().GetWindow().GetNativeWindow());
		switch (cursor)
		{
		case(CursorType::POINTER):
			glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
			break;
		case(CursorType::NO_CURSOR):
			glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
			break;
		}
	}
}
