#include "cwpch.h"

// #if 0
#include "Crowny/Application/Application.h"
#include "Crowny/Events/ApplicationEvent.h"
#include "Crowny/Events/KeyEvent.h"
#include "Crowny/Events/MouseEvent.h"
#include "Crowny/Renderer/Renderer.h"

#include "Platform/Linux/LinuxWindow.h"

namespace Crowny
{

    static void GLFWErrorCallback(int error, const char* desc) { CW_ENGINE_ERROR("GLFW Error ({0})", desc); }

    LinuxWindow::LinuxWindow(const WindowDesc& windowDesc) { Init(windowDesc); }

    LinuxWindow::~LinuxWindow() { Shutdown(); }

    void LinuxWindow::OnUpdate() { glfwPollEvents(); }

    void LinuxWindow::Init(const WindowDesc& windowDesc)
    {
        m_Data.Title = windowDesc.Title;
        m_Data.Width = windowDesc.Width;
        m_Data.Height = windowDesc.Height;
        // m_Data.VSync = windowDesc.VSync;

#ifdef CW_DEBUG
        CW_ENGINE_WARN("Creating Window: {0}", m_Data.Title);
#endif
        glfwSetErrorCallback(GLFWErrorCallback);

        glfwWindowHint(GLFW_DOUBLEBUFFER, GLFW_TRUE);
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

        if (!windowDesc.ShowTitleBar)
        {
            glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
        }

        if (windowDesc.StartMaximized || windowDesc.Hidden)
        {
            glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
        }

        if (!windowDesc.AllowResize)
        {
            glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
        }

        int32_t monitorCount = 0;
        GLFWmonitor** monitors = glfwGetMonitors(&monitorCount);
        const uint32_t monitorIdx = windowDesc.MonitorIdx;
        CW_ENGINE_ASSERT(monitorCount > (int32_t)monitorIdx);
        int areaX, areaY, areaWidth, areaHeight;
        GLFWmonitor* monitor = monitors[monitorIdx];
        glfwGetMonitorWorkarea(monitor, &areaX, &areaY, &areaWidth, &areaHeight);

        // Normal fullscreen
        if (windowDesc.ShowBorder && windowDesc.Fullscreen)
        {
            m_Window = glfwCreateWindow((int)windowDesc.Width, (int)windowDesc.Height, windowDesc.Title.c_str(),
                                        monitor, nullptr);
        }
        else if (!windowDesc.ShowBorder && windowDesc.Fullscreen)
        {
            const GLFWvidmode* videoMode = glfwGetVideoMode(monitor);
            glfwWindowHint(GLFW_RED_BITS, videoMode->redBits);
            glfwWindowHint(GLFW_GREEN_BITS, videoMode->greenBits);
            glfwWindowHint(GLFW_BLUE_BITS, videoMode->blueBits);
            glfwWindowHint(GLFW_REFRESH_RATE, videoMode->refreshRate);
            m_Window = glfwCreateWindow((int)windowDesc.Width, (int)windowDesc.Height, windowDesc.Title.c_str(),
                                        monitor, nullptr);
        }
        else if (!windowDesc.ShowBorder)
        { // Borderless windowed
            glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
            m_Window = glfwCreateWindow((int)windowDesc.Width, (int)windowDesc.Height, windowDesc.Title.c_str(),
                                        nullptr, nullptr);
        }
        else
        { // Normal windowed
            m_Window = glfwCreateWindow((int)windowDesc.Width, (int)windowDesc.Height, windowDesc.Title.c_str(),
                                        nullptr, nullptr);
        }

        // TODO: Fix this with monitors
        int32_t left = windowDesc.Left;
        int32_t top = windowDesc.Top;

        {
            int outerWidth = std::clamp((int)windowDesc.Width, 0, areaWidth);
            int outerHeight = std::clamp((int)windowDesc.Height, 0, areaHeight);
            if (left == -1)
                left = areaX + (areaWidth - outerWidth) / 2;
            else
                left += areaX;

            if (top == -1)
                top = areaY + (areaHeight - outerHeight) / 2;
            else
                top += areaY;
        }

        glfwSetWindowPos(m_Window, 0, 0);

        if (windowDesc.StartMaximized)
        {
            glfwSetWindowPos(m_Window, areaX + areaWidth / 2 - windowDesc.Width / 2,
                             areaY + areaHeight / 2 - windowDesc.Height / 2);
            glfwMaximizeWindow(m_Window);
            if (!windowDesc.Hidden)
                glfwShowWindow(m_Window);
        }

#ifdef CW_DEBUG
        if (Renderer::GetAPI() == RenderAPI::API::OpenGL)
            glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT,
                           GLFW_TRUE); // No idea what this does...
                                       // https://www.khronos.org/registry/OpenGL/extensions/KHR/KHR_debug.txt
#endif
        Application::s_GLFWWindowCount++;

        glfwSetWindowUserPointer(m_Window, &m_Data);

