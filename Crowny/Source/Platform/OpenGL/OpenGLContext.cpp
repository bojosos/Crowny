#include "cwpch.h"

#include "Platform/OpenGL/OpenGLContext.h"

#include <GLFW/glfw3.h>
#include <glad/glad.h>

namespace Crowny
{
    OpenGLContext::OpenGLContext(void* window) : m_Window(window) {}

    void OpenGLContext::Init()
    {
        glfwMakeContextCurrent((GLFWwindow*)m_Window);
        int status = gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

        CW_ENGINE_ASSERT(status, "Failed to initialize GLAD!");
    }

    void OpenGLContext::SwapBuffers() { glfwSwapBuffers((GLFWwindow*)m_Window); }
} // namespace Crowny