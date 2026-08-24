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

    Ref<GraphicsPipeline> GraphicsPipeline::Create(const PipelineStateDesc& desc)
    {
        switch (RenderAPI::TryGet()->GetAPI())
        {
        case RenderAPI::API::OpenGL:
            return Ref<GraphicsPipeline>(new OpenGLGraphicsPipeline(desc));
        case RenderAPI::API::Vulkan:
            return Ref<GraphicsPipeline>(new VulkanGraphicsPipeline(desc));
        default:
            CW_ENGINE_ASSERT(false, "Renderer API not supported");
            return nullptr;
        }

        return nullptr;
    }

    RayTracingPipeline::RayTracingPipeline(const RayTracingPipelineDesc& desc) : m_Data(desc)
    {
        UniformParamDesc uniformDesc;
        if (desc.RaygenShader != nullptr)
            uniformDesc.RaygenParams = desc.RaygenShader->GetUniformDesc();
        if (desc.HitShader != nullptr)
            uniformDesc.HitParams = desc.HitShader->GetUniformDesc();
        if (desc.MissShader != nullptr)
            uniformDesc.MissParams = desc.MissShader->GetUniformDesc();

        m_ParamInfo = UniformParamInfo::Create(uniformDesc);
    }

    Ref<RayTracingPipeline> RayTracingPipeline::Create(const RayTracingPipelineDesc& desc)
    {
        switch (RenderAPI::TryGet()->GetAPI())
        {
        case RenderAPI::API::OpenGL:
            CW_ENGINE_ERROR("OpenGL backend does not support ray tracing");
            return nullptr;
        case RenderAPI::API::Vulkan:
            return Ref<RayTracingPipeline>(new VulkanRayTracingPipeline(desc));
        default:
            CW_ENGINE_ASSERT(false, "Renderer API not supported");
            return nullptr;
        }
    }

    ComputePipeline::ComputePipeline(const Ref<ShaderStage>& shader) : m_Shader(shader)
    {
        UniformParamDesc paramDesc;
        paramDesc.ComputeParams = m_Shader->GetUniformDesc();

        m_ParamInfo = UniformParamInfo::Create(paramDesc);
    }

    Ref<ComputePipeline> ComputePipeline::Create(const Ref<ShaderStage>& shader)
    {
        switch (RenderAPI::TryGet()->GetAPI())
        {
        case RenderAPI::API::OpenGL:
            return Ref<ComputePipeline>(new OpenGLComputePipeline(shader));
        case RenderAPI::API::Vulkan:
            return Ref<ComputePipeline>(new VulkanComputePipeline(shader));
        default:
            CW_ENGINE_ASSERT(false, "Renderer API not supported");
            return nullptr;
        }

        return nullptr;
    }

} // namespace Crowny
