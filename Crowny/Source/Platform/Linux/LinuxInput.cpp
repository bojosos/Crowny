#include "cwpch.h"
#if 0
#include "Crowny/Application/Application.h"
#include "Crowny/Input/Input.h"

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

namespace Crowny
{
    bool Input::s_Grabbed = false;
    float Input::m_FrameScrollX = 0.0f;
    float Input::m_FrameScrollY = 0.0f;
    bool Input::s_GameViewRegionActive = false;
    glm::vec2 Input::s_GameViewRegionMin{ 0.0f };
    glm::vec2 Input::s_GameViewRegionSize{ 0.0f };
    glm::vec2 Input::s_GameViewTargetSize{ 0.0f };
    static UnorderedMap<KeyCode, bool> s_KeyStates(0);
    static UnorderedMap<MouseCode, bool> s_MouseStates(0);

    static Vector<KeyCode> s_Keys = { Key::Space,
                                      Key::Apostrophe,
                                      Key::Comma,
                                      Key::Minus,
                                      Key::Period,
                                      Key::Slash,
                                      Key::D0,
                                      Key::D1,
                                      Key::D2,
                                      Key::D3,
                                      Key::D4,
                                      Key::D5,
                                      Key::D6,
                                      Key::D7,
                                      Key::D8,
                                      Key::D9,
                                      Key::Semicolon,
                                      Key::Equal,
                                      Key::A,
                                      Key::B,
                                      Key::C,
                                      Key::D,
                                      Key::E,
                                      Key::F,
                                      Key::G,
                                      Key::H,
                                      Key::I,
                                      Key::J,
                                      Key::K,
                                      Key::L,
                                      Key::M,
                                      Key::N,
                                      Key::O,
                                      Key::P,
                                      Key::Q,
                                      Key::R,
                                      Key::S,
                                      Key::T,
                                      Key::U,
                                      Key::V,
                                      Key::W,
                                      Key::X,
                                      Key::Y,
                                      Key::Z,
                                      Key::LeftBracket,
                                      Key::Backslash,
                                      Key::RightBracket,
                                      Key::GraveAccent,
                                      Key::World1,
                                      Key::World2,
                                      Key::Escape,
                                      Key::Enter,
                                      Key::Tab,
                                      Key::Backspace,
                                      Key::Insert,
                                      Key::Delete,
                                      Key::Right,
                                      Key::Left,
                                      Key::Down,
                                      Key::Up,
                                      Key::PageUp,
                                      Key::PageDown,
                                      Key::Home,
                                      Key::End,
                                      Key::CapsLock,
                                      Key::ScrollLock,
                                      Key::NumLock,
                                      Key::PrintScreen,
                                      Key::Pause,
                                      Key::F1,
                                      Key::F2,
                                      Key::F3,
                                      Key::F4,
                                      Key::F5,
                                      Key::F6,
                                      Key::F7,
                                      Key::F8,
                                      Key::F9,
                                      Key::F10,
                                      Key::F11,
                                      Key::F12,
                                      Key::F13,
                                      Key::F14,
                                      Key::F15,
                                      Key::F16,
                                      Key::F17,
                                      Key::F18,
                                      Key::F19,
                                      Key::F20,
                                      Key::F21,
                                      Key::F22,
                                      Key::F23,
                                      Key::F24,
                                      Key::F25,
                                      Key::KP0,
                                      Key::KP1,
                                      Key::KP2,
                                      Key::KP3,
                                      Key::KP4,
                                      Key::KP5,
                                      Key::KP6,
                                      Key::KP7,
                                      Key::KP8,
                                      Key::KP9,
                                      Key::KPDecimal,
                                      Key::KPDivide,
                                      Key::KPMultiply,
                                      Key::KPSubtract,
                                      Key::KPAdd,
                                      Key::KPEnter,
                                      Key::KPEqual,
                                      Key::LeftShift,
                                      Key::LeftControl,
                                      Key::LeftAlt,
                                      Key::LeftSuper,
                                      Key::RightShift,
                                      Key::RightControl,
                                      Key::RightAlt,
                                      Key::RightSuper,
                                      Key::Menu };

    static Vector<MouseCode> s_MouseButtons = { Mouse::Button0, Mouse::Button1, Mouse::Button2, Mouse::Button3,
                                                Mouse::Button4, Mouse::Button5, Mouse::Button6, Mouse::Button7 };

    bool Input::GetKey(const KeyCode key)
    {
        auto* window = static_cast<GLFWwindow*>(Application::TryGet()->GetWindow().GetNativeWindow());
        auto state = glfwGetKey(window, static_cast<int32_t>(key));
        return state == GLFW_PRESS;
    }

