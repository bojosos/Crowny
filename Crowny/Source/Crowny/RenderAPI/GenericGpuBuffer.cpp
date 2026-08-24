#include "cwpch.h"

#include "Crowny/RenderAPI/GenericGpuBuffer.h"
#include "Crowny/RenderAPI/RenderAPI.h"

#include "Platform/OpenGL/OpenGLGpuBuffer.h"
#include "Platform/Vulkan/VulkanGenericGpuBuffer.h"

namespace Crowny
{
    Ref<GenericGpuBuffer> GenericGpuBuffer::Create(const GenericGpuBufferDesc& desc)
    {
        switch (RenderAPI::TryGet()->GetAPI())
        {
        case RenderAPI::API::OpenGL:
            return Ref<GenericGpuBuffer>(new OpenGLGenericGpuBuffer(desc.ElementCount, desc.ElementSize, desc.Type, desc.Format, desc.Usage));
        case RenderAPI::API::Vulkan:
            return Ref<GenericGpuBuffer>(new VulkanGenericGpuBuffer(desc.ElementCount, desc.ElementSize, desc.Type, desc.Format, desc.Usage));
        default:
            CW_ENGINE_ASSERT(false, "Renderer API not supported");
            return nullptr;
        }
    }
} // namespace Crowny
