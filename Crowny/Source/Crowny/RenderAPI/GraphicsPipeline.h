#pragma once

#include "Crowny/RenderAPI/Buffer.h"
#include "Crowny/RenderAPI/Shader.h"
#include "Crowny/RenderAPI/UniformParamInfo.h"

namespace Crowny
{
    class Shader;
    class ShaderStage;
    class VulkanRenderPass;

    struct DepthStencilStateDesc
    {
        bool EnableDepthRead = true;
        bool EnableDepthWrite = true;
        CompareFunction DepthCompareFunction = CompareFunction::LESS;

        bool EnableStencil = false;
        uint8_t StencilReadMask = 0xff;
        uint8_t StencilWriteMask = 0xff;

        CompareFunction StencilFrontCompare = CompareFunction::ALWAYS_PASS;
        StencilOperation StencilFrontFailOp = StencilOperation::Keep;
        StencilOperation StencilFrontDepthFailOp = StencilOperation::Keep;
        StencilOperation StencilFrontPassOp = StencilOperation::Keep;

        CompareFunction StencilBackCompare = CompareFunction::ALWAYS_PASS;
        StencilOperation StencilBackFailOp = StencilOperation::Keep;
        StencilOperation StencilBackDepthFailOp = StencilOperation::Keep;
        StencilOperation StencilBackPassOp = StencilOperation::Keep;

        static Ref<DepthStencilStateDesc> GetDefault();
    };

    // TODO: This should not be reused in the PipelineStateDesc. There I should be using completely different structures
    // that cannot be changed.
    struct BlendStateDesc
    {
        bool EnableBlending = false;
        bool AlphaToCoverage = false;

        BlendFactor SrcBlend = BlendFactor::One;
        BlendFactor DstBlend = BlendFactor::Zero;
        BlendFunction BlendOp = BlendFunction::ADD;

        BlendFactor SrcBlendAlpha = BlendFactor::One;
        BlendFactor DstBlendAlpha = BlendFactor::Zero;
        BlendFunction BlendOpAlpha = BlendFunction::ADD;

        static Ref<BlendStateDesc> GetDefault();
    };

    struct RasterizerStateDesc
    {
        CullingMode CullMode = CullingMode::CULL_COUNTERCLOCKWISE;
        float DepthBias = 0.0f;
        float DepthBiasSlope = 0.0f;
        float DepthBiasClamp = 0.0f;
        PolygonMode PolygonDrawMode = PolygonMode::Solid;
        bool DepthClipEnable = false;
        bool ScissorsEnabled = false;

        static Ref<RasterizerStateDesc> GetDefault();
    };

    struct PipelineStateDesc
    {
        Ref<ShaderStage> VertexShader;
        Ref<ShaderStage> FragmentShader;
        Ref<ShaderStage> GeometryShader;
        Ref<ShaderStage> HullShader;
        Ref<ShaderStage> DomainShader;

        Ref<RasterizerStateDesc> RasterizerState;
        Ref<DepthStencilStateDesc> DepthStencilState;
        Ref<BlendStateDesc> BlendState;
    };

    // TODO: Change the name of the file
    class GraphicsPipeline
    {
    public:
        GraphicsPipeline(const PipelineStateDesc& desc);
        virtual ~GraphicsPipeline() = default;

        const Ref<UniformParamInfo>& GetParamInfo() const { return m_ParamInfo; }

    public:
        static Ref<GraphicsPipeline> Create(const PipelineStateDesc& props);

    protected:
        PipelineStateDesc m_Data;
        Ref<UniformParamInfo> m_ParamInfo;
    };

    class ComputePipeline
    {
    public:
        ComputePipeline(const Ref<ShaderStage>& computeShader);
        virtual ~ComputePipeline() = default;

        const Ref<UniformParamInfo>& GetParamInfo() const { return m_ParamInfo; }

    public:
        static Ref<ComputePipeline> Create(const Ref<ShaderStage>& computeShader);

    protected:
        Ref<ShaderStage> m_Shader;
        Ref<UniformParamInfo> m_ParamInfo;
    };

} // namespace Crowny
