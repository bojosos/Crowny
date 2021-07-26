#pragma once

#include "Crowny/Renderer/Shader.h"
#include "Crowny/Renderer/Buffer.h"
#include "Crowny/Renderer/UniformParamInfo.h"

namespace Crowny
{

    class VulkanRenderPass;
    
    struct PipelineStateDesc
    {
        //TODO: blend, resterizer, depth stencil
        Ref<Shader> VertexShader;
        Ref<Shader> FragmentShader;
        Ref<Shader> GeometryShader;
        Ref<Shader> HullShader;
        Ref<Shader> DomainShader;
    };
    
    //TODO: Change the name of the file
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
        
        const Ref<UniformParamInfo>& GetParamInfo() const  { return m_ParamInfo; }
    
    public:
        static Ref<ComputePipeline> Create(const Ref<Shader>& computeShader);
    protected:
        Ref<Shader> m_Shader;
        Ref<UniformParamInfo> m_ParamInfo;
    };
    
}
