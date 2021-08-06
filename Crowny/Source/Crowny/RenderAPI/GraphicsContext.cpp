#include "cwpch.h"

#include "Crowny/RenderAPI/GraphicsContext.h"
#include "Crowny/Renderer/Renderer.h"

#include "Platform/OpenGL/OpenGLContext.h"

namespace Crowny
{

    Scope<GraphicsContext> GraphicsContext::Create(void* window)
    {
        switch (Renderer::GetAPI())
        {
        // TODO: Do not tie OpenGL and GLFW
        case RenderAPI::API::OpenGL:
            return CreateScope<OpenGLContext>(window);
        default:
            CW_ENGINE_ASSERT(false, "Renderer API not supporter");
            return nullptr;
        }

        return nullptr;
    }

} // namespace Crowny
