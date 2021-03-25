#pragma once

#include "Crowny/Renderer/GraphicsPipeline.h"

namespace Crowny
{
    
    class OpenGLGraphicsPipeline : public GraphicsPipeline
    {
    public:
        OpenGLGraphicsPipeline(const PipelineStateDesc& desc);
        ~OpenGLGraphicsPipeline();
    };

    class OpenGLComputePipeline : public ComputePipeline
    {
        OpenGLComputePipeline(const Ref<Shader>& shader);
        ~OpenGLComputePipeline();
    };
    
}