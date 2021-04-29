#include "cwpch.h"

#include "Platform/OpenGL/OpenGLPipeline.h"

namespace Crowny
{

    OpenGLGraphicsPipeline::OpenGLGraphicsPipeline(const PipelineStateDesc& desc)
    {
        
    }

    OpenGLGraphicsPipeline::~OpenGLGraphicsPipeline()
    {

    }

    OpenGLComputePipeline::OpenGLComputePipeline(const Ref<Shader>& shader) : m_Shader(shader)
    {

    }

    OpenGLComputePipeline::~OpenGLComputePipeline()
    {

    }

}