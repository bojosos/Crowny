#pragma once

#include "Crowny/Renderer/UniformBufferBlock.h"
#include "Crowny/Renderer/Shader.h"

namespace Crowny
{
    
    struct PipelineUniformDesc
    {
        Ref<UniformDesc> VertexUniforms;
        Ref<UniformDesc> FragmentUniforms;
        Ref<UniformDesc> GeometryUniforms;
        Ref<UniformDesc> HullUniforms;
        Ref<UniformDesc> DomainUniforms;
        Ref<UniformDesc> ComputeUniforms;
    };

    class UniformParams
    {
    public:
        UniformParams(const PipelineUniformDesc& desc);
        UniformParams();

        void SetUniformBlockBuffer(uint32_t set, uint32_t slot, const Ref<UniformBufferBlock>* uniformBlock);
        // void SetTexture(uint32_t set, uint32_t slot, const Ref<Texture>& texture);
        // void SetLoadStoreTexture(uint32_t set, uint32_t slot, const Ref<Texture>& texture);
        // void SetBuffer(uint32_t set, uint32_t slot, const Ref<GpuBuffer>& buffer);
        // void SetSampler(uint32_t set, uint32_t slot, const Ref<Sampler>& sampler);

    private:
        UniformBufferBlock* m_BufferBlocks = nullptr;
        PipelineUniformDesc* m_UniformDesc;
    };
    
}