#include "cwpch.h"

#include "Crowny/Renderer/GraphicsPipeline.h"
#include "Crowny/Renderer/Renderer.h"

#include "Platform/OpenGL/OpenGLPipeline.h"
#include "Platform/Vulkan/VulkanPipeline.h"

namespace Crowny
{

	Ref<GraphicsPipeline> GraphicsPipeline::Create(const PipelineStateDesc& props, const BufferLayout& layout)
	{
		switch (Renderer::GetAPI())
		{
			case RendererAPI::API::OpenGL: return CreateRef<OpenGLGraphicsPipeline>(props);
			case RendererAPI::API::Vulkan: return CreateRef<VulkanGraphicsPipeline>(props, layout);
			default: 					   CW_ENGINE_ASSERT(false, "Renderer API not supporter"); return nullptr;
		}

		return nullptr;
	}

	Ref<ComputePipeline> ComputePipeline::Create(const Ref<Shader>& shader)
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
