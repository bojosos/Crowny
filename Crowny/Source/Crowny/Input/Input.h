#pragma once

#include "Crowny/Common/Common.h"
#include "Crowny/Input/KeyCodes.h"
#include "Crowny/Input/MouseCodes.h"

namespace Crowny
{
    enum class Cursor
    {
        NO_CURSOR,
        POINTER,
        IBEAM,
        CROSSHAIR,
        HAND,
        HRESIZE,
        VRESIZE
    };

    class Application;
    class LinuxWindow;
    class WindowsWindow;

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
        static float GetMouseX();
        static float GetMouseY();
        static void SetMouseGrabbed(bool grabbed);
        static bool IsMouseGrabbed();
        static void SetMousePosition(const glm::vec2& position);

        static float GetMouseScrollX();
        static float GetMouseScrollY();

        static void OnMouseScroll(float xOffset, float yOffset);
    private:
        friend class Application;
        static void OnUpdate();

        static bool GetKey(const KeyCode key);
        static bool GetMouseButton(const MouseCode button);

    private:
        friend class LinuxWindow;
        friend class WindowsWindow;

        static float m_FrameScrollX, m_FrameScrollY;
        static bool s_Grabbed;
    };
} // namespace Crowny