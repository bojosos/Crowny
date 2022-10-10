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

    LinuxWindow::LinuxWindow(const WindowProperties& props) { Init(props); }

    void LinuxWindow::OnUpdate()
    {
        glfwPollEvents();
        // m_Context->SwapBuffers();

        // auto& rapi = RenderAPI::Get();
        // rapi.SwapBuffers();
    }

    LinuxWindow::~LinuxWindow() { Shutdown(); }

    void LinuxWindow::Init(const WindowProperties& props)
    {
        m_Data.Title = props.Title;
        m_Data.Width = props.Width;
        m_Data.Height = props.Height;

#ifdef CW_DEBUG
        CW_ENGINE_WARN("Creating Window: {0}", m_Data.Title);
#endif
        glfwSetErrorCallback(GLFWErrorCallback);

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_SAMPLES, 8);

        int32_t monitorCount = 0;
        GLFWmonitor** monitors = glfwGetMonitors(&monitorCount);
        for (uint32_t i = 0; i < monitorCount; i++)
            CW_ENGINE_INFO("Monitor: {0}, {1}", i, glfwGetMonitorName(monitors[i]));

        /*int xpos, ypos;
        glfwGetMonitorPos(monitors[1], &xpos, &ypos);
        CW_ENGINE_INFO("{}, {}", xpos, ypos);*/
        // glfwWindowHint(GLFW_MAXIMIZED, true);
        m_Window = glfwCreateWindow((int)props.Width, (int)props.Height, m_Data.Title.c_str(), nullptr, nullptr);
        /*int xpos2, ypos2;
        glfwGetWindowPos(m_Window, &xpos2, &ypos2);
        CW_ENGINE_INFO("{}, {}", xpos2, ypos2);
        glfwSetWindowPos(m_Window, xpos, ypos2)*/
        ;
#ifdef CW_DEBUG
        if (Renderer::GetAPI() == RenderAPI::API::OpenGL)
            glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT,
                           GLFW_TRUE); // No idea what this does...
                                       // https://www.khronos.org/registry/OpenGL/extensions/KHR/KHR_debug.txt
#endif
        Application::s_GLFWWindowCount++;

        // m_Context = GraphicsContext::Create(m_Window);
        // m_Context->Init();
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

	void LinuxWindow::SetTitle(const String& title) {
		glfwSetWindowTitle(m_Window, title.c_str());
		m_Data.Title = title;
	}

    void LinuxWindow::Shutdown()
    {
        glfwDestroyWindow(m_Window);
        Application::s_GLFWWindowCount--;

        if (Application::s_GLFWWindowCount == 0)
            glfwTerminate();
    }

    void LinuxWindow::SetVSync(bool enabled)
    {
        // if (enabled)
        // glfwSwapInterval(1);
        // else
        // glfwSwapInterval(0);
        //
        // m_Data.VSync = enabled;
    }

    bool LinuxWindow::IsVSync() const { return m_Data.VSync; }
} // namespace Crowny
  // #endif