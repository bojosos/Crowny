#include "cwpch.h"

#include "Crowny/RenderAPI/VertexArray.h"

#include "Crowny/Renderer/Renderer.h"
#include "Platform/OpenGL/OpenGLVertexArray.h"

namespace Crowny
{
    Ref<VertexArray> VertexArray::Create(DrawMode drawMode)
    {
        switch (gRenderAPI->GetAPI())
        {
        case RenderAPI::API::OpenGL:
            return Ref<VertexArray>(new OpenGLVertexArray(drawMode));
        default:
            CW_ENGINE_ASSERT(false, "Renderer API not supported");
            return nullptr;
        }

        return nullptr;
    }
} // namespace Crowny