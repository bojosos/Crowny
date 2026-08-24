#include "cwpch.h"

#include "Platform/OpenGL/OpenGLContext.h"

#include <GLFW/glfw3.h>
#include <glad/glad.h>

#include <stdexcept>

namespace Crowny
{
    OpenGLContext::OpenGLContext(void* window) : m_Window(window)
    {
        if (m_Window == nullptr)
            throw std::invalid_argument("OpenGLContext requires a valid GLFW window");
    }

    void OpenGLContext::Init()
    {
        MakeCurrent();
        if (gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)) == 0)
            throw std::runtime_error("Could not load OpenGL entry points through GLFW");

        const char* version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
        const char* renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
        if (version == nullptr || renderer == nullptr)
            throw std::runtime_error("OpenGL context did not report a renderer or version");
        CW_ENGINE_INFO("OpenGL {} on {}", version, renderer);
    }

    void OpenGLContext::SwapBuffers()
    {
        MakeCurrent();
        glfwSwapBuffers(static_cast<GLFWwindow*>(m_Window));
    }

    void OpenGLContext::MakeCurrent() const { glfwMakeContextCurrent(static_cast<GLFWwindow*>(m_Window)); }

    void OpenGLContext::SetSwapInterval(int interval) const
    {
        MakeCurrent();
        glfwSwapInterval(interval);
    }
} // namespace Crowny
