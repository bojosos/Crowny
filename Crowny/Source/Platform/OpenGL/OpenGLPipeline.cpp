#include "cwpch.h"

#include "Platform/OpenGL/OpenGLPipeline.h"

namespace Crowny
{

    OpenGLGraphicsPipeline::OpenGLGraphicsPipeline(const PipelineStateDesc& shader) : GraphicsPipeline(shader) {}

    OpenGLGraphicsPipeline::~OpenGLGraphicsPipeline() {}

    OpenGLComputePipeline::OpenGLComputePipeline(const Ref<Shader>& shader) : ComputePipeline(shader) {}

    OpenGLComputePipeline::~OpenGLComputePipeline() {}

} // namespace Crowny