#include "cwpch.h"

#include "Crowny/RenderAPI/CommandBuffer.h"
#include "Crowny/Renderer/Renderer.h"

#include "Platform/Vulkan/VulkanCommandBuffer.h"
#include "Platform/Vulkan/VulkanRenderAPI.h"

namespace Crowny
{

    void CommandSyncMask::AddDependency(const Ref<CommandBuffer>& buffer)
    {
        if (buffer == nullptr)
            return;
        m_Mask |= GetGlobalQueueMask(buffer->GetType(), buffer->GetQueueIdx());
    }

    uint32_t CommandSyncMask::GetGlobalQueueMask(GpuQueueType type, uint32_t queueIdx)
    {
        uint32_t bitShift = 0;
        switch (type)
        {
        case GpuQueueType::GRAPHICS_QUEUE:
            break;
        case GpuQueueType::COMPUTE_QUEUE:
            bitShift = 8;
            break;
        case GpuQueueType::UPLOAD_QUEUE:
            bitShift = 16;
            break;
        default:
            break;
        }
        return (1 << queueIdx) << bitShift;
    }

    uint32_t CommandSyncMask::GetGlobalQueueIdx(GpuQueueType type, uint32_t queueIdx)
    {
        switch (type)
        {
        case GpuQueueType::COMPUTE_QUEUE:
            return 8 + queueIdx;
        case GpuQueueType::UPLOAD_QUEUE:
            return 16 + queueIdx;
        default:
            return queueIdx;
        }
    }

    uint32_t CommandSyncMask::GetQueueIdxAndType(uint32_t globalQueueIdx, GpuQueueType& type)
    {
        if (globalQueueIdx >= 16)
        {
            type = GpuQueueType::UPLOAD_QUEUE;
            return globalQueueIdx - 16;
        }

        if (globalQueueIdx >= 8)
        {
            type = GpuQueueType::COMPUTE_QUEUE;
            return globalQueueIdx - 8;
        }

        type = GpuQueueType::GRAPHICS_QUEUE;
        return globalQueueIdx;
    }

    CommandBuffer::CommandBuffer(GpuQueueType queueType, uint32_t queueIdx, bool secondary)
      : m_Type(queueType), m_QueueIdx(queueIdx), m_IsSecondary(secondary)
    {
    }

    Ref<CommandBuffer> CommandBuffer::Create(GpuQueueType type, uint32_t queueIdx, bool secondary)
    {
        switch (Renderer::GetAPI())
        {
        // case RenderAPI::API::OpenGL: return CreateRef<OpenGLFramebuffer>(props);
        case RenderAPI::API::Vulkan:
            return CreateRef<VulkanCommandBuffer>(*gVulkanRenderAPI().GetPresentDevice().get(), type, queueIdx,
                                                  secondary);
        default:
            CW_ENGINE_ASSERT(false, "Renderer API not supporter");
            return nullptr;
        }

        return nullptr;
    }

} // namespace Crowny