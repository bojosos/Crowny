#include "cwpch.h"

#include "Crowny/Renderer/SamplerState.h"
#include "Crowny/Renderer/Renderer.h"

#include "Platform/Vulkan/VulkanSamplerState.h"

namespace Crowny
{

    Ref<SamplerState> SamplerState::s_DefaultSamplerState = nullptr;

    SamplerState::SamplerState(const SamplerStateDesc& desc) : m_Properties(desc)
    { }
    
    const Ref<SamplerState>& SamplerState::GetDefault()
    {
        if (s_DefaultSamplerState == nullptr)
            s_DefaultSamplerState = SamplerState::Create({});
        return s_DefaultSamplerState;
    }

    Ref<SamplerState> SamplerState::Create(const SamplerStateDesc& desc)
    {
        switch (Renderer::GetAPI())
		{
			//TODO: Add support for binary OpenGL shaders
			//case RendererAPI::API::OpenGL: return CreateRef<OpenGLShader>(m_Filepath);
			case RendererAPI::API::Vulkan: return CreateRef<VulkanSamplerState>(desc);
			default: 					   CW_ENGINE_ASSERT(false, "Renderer API not supporter"); return nullptr;
		}

		return nullptr;
    }

}