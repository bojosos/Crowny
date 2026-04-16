#pragma once

#include "Crowny/RenderAPI/GraphicsPipeline.h"
#include "Crowny/RenderAPI/Shader.h"

namespace Crowny
{

    class OpenGLGraphicsPipeline : public GraphicsPipeline
    {
    public:
        friend class GraphicsPipeline;
        ~OpenGLGraphicsPipeline();
    protected:
        OpenGLGraphicsPipeline(const PipelineStateDesc& desc);
    };

    class OpenGLComputePipeline : public ComputePipeline
    {
    public:
        friend class ComputePipeline;
        ~OpenGLComputePipeline();
    protected:
        OpenGLComputePipeline(const Ref<ShaderStage>& shader);
    };

} // namespace Crowny