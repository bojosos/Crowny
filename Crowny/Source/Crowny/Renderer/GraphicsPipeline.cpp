#include "cwpch.h"

#include "Crowny/Renderer/GraphicsPipeline.h"
#include "Crowny/Renderer/Renderer.h"

#include "Platform/OpenGL/OpenGLPipeline.h"
#include "Platform/Vulkan/VulkanPipeline.h"

namespace Crowny
{

	Ref<GraphicsPipeline> GraphicsPipeline::Create(const PipelineStateDesc& props)
	{
		switch (Renderer::GetAPI())
		{
			case RendererAPI::API::OpenGL: return CreateRef<OpenGLGraphicsPipeline>(props);
			case RendererAPI::API::Vulkan: return CreateRef<VulkanGraphicsPipeline>(props);
			default: 					   CW_ENGINE_ASSERT(false, "Renderer API not supporter"); return nullptr;
		}

		return nullptr;
	}

	Ref<GraphicsPipeline> ComputePipeline::Create(const Ref<Shader>& props)
	{
		switch (Renderer::GetAPI())
		{
			case RendererAPI::API::OpenGL: return CreateRef<OpenGLComputePipeline>(shader);
			case RendererAPI::API::Vulkan: return CreateRef<VulkanComputePipeline>(shader);
			default: 					   CW_ENGINE_ASSERT(false, "Renderer API not supporter"); return nullptr;
		}

		return nullptr;
	}

}