#include "cwpch.h"

#include "Crowny/RenderAPI/GraphicsPipeline.h"
#include "Crowny/Renderer/Renderer.h"

#include "Platform/OpenGL/OpenGLPipeline.h"
#include "Platform/Vulkan/VulkanPipeline.h"

namespace Crowny
{

    Ref<DepthStencilStateDesc> DepthStencilStateDesc::GetDefault()
    {
        static Ref<DepthStencilStateDesc> defaultState = CreateRef<DepthStencilStateDesc>();
        return defaultState;
    }

    Ref<BlendStateDesc> BlendStateDesc::GetDefault()
    {
        static Ref<BlendStateDesc> defaultState = CreateRef<BlendStateDesc>();
        return defaultState;
    }

    Ref<RasterizerStateDesc> RasterizerStateDesc::GetDefault()
    {
        static Ref<RasterizerStateDesc> defaultState = CreateRef<RasterizerStateDesc>();
        return defaultState;
    }

    GraphicsPipeline::GraphicsPipeline(const PipelineStateDesc& desc) : m_Data(desc)
    {
        UniformParamDesc uniformDesc;
        if (desc.VertexShader != nullptr)
            uniformDesc.VertexParams = desc.VertexShader->GetUniformDesc();
        if (desc.FragmentShader != nullptr)
            uniformDesc.FragmentParams = desc.FragmentShader->GetUniformDesc();
        if (desc.GeometryShader != nullptr)
            uniformDesc.GeometryParams = desc.GeometryShader->GetUniformDesc();
        if (desc.HullShader != nullptr)
            uniformDesc.HullParams = desc.HullShader->GetUniformDesc();
        if (desc.DomainShader != nullptr)
            uniformDesc.DomainParams = desc.DomainShader->GetUniformDesc();

        m_ParamInfo = UniformParamInfo::Create(uniformDesc);
    }

    Ref<GraphicsPipeline> GraphicsPipeline::Create(const PipelineStateDesc& props)
    {
        switch (Renderer::GetAPI())
        {
        case RenderAPI::API::OpenGL:
            return CreateRef<OpenGLGraphicsPipeline>(props);
        case RenderAPI::API::Vulkan:
            return CreateRef<VulkanGraphicsPipeline>(props);
        default:
            CW_ENGINE_ASSERT(false, "Renderer API not supporter");
            return nullptr;
        }

        return nullptr;
    }

    ComputePipeline::ComputePipeline(const Ref<ShaderStage>& shader) : m_Shader(shader)
    {
        UniformParamDesc paramDesc;
        paramDesc.ComputeParams = m_Shader->GetUniformDesc();

        m_ParamInfo = UniformParamInfo::Create(paramDesc);
    }

    Ref<ComputePipeline> ComputePipeline::Create(const Ref<ShaderStage>& shader)
    {
        switch (Renderer::GetAPI())
        {
        case RenderAPI::API::OpenGL:
            return CreateRef<OpenGLComputePipeline>(shader);
        case RenderAPI::API::Vulkan:
            return CreateRef<VulkanComputePipeline>(shader);
        default:
            CW_ENGINE_ASSERT(false, "Renderer API not supporter");
            return nullptr;
        }

        return nullptr;
    }

} // namespace Crowny
