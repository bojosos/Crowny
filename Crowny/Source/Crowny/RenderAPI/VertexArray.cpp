#include "cwpch.h"

#include "Crowny/RenderAPI/VertexArray.h"

#include "Crowny/Renderer/Renderer.h"
#include "Platform/OpenGL/OpenGLVertexArray.h"

namespace Crowny
{
    Ref<VertexArray> VertexArray::Create(DrawMode drawMode)
    {
        switch (Renderer::GetAPI())
        {
        case RenderAPI::API::OpenGL:
            return CreateRef<OpenGLVertexArray>(drawMode);
        default:
            CW_ENGINE_ASSERT(false, "Renderer API not supporter");
            return nullptr;
        }

        return nullptr;
    }
} // namespace Crowny