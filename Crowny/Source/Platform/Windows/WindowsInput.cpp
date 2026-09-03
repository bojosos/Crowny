#include "cwpch.h"

#include "Crowny/Application/Application.h"
#include "Crowny/Input/Input.h"
#include "Crowny/Input/InputMap.h"
#include "Crowny/Window/Window.h"

#include <GLFW/glfw3.h>
#include <array>

namespace Crowny
{
    namespace
    {
        constexpr size_t GamepadCount = GLFW_JOYSTICK_LAST + 1;
        constexpr size_t GamepadButtonCount = GamepadButton::DPadLeft + 1;
        constexpr size_t GamepadAxisCount = GamepadAxis::RightTrigger + 1;

        using ButtonState = Detail::InputButtonState;

        struct GamepadState
        {
            std::array<ButtonState, GamepadButtonCount> Buttons{};
            std::array<float, GamepadAxisCount> Axes{};
            bool Connected = false;
        };

        std::array<ButtonState, GLFW_KEY_LAST + 1> s_Keys{};
        std::array<ButtonState, GLFW_MOUSE_BUTTON_LAST + 1> s_MouseButtons{};
        std::array<GamepadState, GamepadCount> s_Gamepads{};
        Scope<InputMap> s_ActionMap;
        glm::vec2 s_MousePosition{ 0.0f };
        bool s_MousePositionInitialized = false;
        bool s_AcceptsInput = true;

        GLFWwindow* GetMainGLFWWindow()
        {
            if (Application::TryGet() == nullptr || Application::TryGet()->GetApplicationDesc().Headless)
                return nullptr;
            return static_cast<GLFWwindow*>(Application::TryGet()->GetWindow().GetNativeWindow());
        }

        bool IsValidKey(KeyCode key) { return key <= GLFW_KEY_LAST; }

        bool IsValidMouseButton(MouseCode button) { return button <= GLFW_MOUSE_BUTTON_LAST; }

        bool IsValidGamepad(uint32_t gamepadIndex) { return gamepadIndex < s_Gamepads.size(); }

        bool IsValidGamepadButton(GamepadButtonCode button) { return button < GamepadButtonCount; }

        bool IsValidGamepadAxis(GamepadAxisCode axis) { return axis < GamepadAxisCount; }

        void ReleaseButtons(auto& buttons)
        {
            for (ButtonState& button : buttons)
                button.SetHeld(false);
        }

        void ClearGamepadInput(GamepadState& gamepad)
        {
            ReleaseButtons(gamepad.Buttons);
            gamepad.Axes.fill(0.0f);
        }
    } // namespace

    bool Input::s_Grabbed = false;
    float Input::m_FrameScrollX = 0.0f;
    float Input::m_FrameScrollY = 0.0f;
    glm::vec2 Input::s_MouseDelta{ 0.0f };
    bool Input::s_GameViewRegionActive = false;
    glm::vec2 Input::s_GameViewRegionMin{ 0.0f };
    glm::vec2 Input::s_GameViewRegionSize{ 0.0f };
    glm::vec2 Input::s_GameViewTargetSize{ 0.0f };

    bool Input::GetKey(const KeyCode key) { return IsValidKey(key) && s_Keys[key].IsHeld(); }

    bool Input::GetMouseButton(const MouseCode button) { return IsValidMouseButton(button) && s_MouseButtons[button].IsHeld(); }

    bool Input::IsKeyPressed(const KeyCode key) { return GetKey(key); }

    bool Input::IsKeyDown(const KeyCode key) { return IsValidKey(key) && s_Keys[key].WasPressed(); }

    bool Input::IsKeyUp(const KeyCode key) { return IsValidKey(key) && s_Keys[key].WasReleased(); }

    bool Input::IsMouseButtonPressed(const MouseCode button) { return GetMouseButton(button); }

    bool Input::IsMouseButtonDown(const MouseCode button) { return IsValidMouseButton(button) && s_MouseButtons[button].WasPressed(); }

    bool Input::IsMouseButtonUp(const MouseCode button) { return IsValidMouseButton(button) && s_MouseButtons[button].WasReleased(); }

