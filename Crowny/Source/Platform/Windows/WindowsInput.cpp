#include "cwpch.h"

#include "Crowny/Application/Application.h"
#include "Crowny/Input/Input.h"
#include "Crowny/Window/Window.h"

#include <GLFW/glfw3.h>
#include <array>

namespace Crowny
{
    namespace
    {
        std::array<uint8_t, GLFW_KEY_LAST + 1> s_CurrentKeys{};
        std::array<uint8_t, GLFW_KEY_LAST + 1> s_PreviousKeys{};
        std::array<uint8_t, GLFW_MOUSE_BUTTON_LAST + 1> s_CurrentMouseButtons{};
        std::array<uint8_t, GLFW_MOUSE_BUTTON_LAST + 1> s_PreviousMouseButtons{};

        GLFWwindow* GetMainGLFWWindow()
        {
            if (Application::TryGet() == nullptr || Application::TryGet()->GetApplicationDesc().Headless)
                return nullptr;
            return static_cast<GLFWwindow*>(Application::TryGet()->GetWindow().GetNativeWindow());
        }

        bool IsValidKey(KeyCode key) { return key <= GLFW_KEY_LAST; }
        bool IsValidMouseButton(MouseCode button) { return button <= GLFW_MOUSE_BUTTON_LAST; }
    } // namespace

    bool Input::s_Grabbed = false;
    float Input::m_FrameScrollX = 0.0f;
    float Input::m_FrameScrollY = 0.0f;

    bool Input::GetKey(const KeyCode key) { return IsValidKey(key) && s_CurrentKeys[key] != 0; }

    bool Input::GetMouseButton(const MouseCode button) { return IsValidMouseButton(button) && s_CurrentMouseButtons[button] != 0; }

    bool Input::IsKeyPressed(const KeyCode key) { return GetKey(key); }

    bool Input::IsKeyDown(const KeyCode key) { return IsValidKey(key) && s_CurrentKeys[key] != 0 && s_PreviousKeys[key] == 0; }

    bool Input::IsKeyUp(const KeyCode key) { return IsValidKey(key) && s_CurrentKeys[key] == 0 && s_PreviousKeys[key] != 0; }

    bool Input::IsMouseButtonPressed(const MouseCode button) { return GetMouseButton(button); }

    bool Input::IsMouseButtonDown(const MouseCode button)
    {
        return IsValidMouseButton(button) && s_CurrentMouseButtons[button] != 0 && s_PreviousMouseButtons[button] == 0;
    }

    bool Input::IsMouseButtonUp(const MouseCode button)
    {
        return IsValidMouseButton(button) && s_CurrentMouseButtons[button] == 0 && s_PreviousMouseButtons[button] != 0;
    }

    glm::vec2 Input::GetMousePosition()
    {
        GLFWwindow* window = GetMainGLFWWindow();
        if (window == nullptr)
            return glm::vec2(0.0f);

        double x = 0.0;
        double y = 0.0;
        glfwGetCursorPos(window, &x, &y);
        return { static_cast<float>(x), static_cast<float>(y) };
    }

    void Input::OnMouseScroll(float xOffset, float yOffset)
    {
        m_FrameScrollX += xOffset;
        m_FrameScrollY += yOffset;
    }

    float Input::GetMouseX() { return GetMousePosition().x; }

    float Input::GetMouseY() { return GetMousePosition().y; }

    void Input::SetMousePosition(const glm::vec2& position)
    {
        if (GLFWwindow* window = GetMainGLFWWindow())
            glfwSetCursorPos(window, position.x, position.y);
    }

    void Input::SetMouseGrabbed(bool grabbed)
    {
        s_Grabbed = grabbed;
        if (Application::TryGet() != nullptr && !Application::TryGet()->GetApplicationDesc().Headless)
            Application::TryGet()->GetWindow().SetCursorGrabbed(grabbed);
    }

    bool Input::IsMouseGrabbed() { return s_Grabbed; }

    float Input::GetMouseScrollX() { return m_FrameScrollX; }

    float Input::GetMouseScrollY() { return m_FrameScrollY; }

    void Input::OnKeyState(int32_t key, bool pressed)
    {
        if (key >= 0 && key <= GLFW_KEY_LAST)
            s_CurrentKeys[static_cast<size_t>(key)] = pressed ? 1 : 0;
    }

    void Input::OnMouseButtonState(int32_t button, bool pressed)
    {
        if (button >= 0 && button <= GLFW_MOUSE_BUTTON_LAST)
            s_CurrentMouseButtons[static_cast<size_t>(button)] = pressed ? 1 : 0;
    }

    void Input::ResetState()
    {
        s_CurrentKeys.fill(0);
        s_PreviousKeys.fill(0);
        s_CurrentMouseButtons.fill(0);
        s_PreviousMouseButtons.fill(0);
    }

    void Input::OnUpdate()
    {
        s_PreviousKeys = s_CurrentKeys;
        s_PreviousMouseButtons = s_CurrentMouseButtons;
        m_FrameScrollX = 0.0f;
        m_FrameScrollY = 0.0f;
    }

} // namespace Crowny
