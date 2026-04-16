#include "cwpch.h"

#include "Crowny/RenderAPI/VertexBuffer.h"
#include "Crowny/Renderer/Renderer.h"
#include "Platform/OpenGL/OpenGLVertexBuffer.h"
#include "Platform/Vulkan/VulkanVertexBuffer.h"

namespace Crowny
{

    Ref<VertexBuffer> VertexBuffer::Create(const VertexBufferDesc& desc)
    {
        switch (gRenderAPI->GetAPI())
        {
        case RenderAPI::API::OpenGL:
            if (desc.Data)
                return Ref<VertexBuffer>(new OpenGLVertexBuffer(const_cast<void*>(desc.Data), desc.Size, desc.Usage));
            else
                return Ref<VertexBuffer>(new OpenGLVertexBuffer(desc.Size, desc.Usage));
        case RenderAPI::API::Vulkan:
            if (desc.Data)
                return Ref<VertexBuffer>(new VulkanVertexBuffer(const_cast<void*>(desc.Data), desc.Size, desc.Usage));
            else
                return Ref<VertexBuffer>(new VulkanVertexBuffer(desc.Size, desc.Usage));
        default:
            CW_ENGINE_ASSERT(false, "Renderer API not supported");
            return nullptr;
        }
    }

} // namespace Crowny
