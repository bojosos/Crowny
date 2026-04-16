#include "cwpch.h"

#include "Crowny/RenderAPI/SamplerState.h"
#include "Crowny/Renderer/Renderer.h"

#include "Platform/Vulkan/VulkanSamplerState.h"

namespace Crowny
{

    Ref<SamplerState> SamplerState::s_DefaultSamplerState = nullptr;

    SamplerState::SamplerState(const SamplerStateDesc& desc) : m_Properties(desc) {}

    const Ref<SamplerState>& SamplerState::GetDefault()
    {
        if (s_DefaultSamplerState == nullptr)
            s_DefaultSamplerState = SamplerState::Create({});
        return s_DefaultSamplerState;
    }

    Ref<SamplerState> SamplerState::Create(const SamplerStateDesc& desc)
    {
        switch (gRenderAPI->GetAPI())
        {
        // TODO: Add support for binary OpenGL shaders
        // case RenderAPI::API::OpenGL: return CreateRef<OpenGLShader>(m_Filepath);
        case RenderAPI::API::Vulkan:
            return Ref<SamplerState>(new VulkanSamplerState(desc));
        default:
            CW_ENGINE_ASSERT(false, "Renderer API not supported");
            return nullptr;
        }

        return nullptr;
    }

} // namespace Crowny