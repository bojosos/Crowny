#pragma once

#include "Crowny/Renderer/Shader.h"

namespace Crowny
{
    
    struct PipelineStateDesc
    {
        //TODO: blend, resterizer, depth stencil
        Ref<Shader> VertexShader;
        Ref<Shader> FragmentShader;
        Ref<Shader> GeometryShader;
        Ref<Shader> HullShader;
        Ref<Shader> DomainShader;
    };
    
    //TODO: Rename the name of the file
    class GraphicsPipeline
    {
    public:
        virtual ~GraphicsPipeline() = default;

        static Ref<GraphicsPipeline> Create(const PipelineStateDesc& desc);
        
    protected:
        PipelineStateDesc m_Data;
    };

    class ComputePipeline
    {
    public:
        virtual ~ComputePipeline() = default;
        
        static Ref<ComputePipeline> Create(const Ref<Shader>& computeShader);
    protected:
        Ref<Shader> m_Shader;
    };
    
}