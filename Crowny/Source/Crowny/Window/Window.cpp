#include "cwpch.h"

#include "Crowny/Application/Application.h"
#include "Crowny/Events/ApplicationEvent.h"
#include "Crowny/Events/KeyEvent.h"
#include "Crowny/Events/MouseEvent.h"
#include "Crowny/Input/Input.h"
#include "Crowny/Window/Window.h"

#include "Platform/Linux/LinuxWindow.h"

#include <GLFW/glfw3.h>

namespace Crowny
{
    namespace
    {
        bool s_WindowSystemInitialized = false;

        void GLFWErrorCallback(int error, const char* description)
        {
            CW_ENGINE_ERROR("GLFW error {}: {}", error, description != nullptr ? description : "Unknown error");
        }
    } // namespace

    bool Window::Initialize()
    {
        if (s_WindowSystemInitialized)
            return true;

        glfwSetErrorCallback(GLFWErrorCallback);
        s_WindowSystemInitialized = glfwInit() == GLFW_TRUE;
        if (!s_WindowSystemInitialized)
            CW_ENGINE_ERROR("Could not initialize GLFW");
        return s_WindowSystemInitialized;
    }

    void Window::Shutdown()
    {
        if (!s_WindowSystemInitialized)
            return;
        glfwTerminate();
        s_WindowSystemInitialized = false;
    }

    bool Window::IsInitialized() { return s_WindowSystemInitialized; }

    void Window::PollEvents()
    {
        if (s_WindowSystemInitialized)
            glfwPollEvents();
    }

    Scope<Window> Window::Create(const WindowDesc& windowDesc)
    {
#if defined(CW_WINDOWS) || defined(CW_PLATFORM_LINUX) || defined(CW_MACOSX)
        if (!Initialize())
            return nullptr;
        return CreateScope<LinuxWindow>(windowDesc);
#else
        CW_ENGINE_ASSERT(false, "Platform not supported");
        return nullptr;
#endif
    }

} // namespace Crowny
