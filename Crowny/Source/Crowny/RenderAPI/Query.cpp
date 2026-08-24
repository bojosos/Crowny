#include "cwpch.h"

#include "Crowny/RenderAPI/Query.h"

#include "Crowny/Renderer/Renderer.h"

#include "Platform/OpenGL/OpenGLQuery.h"
#include "Platform/Vulkan/VulkanQuery.h"

namespace Crowny
{

    Ref<TimerQuery> TimerQuery::Create()
    {
        switch (RenderAPI::TryGet()->GetAPI())
        {
        case RenderAPI::API::OpenGL:
            return Ref<TimerQuery>(new OpenGLTimerQuery());
        case RenderAPI::API::Vulkan:
            return Ref<TimerQuery>(new VulkanTimerQuery());
        default:
            CW_ENGINE_ASSERT(false, "Renderer API not supported");
            return nullptr;
        }

        return nullptr;
    }

    Ref<PipelineQuery> PipelineQuery::Create()
    {
        switch (RenderAPI::TryGet()->GetAPI())
        {
        case RenderAPI::API::OpenGL:
            return Ref<PipelineQuery>(new OpenGLPipelineQuery());
        case RenderAPI::API::Vulkan:
            return Ref<PipelineQuery>(new VulkanPipelineQuery());
        default:
            CW_ENGINE_ASSERT(false, "Renderer API not supported");
            return nullptr;
        }

        return nullptr;
    }

    Ref<OcclusionQuery> OcclusionQuery::Create(bool binary)
    {
        switch (RenderAPI::TryGet()->GetAPI())
        {
        case RenderAPI::API::OpenGL:
            return Ref<OcclusionQuery>(new OpenGLOcclusionQuery(binary));
        case RenderAPI::API::Vulkan:
            return Ref<OcclusionQuery>(new VulkanOcclusionQuery(binary));
        default:
            CW_ENGINE_ASSERT(false, "Renderer API not supported");
            return nullptr;
        }

        return nullptr;
    }
} // namespace Crowny