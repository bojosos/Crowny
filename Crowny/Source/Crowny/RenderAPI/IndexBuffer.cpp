#include "cwpch.h"

#include "Crowny/RenderAPI/IndexBuffer.h"
#include "Crowny/Renderer/Renderer.h"

#include "Platform/OpenGL/OpenGLIndexBuffer.h"
#include "Platform/Vulkan/VulkanIndexBuffer.h"

namespace Crowny
{

    Ref<IndexBuffer> IndexBuffer::Create(const IndexBufferDesc& desc)
    {
        switch (gRenderAPI->GetAPI())
        {
        case RenderAPI::API::OpenGL:
            if (desc.Data)
            {
                if (desc.Type == IndexType::Index_16)
                    return Ref<IndexBuffer>(new OpenGLIndexBuffer(static_cast<uint16_t*>(const_cast<void*>(desc.Data)), desc.Count, desc.Usage));
                else
                    return Ref<IndexBuffer>(new OpenGLIndexBuffer(static_cast<uint32_t*>(const_cast<void*>(desc.Data)), desc.Count, desc.Usage));
            }
            return Ref<IndexBuffer>(new OpenGLIndexBuffer(desc.Count, desc.Type, desc.Usage));
        case RenderAPI::API::Vulkan:
            if (desc.Data)
            {
                if (desc.Type == IndexType::Index_16)
                    return Ref<IndexBuffer>(new VulkanIndexBuffer(static_cast<uint16_t*>(const_cast<void*>(desc.Data)), desc.Count, desc.Usage));
                else
                    return Ref<IndexBuffer>(new VulkanIndexBuffer(static_cast<uint32_t*>(const_cast<void*>(desc.Data)), desc.Count, desc.Usage));
            }
            return Ref<IndexBuffer>(new VulkanIndexBuffer(desc.Count, desc.Type, desc.Usage));
        default:
            CW_ENGINE_ASSERT(false, "Renderer API not supported");
            return nullptr;
        }
    }

} // namespace Crowny