    glm::vec2 Input::GetMousePosition()
    {
        if (!s_MousePositionInitialized)
        {
            GLFWwindow* window = GetMainGLFWWindow();
            if (window == nullptr)
                return glm::vec2(0.0f);

            double x = 0.0;
            double y = 0.0;
            glfwGetCursorPos(window, &x, &y);
            s_MousePosition = { static_cast<float>(x), static_cast<float>(y) };
            s_MousePositionInitialized = true;
        }

        return s_MousePosition;
    }

    glm::vec2 Input::GetMouseDelta() { return s_MouseDelta; }

    void Input::OnMouseScroll(float xOffset, float yOffset)
    {
        if (!s_AcceptsInput)
            return;
        m_FrameScrollX += xOffset;
        m_FrameScrollY += yOffset;
    }

    float Input::GetMouseX() { return GetMousePosition().x; }

    float Input::GetMouseY() { return GetMousePosition().y; }

    float Input::GetMouseDeltaX() { return s_MouseDelta.x; }

    float Input::GetMouseDeltaY() { return s_MouseDelta.y; }

    void Input::SetMousePosition(const glm::vec2& position)
    {
        if (GLFWwindow* window = GetMainGLFWWindow())
        {
            glfwSetCursorPos(window, position.x, position.y);
            OnMousePosition(position.x, position.y);
        }
    }

    void Input::SetMouseGrabbed(bool grabbed)
    {
        s_Grabbed = grabbed;
        if (Application::TryGet() != nullptr && !Application::TryGet()->GetApplicationDesc().Headless)
            Application::TryGet()->GetWindow().SetCursorGrabbed(grabbed);
    }

    bool Input::IsMouseGrabbed()
    {
        if (Application::TryGet() != nullptr && !Application::TryGet()->GetApplicationDesc().Headless)
            return Application::TryGet()->GetWindow().IsCursorGrabbed();
        return s_Grabbed;
    }

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

    float Input::GetMouseScrollX() { return m_FrameScrollX; }

    float Input::GetMouseScrollY() { return m_FrameScrollY; }

    bool Input::IsGamepadConnected(uint32_t gamepadIndex) { return IsValidGamepad(gamepadIndex) && s_Gamepads[gamepadIndex].Connected; }

    bool Input::IsGamepadButtonPressed(uint32_t gamepadIndex, GamepadButtonCode button)
    {
        return IsValidGamepad(gamepadIndex) && IsValidGamepadButton(button) && s_Gamepads[gamepadIndex].Buttons[button].IsHeld();
    }

    bool Input::IsGamepadButtonDown(uint32_t gamepadIndex, GamepadButtonCode button)
    {
        return IsValidGamepad(gamepadIndex) && IsValidGamepadButton(button) && s_Gamepads[gamepadIndex].Buttons[button].WasPressed();
    }

    bool Input::IsGamepadButtonUp(uint32_t gamepadIndex, GamepadButtonCode button)
    {
        return IsValidGamepad(gamepadIndex) && IsValidGamepadButton(button) && s_Gamepads[gamepadIndex].Buttons[button].WasReleased();
    }

    float Input::GetGamepadAxis(uint32_t gamepadIndex, GamepadAxisCode axis)
    {
        if (!IsValidGamepad(gamepadIndex) || !IsValidGamepadAxis(axis) || !s_Gamepads[gamepadIndex].Connected)
            return 0.0f;
        return s_Gamepads[gamepadIndex].Axes[axis];
    }

    void Input::SetActionMap(const InputMap& actionMap) { s_ActionMap = CreateScope<InputMap>(actionMap); }

    void Input::ClearActionMap() { s_ActionMap.reset(); }

    bool Input::HasActionMap() { return s_ActionMap != nullptr; }

    bool Input::GetAction(StringView actionName) { return s_ActionMap && s_ActionMap->IsHeld(actionName); }

    bool Input::GetActionDown(StringView actionName) { return s_ActionMap && s_ActionMap->IsPressed(actionName); }

    bool Input::GetActionUp(StringView actionName) { return s_ActionMap && s_ActionMap->IsReleased(actionName); }

    float Input::GetAxis(StringView actionName) { return s_ActionMap ? s_ActionMap->GetAxis1D(actionName) : 0.0f; }