        glfwSetWindowSizeCallback(m_Window, [](GLFWwindow* window, int width, int height) {
            WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);
            data.Width = width;
            data.Height = height;

            WindowResizeEvent event(width, height);
            data.EventCallback(event);
        });

        glfwSetWindowCloseCallback(m_Window, [](GLFWwindow* window) {
            WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);
            WindowCloseEvent event;
            data.EventCallback(event);
        });

        glfwSetWindowIconifyCallback(m_Window, [](GLFWwindow* window, int iconified) {
            WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);

            WindowMinimizeEvent event;
            data.EventCallback(event);
        });

        glfwSetWindowPosCallback(m_Window, [](GLFWwindow* window, int x, int y) {
            WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);

            WindowMoveEvent event(x, y);
            data.EventCallback(event);
        });

        glfwSetKeyCallback(m_Window, [](GLFWwindow* window, int key, int scancode, int action, int mods) {
            WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);

            switch (action)
            {
            case GLFW_PRESS: {
                KeyPressedEvent event(key, 0);
                data.EventCallback(event);
                break;
            }
            case GLFW_RELEASE: {
                KeyReleasedEvent event(key);
                data.EventCallback(event);
                break;
            }
            case GLFW_REPEAT: {
                KeyPressedEvent event(key, 1);
                data.EventCallback(event);
                break;
            }
            }
        });

        glfwSetWindowFocusCallback(m_Window, [](GLFWwindow* window, int focus) {
            WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);
            if (focus == GLFW_TRUE)
            {
                WindowFocusEvent event;
                data.EventCallback(event);
            }
            else
            {
                WindowLostFocusEvent event;
                data.EventCallback(event);
            }
        });

        glfwSetCharCallback(m_Window, [](GLFWwindow* window, unsigned int keycode) {
            WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);

            KeyTypedEvent event(keycode);
            data.EventCallback(event);
        });

        glfwSetMouseButtonCallback(m_Window, [](GLFWwindow* window, int button, int action, int mods) {
            WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);

            switch (action)
            {
            case GLFW_PRESS: {
                MouseButtonPressedEvent event(button);
                data.EventCallback(event);
                break;
            }
            case GLFW_RELEASE: {
                MouseButtonReleasedEvent event(button);
                data.EventCallback(event);
                break;
            }
            }
        });

        glfwSetScrollCallback(m_Window, [](GLFWwindow* window, double xOffset, double yOffset) {
            WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);

            MouseScrolledEvent event((float)xOffset, (float)yOffset);
            data.EventCallback(event);
        });

        glfwSetCursorPosCallback(m_Window, [](GLFWwindow* window, double xPos, double yPos) {
            WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);

            MouseMovedEvent event((float)xPos, (float)yPos);
            data.EventCallback(event);
        });
    }

    void LinuxWindow::SetCursor(Cursor cursor)
    {
        if (!m_Cursor)
            glfwSetInputMode(m_Window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

        glfwDestroyCursor(m_Cursor);

        switch (cursor)
        {
        case Cursor::NO_CURSOR:
            m_Cursor = nullptr;
            glfwSetInputMode(m_Window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            break;
        case Cursor::POINTER:
            m_Cursor = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);
            break;
        case Cursor::IBEAM:
            m_Cursor = glfwCreateStandardCursor(GLFW_IBEAM_CURSOR);
            break;
        case Cursor::CROSSHAIR:
            m_Cursor = glfwCreateStandardCursor(GLFW_CROSSHAIR_CURSOR);
            break;
        case Cursor::HAND:
            m_Cursor = glfwCreateStandardCursor(GLFW_HAND_CURSOR);
            break;
        case Cursor::HRESIZE:
            m_Cursor = glfwCreateStandardCursor(GLFW_HRESIZE_CURSOR);
            break;
        case Cursor::VRESIZE:
            m_Cursor = glfwCreateStandardCursor(GLFW_VRESIZE_CURSOR);
            break;
        }

        if (cursor != Cursor::NO_CURSOR)
            glfwSetCursor(m_Window, m_Cursor);
    }

    void LinuxWindow::SetTitle(const String& title)
    {
        glfwSetWindowTitle(m_Window, title.c_str());
        m_Data.Title = title;
    }

    void LinuxWindow::Shutdown()
    {
        glfwDestroyCursor(m_Cursor);
        glfwDestroyWindow(m_Window);
        Application::s_GLFWWindowCount--;

        if (Application::s_GLFWWindowCount == 0)
            glfwTerminate();
    }

    void LinuxWindow::SetHidden(bool hidden)
    {
        if (hidden)
            glfwHideWindow(m_Window);
        else
            glfwShowWindow(m_Window);
    }

    void LinuxWindow::Move(int32_t left, int32_t top) { glfwSetWindowPos(m_Window, left, top); }

    void LinuxWindow::Resize(uint32_t width, uint32_t height) { glfwSetWindowSize(m_Window, width, height); }

    void LinuxWindow::Minimize() { glfwIconifyWindow(m_Window); }

    void LinuxWindow::Maximize() { glfwMaximizeWindow(m_Window); }

    void LinuxWindow::Restore() { glfwRestoreWindow(m_Window); }

} // namespace Crowny
  // #endif