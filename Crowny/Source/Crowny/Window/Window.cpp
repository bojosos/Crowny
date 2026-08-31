#include "cwpch.h"

#include "Crowny/Application/Application.h"
#include "Crowny/Events/ApplicationEvent.h"
#include "Crowny/Events/KeyEvent.h"
#include "Crowny/Events/MouseEvent.h"
#include "Crowny/Input/Input.h"
#include "Crowny/Window/Window.h"
#include "Crowny/Window/WindowSystemState.h"

#include "Platform/Linux/LinuxWindow.h"

#include <GLFW/glfw3.h>

namespace Crowny
{
    namespace
    {
        Detail::WindowSystemState s_WindowSystemState;

        void GLFWErrorCallback(int error, const char* description)
        {
            CW_ENGINE_ERROR("GLFW error {}: {}", error, description != nullptr ? description : "Unknown error");
        }
    } // namespace

    bool Window::Initialize()
    {
        if (s_WindowSystemState.IsInitialized())
        {
            s_WindowSystemState.CancelPendingShutdown();
            return true;
        }

        glfwSetErrorCallback(GLFWErrorCallback);
        if (glfwInit() != GLFW_TRUE)
        {
            s_WindowSystemState.MarkInitializationFailed();
            CW_ENGINE_ERROR("Could not initialize GLFW");
            return false;
        }

        s_WindowSystemState.MarkInitialized();
        return true;
    }

    void Window::Shutdown()
    {
        if (s_WindowSystemState.RequestShutdown() == Detail::WindowSystemAction::TerminateBackend)
            glfwTerminate();
    }

    bool Window::IsInitialized() { return s_WindowSystemState.IsInitialized(); }

    void Window::PollEvents()
    {
        if (s_WindowSystemState.IsInitialized())
            glfwPollEvents();
    }

    bool Window::RegisterNativeWindow() { return s_WindowSystemState.RegisterWindow(); }

    void Window::UnregisterNativeWindow()
    {
        if (s_WindowSystemState.UnregisterWindow() == Detail::WindowSystemAction::TerminateBackend)
            glfwTerminate();
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