    glm::vec2 Input::GetActionVector(StringView actionName) { return s_ActionMap ? s_ActionMap->GetAxis2D(actionName) : glm::vec2(0.0f); }

    bool Input::SetActionMapEnabled(StringView mapName, bool enabled) { return s_ActionMap && s_ActionMap->SetContextEnabled(mapName, enabled); }

    void Input::ClearActionRebinds()
    {
        if (s_ActionMap)
            s_ActionMap->ClearAllRebinds();
    }

    void Input::SetActionCapture(bool keyboardCaptured, bool pointerCaptured)
    {
        if (s_ActionMap)
            s_ActionMap->SetCapture(keyboardCaptured, pointerCaptured);
    }

    void Input::OnKeyState(int32_t key, bool pressed)
    {
        if (s_AcceptsInput && key >= 0 && key <= GLFW_KEY_LAST)
            s_Keys[static_cast<size_t>(key)].SetHeld(pressed);
    }

    void Input::OnMouseButtonState(int32_t button, bool pressed)
    {
        if (s_AcceptsInput && button >= 0 && button <= GLFW_MOUSE_BUTTON_LAST)
            s_MouseButtons[static_cast<size_t>(button)].SetHeld(pressed);
    }

    glm::vec2 Input::OnMousePosition(float x, float y)
    {
        const glm::vec2 position(x, y);
        glm::vec2 delta(0.0f);
        if (s_AcceptsInput && s_MousePositionInitialized)
        {
            delta = position - s_MousePosition;
            s_MouseDelta += delta;
        }
        s_MousePosition = position;
        s_MousePositionInitialized = true;
        return delta;
    }

    void Input::OnFocusChanged(bool focused)
    {
        s_AcceptsInput = focused;
        s_MousePositionInitialized = false;
        if (!focused)
            ResetState();
    }

    void Input::ResetState()
    {
        ReleaseButtons(s_Keys);
        ReleaseButtons(s_MouseButtons);
        for (GamepadState& gamepad : s_Gamepads)
            ClearGamepadInput(gamepad);
        m_FrameScrollX = 0.0f;
        m_FrameScrollY = 0.0f;
        s_MouseDelta = glm::vec2(0.0f);
    }

    void Input::BeginFrame()
    {
        for (ButtonState& key : s_Keys)
            key.BeginFrame();
        for (ButtonState& button : s_MouseButtons)
            button.BeginFrame();
        for (GamepadState& gamepad : s_Gamepads)
        {
            for (ButtonState& button : gamepad.Buttons)
                button.BeginFrame();
        }

        m_FrameScrollX = 0.0f;
        m_FrameScrollY = 0.0f;
        s_MouseDelta = glm::vec2(0.0f);

        if (s_AcceptsInput && !s_MousePositionInitialized)
            GetMousePosition();
    }

    void Input::UpdateGamepads()
    {
        for (uint32_t gamepadIndex = 0; gamepadIndex < s_Gamepads.size(); ++gamepadIndex)
        {
            GamepadState& gamepad = s_Gamepads[gamepadIndex];
            GLFWgamepadstate glfwState{};
            const bool connected = glfwJoystickIsGamepad(static_cast<int>(gamepadIndex)) == GLFW_TRUE &&
                                   glfwGetGamepadState(static_cast<int>(gamepadIndex), &glfwState) == GLFW_TRUE;
            gamepad.Connected = connected;
            if (!connected || !s_AcceptsInput)
            {
                ClearGamepadInput(gamepad);
                continue;
            }

            for (size_t button = 0; button < gamepad.Buttons.size(); ++button)
                gamepad.Buttons[button].SetHeld(glfwState.buttons[button] == GLFW_PRESS);
            for (size_t axis = 0; axis < gamepad.Axes.size(); ++axis)
                gamepad.Axes[axis] = std::clamp(glfwState.axes[axis], -1.0f, 1.0f);
        }
    }

    void Input::UpdateActions()
    {
        if (s_ActionMap)
            s_ActionMap->Update();
    }

    void Input::OnUpdate() { BeginFrame(); }

} // namespace Crowny