    bool Input::GetMouseButton(const MouseCode btn)
    {
        auto* window = static_cast<GLFWwindow*>(Application::TryGet()->GetWindow().GetNativeWindow());
        auto state = glfwGetMouseButton(window, btn);
        return state == GLFW_PRESS;
    }

    // Returns if the key is pressed
    bool Input::IsKeyPressed(const KeyCode key)
    {
        auto* window = static_cast<GLFWwindow*>(Application::TryGet()->GetWindow().GetNativeWindow());
        auto state = glfwGetKey(window, key);
        return state == GLFW_PRESS || state == GLFW_REPEAT;
    }

    // Returns true if the key was not pressed last frame and now is
    bool Input::IsKeyDown(const KeyCode key) { return GetKey(key) && !s_KeyStates[key]; }

    // Returns true if key was pressed last frame and now isn't
    bool Input::IsKeyUp(const KeyCode key) { return !GetKey(key) && s_KeyStates[key]; }

    bool Input::IsMouseButtonPressed(const MouseCode btn) { return GetMouseButton(btn); }

    bool Input::IsMouseButtonDown(const MouseCode btn) { return GetMouseButton(btn) && !s_MouseStates[btn]; }

    bool Input::IsMouseButtonUp(const MouseCode btn) { return !GetMouseButton(btn) && s_MouseStates[btn]; }

    glm::vec2 Input::GetMousePosition()
    {
        auto window = static_cast<GLFWwindow*>(Application::TryGet()->GetWindow().GetNativeWindow());
        double xpos, ypos;
        glfwGetCursorPos(window, &xpos, &ypos);

        return { (float)xpos, (float)ypos };
    }

    float Input::GetMouseScrollX()
    {
        return m_FrameScrollX;
    }

    float Input::GetMouseScrollY()
    {
        return m_FrameScrollY;
    }

    void Input::OnMouseScroll(float xOffset, float yOffset)
    {
        m_FrameScrollX += xOffset;
        m_FrameScrollY += yOffset;
    }

    float Input::GetMouseX() { return GetMousePosition().x; }

    float Input::GetMouseY() { return GetMousePosition().y; }

    void Input::SetMousePosition(const glm::vec2& pos)
    {
        auto window = static_cast<GLFWwindow*>(Application::TryGet()->GetWindow().GetNativeWindow());
        glfwSetCursorPos(window, pos.x, pos.y);
    }

    void Input::SetMouseGrabbed(bool grabbed) { s_Grabbed = grabbed; }

    bool Input::IsMouseGrabbed() { return s_Grabbed; }

    void Input::SetCursorType(Cursor type)
    {
        if (Application::TryGet() != nullptr && !Application::TryGet()->GetApplicationDesc().Headless)
            Application::TryGet()->GetWindow().SetCursor(type);
    }

    Cursor Input::GetCursorType()
    {
        if (Application::TryGet() != nullptr && !Application::TryGet()->GetApplicationDesc().Headless)
            return Application::TryGet()->GetWindow().GetCursor();
        return Cursor::POINTER;
    }

    void Input::SetGameViewRegion(bool active, const glm::vec2& regionMin, const glm::vec2& regionSize,
                                  const glm::vec2& targetSize)
    {
        s_GameViewRegionActive = active;
        s_GameViewRegionMin = regionMin;
        s_GameViewRegionSize = regionSize;
        s_GameViewTargetSize = targetSize;
    }

    glm::vec2 Input::GetGameMousePosition()
    {
        const glm::vec2 raw = GetMousePosition();
        if (s_GameViewRegionActive && s_GameViewRegionSize.x > 0.0f && s_GameViewRegionSize.y > 0.0f &&
            s_GameViewTargetSize.x > 0.0f && s_GameViewTargetSize.y > 0.0f)
        {
            const glm::vec2 uv = (raw - s_GameViewRegionMin) / s_GameViewRegionSize;
            return { uv.x * s_GameViewTargetSize.x, (1.0f - uv.y) * s_GameViewTargetSize.y };
        }

        // Unity parity at runtime: framebuffer pixels with a bottom-left origin.
        Application* application = Application::TryGet();
        if (application == nullptr || application->GetApplicationDesc().Headless)
            return raw;
        const Window& window = application->GetWindow();
        const glm::vec2 contentScale = window.GetContentScale();
        return { raw.x * contentScale.x, static_cast<float>(window.GetFramebufferHeight()) - raw.y * contentScale.y };
    }

    void Input::OnUpdate()
    {
        for (KeyCode key : s_Keys)
        {
            s_KeyStates[key] = GetKey(key);
        }

        for (MouseCode btn : s_MouseButtons)
        {
            s_MouseStates[btn] = GetMouseButton(btn);
        }
        m_FrameScrollX = m_FrameScrollY = 0;
    }

} // namespace Crowny
#endif