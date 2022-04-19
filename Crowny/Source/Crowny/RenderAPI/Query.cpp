#include "cwpch.h"

#include "Crowny/RenderAPI/Query.h"

#include "Crowny/Renderer/Renderer.h"

#include "Platform/Vulkan/VulkanQuery.h"
#include "Platform/OpenGL/OpenGLQuery.h"

namespace Crowny
{

	Ref<TimerQuery> TimerQuery::Create()
	{
		switch (Renderer::GetAPI())
		{
		case RenderAPI::API::OpenGL:
			return CreateRef<OpenGLTimerQuery>();
		case RenderAPI::API::Vulkan:
			return CreateRef<VulkanTimerQuery>();
		default:
			CW_ENGINE_ASSERT(false, "Renderer API not supporter");
			return nullptr;
		}

		return nullptr;
	}

	Ref<PipelineQuery> PipelineQuery::Create()
	{
		switch (Renderer::GetAPI())
		{
		case RenderAPI::API::OpenGL:
			return CreateRef<OpenGLPipelineQuery>();
		case RenderAPI::API::Vulkan:
			return CreateRef<VulkanPipelineQuery>();
		default:
			CW_ENGINE_ASSERT(false, "Renderer API not supporter");
			return nullptr;
		}

		return nullptr;
	}
	
	Ref<OcclusionQuery> OcclusionQuery::Create(bool binary)
	{
		switch (Renderer::GetAPI())
		{
		case RenderAPI::API::OpenGL:
			return CreateRef<OpenGLOcclusionQuery>(binary);
		case RenderAPI::API::Vulkan:
			return CreateRef<VulkanOcclusionQuery>(binary);
		default:
			CW_ENGINE_ASSERT(false, "Renderer API not supporter");
			return nullptr;
		}

		return nullptr;
	}
}