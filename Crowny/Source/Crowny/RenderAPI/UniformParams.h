#pragma once

#include "Crowny/Common/RefCounted.h"
#include "Crowny/Assets/AssetHandle.h"

#include "Crowny/RenderAPI/AccelerationStructure.h"
#include "Crowny/RenderAPI/GenericGpuBuffer.h"
#include "Crowny/RenderAPI/GraphicsPipeline.h"
#include "Crowny/RenderAPI/SamplerState.h"
#include "Crowny/RenderAPI/Shader.h"
#include "Crowny/RenderAPI/Texture.h"
#include "Crowny/RenderAPI/UniformBufferBlock.h"
#include "Crowny/RenderAPI/UniformParamInfo.h"

namespace Crowny
{

    class UniformParams : public RefCounted
    {
    public:
        virtual ~UniformParams();

        void SetUniformBlockBuffer(ShaderType type, const String& name, const Ref<UniformBufferBlock>& uniformBuffer);
        void SetUniformBlockBuffer(const String& name, const Ref<UniformBufferBlock>& uniformBuffer);
        virtual void SetUniformBlockBuffer(uint32_t set, uint32_t slot, const Ref<UniformBufferBlock>& uniformBuffer);

        Ref<Texture> GetTexture(uint32_t set, uint32_t slot);
        virtual void SetTexture(uint32_t set, uint32_t slot, const Ref<Texture>& texture, const TextureSurface& surface = TextureSurface::COMPLETE);
        virtual void SetTextureArray(uint32_t set, uint32_t slot, const Ref<Texture>* textures, uint32_t count,
                                     const TextureSurface* surfaces = nullptr);
        void SetTexture(ShaderType type, const String& name, const Ref<Texture>& texture, const TextureSurface& surface = TextureSurface::COMPLETE);
        void SetTexture(ShaderType type, HashedString name, const Ref<Texture>& texture,
                        const TextureSurface& surface = TextureSurface::COMPLETE);
        virtual void SetSamplerState(uint32_t set, uint32_t slot, const Ref<SamplerState>& sampler);

        virtual void SetLoadStoreTexture(uint32_t set, uint32_t slot, const Ref<Texture>& texture,
                                         const TextureSurface& surface = TextureSurface::COMPLETE);
        virtual void SetBuffer(uint32_t set, uint32_t slot, const Ref<GenericGpuBuffer>& buffer);
        virtual void SetAccelerationStructure(uint32_t set, uint32_t slot, const Ref<AccelerationStructure>& accelerationStructure);

        const Ref<UniformBufferBlock>& GetUniformBlockBuffer(uint32_t set, uint32_t slot) const;
        const Ref<UniformDesc>& GetUniformDesc(ShaderType shaderType) const { return m_ParamInfo->GetUniformDesc(shaderType); }
        static Ref<UniformParams> Create(const Ref<GraphicsPipeline>& pipeline);
        static Ref<UniformParams> Create(const Ref<ComputePipeline>& pipeline);

        const static GpuDataParameterInfos ParameterInfo;

    protected:
        UniformParams(const Ref<UniformParamInfo>& desc);
        struct TextureData
        {
            Ref<Crowny::Texture> Texture;
            TextureSurface Surface;
        };

        Ref<UniformParamInfo> m_ParamInfo;
        Ref<UniformBufferBlock>* m_BufferBlocks = nullptr;
        TextureData* m_SampledTextureData = nullptr;
        Vector<Vector<TextureData>> m_SampledTextureArrays;
        TextureData* m_LoadStoreTextures = nullptr;
        Ref<GenericGpuBuffer>* m_Buffers = nullptr;
        Ref<SamplerState>* m_SamplerStates = nullptr;
        Ref<AccelerationStructure>* m_AccelStructs = nullptr;
    };

} // namespace Crowny
