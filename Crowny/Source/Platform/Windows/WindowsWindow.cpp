#include "cwpch.h"

#if 0
#include "Crowny/Application/Application.h"
#include "Crowny/Events/ApplicationEvent.h"
#include "Crowny/Events/KeyEvent.h"
#include "Crowny/Events/MouseEvent.h"
#include "Crowny/Renderer/Renderer.h"

#include "Platform/Windows/WindowsWindow.h"

namespace Crowny
{
    static byte s_GLFWWindowCount = 0;

    static void GLFWErrorCallback(int error, const char* desc) { CW_ENGINE_ERROR("GLFW Error ({0})", desc); }

    WindowsWindow::WindowsWindow(const WindowConfig& config) { Init(config); }

    void WindowsWindow::OnUpdate()
    {
        glfwPollEvents();
        m_Context->SwapBuffers();
    }

    WindowsWindow::~WindowsWindow() { Shutdown(); }

    void WindowsWindow::Init(const WindowConfig& config)
    {
        m_Data.Title = config.Name;
        m_Data.Width = config.Width;
        m_Data.Height = config.Height;

#ifdef CW_DEBUG
        CW_ENGINE_WARN("Creating Window: {0}", m_Data.Title);
#endif

        if (s_GLFWWindowCount == 0)
        {
            int success = glfwInit();
            CW_ENGINE_ASSERT(success, "Could not initialize GLFW!");
            glfwSetErrorCallback(GLFWErrorCallback);
        }

#ifdef CW_DEBUG
        if (Renderer::GetAPI() == RenderAPI::API::OpenGL)
            glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
#endif
        m_Window = glfwCreateWindow((int)config.Width, (int)config.Height, config.Name.c_str(), nullptr, nullptr);
        s_GLFWWindowCount++;

        m_Context = GraphicsContext::Create(m_Window);
        m_Context->Init();

        glfwSetWindowUserPointer(m_Window, &m_Data);
        SetVSync(true);

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

        // This is all good, but I want to be able to highlight the area that the user is dragging over, so I would need
        // to use COM, IDropTarget
        // TODO:a
        glfwSetDropCallback(m_Window, [](GLFWwindow* window, int count, const char** paths) {
            for (int32_t i = 0; i < count; i++)
            {
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

    void WindowsWindow::SetCursor(Cursor cursor)
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

    void WindowsWindow::SetTitle(const String& title)
    {
        glfwSetWindowTitle(m_Window, title.c_str());
        m_Data.Title = title;
    }

    void WindowsWindow::Shutdown()
    {
        glfwDestroyWindow(m_Window);
        s_GLFWWindowCount--;

        if (s_GLFWWindowCount == 0)
            glfwTerminate();
    }

} // namespace Crowny
#endif