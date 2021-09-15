#include "cwpch.h"

#include "Crowny/RenderAPI/IndexBuffer.h"
#include "Crowny/Renderer/Renderer.h"

#include "Platform/OpenGL/OpenGLIndexBuffer.h"
#include "Platform/Vulkan/VulkanIndexBuffer.h"

namespace Crowny
{

    Ref<IndexBuffer> IndexBuffer::Create(uint32_t count, IndexType indexType, BufferUsage usage)
    {
        switch (Renderer::GetAPI())
        {
        case RenderAPI::API::OpenGL:
            return CreateRef<OpenGLIndexBuffer>(count, indexType, usage);
        case RenderAPI::API::Vulkan:
            return CreateRef<VulkanIndexBuffer>(count, indexType, usage);
        default:
            CW_ENGINE_ASSERT(false, "Renderer API not supporter");
            return nullptr;
        }

        return nullptr;
    }

    Ref<IndexBuffer> IndexBuffer::Create(uint16_t* indices, uint32_t count, BufferUsage usage)
    {
        switch (Renderer::GetAPI())
        {
        case RenderAPI::API::OpenGL:
            return CreateRef<OpenGLIndexBuffer>(indices, count, usage);
        case RenderAPI::API::Vulkan:
            return CreateRef<VulkanIndexBuffer>(indices, count, usage);
        default:
            CW_ENGINE_ASSERT(false, "Renderer API not supporter");
            return nullptr;
        }

        return nullptr;
    }

    Ref<IndexBuffer> IndexBuffer::Create(uint32_t* indices, uint32_t count, BufferUsage usage)
    {
        switch (Renderer::GetAPI())
        {
        case RenderAPI::API::OpenGL:
            return CreateRef<OpenGLIndexBuffer>(indices, count, usage);
        case RenderAPI::API::Vulkan:
            return CreateRef<VulkanIndexBuffer>(indices, count, usage);
        default:
            CW_ENGINE_ASSERT(false, "Renderer API not supporter");
            return nullptr;
        }

        return nullptr;
    }

} // namespace Crowny