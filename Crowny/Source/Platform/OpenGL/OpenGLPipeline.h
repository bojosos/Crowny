#pragma once

#include "Crowny/RenderAPI/GraphicsPipeline.h"
#include "Crowny/RenderAPI/Shader.h"

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
    public:
        OpenGLComputePipeline(const Ref<ShaderStage>& shader);
        ~OpenGLComputePipeline();
    };

} // namespace Crowny