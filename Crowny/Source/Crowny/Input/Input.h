#pragma once

#include "Crowny/Common/Common.h"
#include "Crowny/Common/Flags.h"
#include "Crowny/Input/GamepadCodes.h"
#include "Crowny/Input/KeyCodes.h"
#include "Crowny/Input/MouseCodes.h"

namespace Crowny
{
    enum class InputModifierBits : uint8_t
    {
        None = 0,
        Shift = BIT(0),
        Control = BIT(1),
        Alt = BIT(2),
        Super = BIT(3),
        CapsLock = BIT(4),
        NumLock = BIT(5)
    };

    using InputModifiers = Flags<InputModifierBits, uint8_t>;

    enum class Cursor
    {
        NO_CURSOR,
        POINTER,
        IBEAM,
        CROSSHAIR,
        HAND,
        HRESIZE,
        VRESIZE,
        STOPSIGN
    };

    class Application;
    class InputMap;
    class LinuxWindow;

    // TODO: Make this a module!
    class Input
    {
    public:
        static bool IsKeyPressed(const KeyCode key);
        static bool IsKeyDown(const KeyCode key);
        static bool IsKeyUp(const KeyCode key);

        static bool IsMouseButtonPressed(const MouseCode button);
        static bool IsMouseButtonDown(const MouseCode button);
        static bool IsMouseButtonUp(const MouseCode button);

        static glm::vec2 GetMousePosition();
        static glm::vec2 GetMouseDelta();
        static float GetMouseX();
        static float GetMouseY();
        static float GetMouseDeltaX();
        static float GetMouseDeltaY();
        static void SetMouseGrabbed(bool grabbed);
        static bool IsMouseGrabbed();
        static void SetMousePosition(const glm::vec2& position);

        static float GetMouseScrollX();
        static float GetMouseScrollY();

        static bool IsGamepadConnected(uint32_t gamepadIndex);
        static bool IsGamepadButtonPressed(uint32_t gamepadIndex, GamepadButtonCode button);
        static bool IsGamepadButtonDown(uint32_t gamepadIndex, GamepadButtonCode button);
        static bool IsGamepadButtonUp(uint32_t gamepadIndex, GamepadButtonCode button);
        // GLFW's standardized mapping uses [-1, 1] for sticks and triggers.
        static float GetGamepadAxis(uint32_t gamepadIndex, GamepadAxisCode axis);

        static void SetActionMap(const InputMap& actionMap);
        static void ClearActionMap();
        static bool HasActionMap();
        static bool GetAction(StringView actionName);
        static bool GetActionDown(StringView actionName);
        static bool GetActionUp(StringView actionName);
        static float GetAxis(StringView actionName);
        static glm::vec2 GetActionVector(StringView actionName);
        static bool SetActionMapEnabled(StringView mapName, bool enabled);
        static void ClearActionRebinds();
        static void SetActionCapture(bool keyboardCaptured, bool pointerCaptured);

        static void OnMouseScroll(float xOffset, float yOffset);

    private:
        friend class Application;
        static void BeginFrame();
        static void UpdateGamepads();
        static void UpdateActions();
        // Kept while engine modules migrate to the explicit frame name.
        static void OnUpdate();

        static bool GetKey(const KeyCode key);
        static bool GetMouseButton(const MouseCode button);

    private:
        friend class LinuxWindow;
        static void OnKeyState(int32_t key, bool pressed);
        static void OnMouseButtonState(int32_t button, bool pressed);
        static glm::vec2 OnMousePosition(float x, float y);
        static void OnFocusChanged(bool focused);
        static void ResetState();

        static float m_FrameScrollX, m_FrameScrollY;
        static glm::vec2 s_MouseDelta;
        static bool s_Grabbed;
    };

    namespace Detail
    {
        // The platform backend uses this small state machine for keys, mouse
        // buttons, and gamepad buttons. Keeping it independent of GLFW makes
        // same-frame transition behavior cheap to test.
        class InputButtonState
        {
        public:
            void BeginFrame()
            {
                m_Pressed = false;
                m_Released = false;
            }

            void SetHeld(bool held)
            {
                if (held == m_Held)
                    return;

                m_Held = held;
                if (held)
                    m_Pressed = true;
                else
                    m_Released = true;
            }

            void Clear()
            {
                m_Held = false;
                m_Pressed = false;
                m_Released = false;
            }

            bool IsHeld() const { return m_Held; }
            bool WasPressed() const { return m_Pressed; }
            bool WasReleased() const { return m_Released; }

        private:
            bool m_Held = false;
            bool m_Pressed = false;
            bool m_Released = false;
        };
    } // namespace Detail
} // namespace Crowny
