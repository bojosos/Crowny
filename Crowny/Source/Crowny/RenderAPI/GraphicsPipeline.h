#pragma once

#include "Crowny/RenderAPI/Buffer.h"
#include "Crowny/RenderAPI/Shader.h"
#include "Crowny/RenderAPI/UniformParamInfo.h"

namespace Crowny
{

    class VulkanRenderPass;

    struct DepthStencilStateDesc
    {
        bool EnableDepthRead = true;
        bool EnableDepthWrite = true;
        CompareFunction DepthCompareFunction = CompareFunction::LESS;
        bool EnableStencil = false;
    };

    struct BlendStateDesc
    {
        bool EnableBlending = false;

        BlendFactor SrcBlend = BlendFactor::One;
        BlendFactor DstBlend = BlendFactor::Zero;
        BlendFunction BlendOp = BlendFunction::ADD;

        BlendFactor SrcBlendAlpha = BlendFactor::One;
        BlendFactor DstBlendAlpha = BlendFactor::Zero;
        BlendFunction BlendOpAlpha = BlendFunction::ADD;
    };

    struct RasterizerStateDesc
    {
    };

    struct PipelineStateDesc
    {
        Ref<Shader> VertexShader;
        Ref<Shader> FragmentShader;
        Ref<Shader> GeometryShader;
        Ref<Shader> HullShader;
        Ref<Shader> DomainShader;

        RasterizerStateDesc RasterizerState;
        DepthStencilStateDesc DepthStencilState;
        BlendStateDesc BlendState;
    };

    // TODO: Change the name of the file
    class GraphicsPipeline
    {
    public:
        GraphicsPipeline(const PipelineStateDesc& desc);
        virtual ~GraphicsPipeline() = default;

        const Ref<UniformParamInfo>& GetParamInfo() const { return m_ParamInfo; }

    public:
        static Ref<GraphicsPipeline> Create(const PipelineStateDesc& props, const BufferLayout& layout);

    protected:
        PipelineStateDesc m_Data;
        Ref<UniformParamInfo> m_ParamInfo;
    };

    class ComputePipeline
    {
    public:
        ComputePipeline(const Ref<Shader>& computeShader);
        virtual ~ComputePipeline() = default;

        const Ref<UniformParamInfo>& GetParamInfo() const { return m_ParamInfo; }

    public:
        static Ref<ComputePipeline> Create(const Ref<Shader>& computeShader);

    protected:
        Ref<Shader> m_Shader;
        Ref<UniformParamInfo> m_ParamInfo;
    };

} // namespace Crowny
