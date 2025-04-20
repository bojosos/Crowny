#include "cwpch.h"

#include "Crowny/RenderAPI/GenericGpuBuffer.h"
#include "Crowny/RenderAPI/RenderAPI.h"

#include "Platform/Vulkan/VulkanGenericGpuBuffer.h"

namespace Crowny
{
    Ref<GenericGpuBuffer> GenericGpuBuffer::Create(uint32_t elementCount, uint32_t elementSize, GpuBufferType type, GpuBufferFormat format,
                                                   BufferUsage usage)
    {
        switch (RenderAPI::Get().GetAPI())
        {
        case RenderAPI::API::OpenGL:
            return nullptr;
        case RenderAPI::API::Vulkan:
            return CreateRef<VulkanGenericGpuBuffer>(elementCount, elementSize, type, format, usage);
        default:
            CW_ENGINE_ASSERT(false, "Renderer API not supported");
            return nullptr;
        }
    }
} // namespace Crowny