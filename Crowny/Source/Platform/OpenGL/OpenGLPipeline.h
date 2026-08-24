#pragma once

#include "Crowny/RenderAPI/GraphicsPipeline.h"

namespace Crowny
{
    class OpenGLGraphicsPipeline : public GraphicsPipeline
    {
    public:
        friend class GraphicsPipeline;
        ~OpenGLGraphicsPipeline() override;

        uint32_t GetProgram() const { return m_Program; }
        const PipelineStateDesc& GetDesc() const { return m_Data; }

    protected:
        explicit OpenGLGraphicsPipeline(const PipelineStateDesc& desc);

    private:
        uint32_t m_Program = 0;
    };

    class OpenGLComputePipeline : public ComputePipeline
    {
    public:
        friend class ComputePipeline;
        ~OpenGLComputePipeline() override;

        uint32_t GetProgram() const { return m_Program; }

    protected:
        explicit OpenGLComputePipeline(const Ref<ShaderStage>& shader);

    private:
        uint32_t m_Program = 0;
    };
} // namespace Crowny
