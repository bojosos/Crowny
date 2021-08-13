#pragma once

#include "Crowny/RenderAPI/GraphicsPipeline.h"
#include "Crowny/RenderAPI/SamplerState.h"
#include "Crowny/RenderAPI/Shader.h"
#include "Crowny/RenderAPI/Texture.h"
#include "Crowny/RenderAPI/UniformBufferBlock.h"
#include "Crowny/RenderAPI/UniformParamInfo.h"

namespace Crowny
{

    class UniformParams
    {
    public:
        enum class ParamType
        {
            ParamBlock,
            Texture,
            LoadStoreTexture,
            Buffer,
            SamplerState,
            Count
        };

        UniformParams(const Ref<UniformParamInfo>& desc);
        virtual ~UniformParams();

        void SetUniformBlockBuffer(ShaderType type, const std::string& name,
                                   const Ref<UniformBufferBlock>& uniformBuffer);
        void SetUniformBlockBuffer(const std::string& name, const Ref<UniformBufferBlock>& uniformBuffer);
        virtual void SetUniformBlockBuffer(uint32_t set, uint32_t slot, const Ref<UniformBufferBlock>& uniformBuffer);

        virtual void SetTexture(uint32_t set, uint32_t slot, const Ref<Texture>& texture,
                                const TextureSurface& surface = TextureSurface::COMPLETE);
        void SetTexture(ShaderType type, const std::string& name, const Ref<Texture>& texture,
                        const TextureSurface& surface = TextureSurface::COMPLETE);

        // virtual void SetLoadStoreTexture(uint32_t set, uint32_t slot, const Ref<Texture>& texture);
        // virtual void SetBuffer(uint32_t set, uint32_t slot, const Ref<GpuBuffer>& buffer);
        virtual void SetSamplerState(uint32_t set, uint32_t slot, const Ref<SamplerState>& sampler);

        const Ref<UniformBufferBlock>& GetUniformBlockBuffer(uint32_t slot, uint32_t set) const;
        const Ref<UniformDesc>& GetUniformDesc(ShaderType shaderType) const
        {
            return m_ParamInfo->GetUniformDesc(shaderType);
        }
        static Ref<UniformParams> Create(const Ref<GraphicsPipeline>& pipeline);

    protected:
        struct TextureData
        {
            Ref<Crowny::Texture> Texture;
            TextureSurface Surface;
        };

        Ref<UniformParamInfo> m_ParamInfo;
        Ref<UniformBufferBlock>* m_BufferBlocks = nullptr;
        TextureData* m_SampledTextureData = nullptr;
        // Ref<Texture> m_LoadStoreTextures = nullptr;
        // Ref<GpuBuffer> m_Buffers = nullptr;
        Ref<SamplerState>* m_SamplerStates = nullptr;
    };

} // namespace Crowny
