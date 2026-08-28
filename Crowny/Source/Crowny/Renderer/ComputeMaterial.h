#pragma once

#include "Crowny/Assets/AssetHandle.h"
#include "Crowny/RenderAPI/GraphicsPipeline.h"
#include "Crowny/RenderAPI/UniformBufferBlock.h"
#include "Crowny/RenderAPI/UniformParams.h"

namespace Crowny
{
    class Shader;
    class ShaderVariation;

    // Reflected resource set for an engine compute shader. Unlike Material,
    // this owns a ComputePipeline and never assumes a graphics render pass.
    class ComputeMaterial
    {
    public:
        bool Initialize(const AssetHandle<Shader>& shader);
        void Reset();

        bool IsValid() const { return m_Pipeline != nullptr && m_Uniforms != nullptr; }
        const String& GetError() const { return m_Error; }

        bool WriteUniformBlock(StringView name, const void* data, uint32_t size, uint32_t offset = 0);
        bool WriteUniformBlock(uint32_t set, uint32_t slot, const void* data, uint32_t size, uint32_t offset = 0);
        bool SetBuffer(StringView name, const Ref<GenericGpuBuffer>& buffer);
        bool SetBuffer(uint32_t set, uint32_t slot, const Ref<GenericGpuBuffer>& buffer);
        bool SetTexture(StringView name, const Ref<Texture>& texture,
                        const TextureSurface& surface = TextureSurface::COMPLETE);
        bool SetTexture(uint32_t set, uint32_t slot, const Ref<Texture>& texture,
                        const TextureSurface& surface = TextureSurface::COMPLETE);
        bool SetTextureArray(uint32_t set, uint32_t slot, const Ref<Texture>* textures, uint32_t count,
                             const TextureSurface* surfaces = nullptr);
        bool SetSamplerState(uint32_t set, uint32_t slot, const Ref<SamplerState>& sampler);
        bool SetLoadStoreTexture(StringView name, const Ref<Texture>& texture,
                                 const TextureSurface& surface = TextureSurface::COMPLETE);
        bool SetLoadStoreTexture(uint32_t set, uint32_t slot, const Ref<Texture>& texture,
                                 const TextureSurface& surface = TextureSurface::COMPLETE);
        bool Dispatch(uint32_t groupsX, uint32_t groupsY = 1, uint32_t groupsZ = 1,
                      const Ref<CommandBuffer>& commandBuffer = nullptr);

    private:
        struct UniformBlock
        {
            Ref<UniformBufferBlock> Buffer;
            uint32_t Size = 0;
        };

        AssetHandle<Shader> m_Shader;
        Ref<ComputePipeline> m_Pipeline;
        Ref<UniformParams> m_Uniforms;
        UnorderedMap<String, UniformBlock, StringHash, StringEqual> m_UniformBlocks;
        UnorderedMap<uint64_t, UniformBlock*> m_UniformBlocksByBinding;
        String m_Error;
    };

    class GraphicsMaterial
    {
    public:
        bool Initialize(const AssetHandle<Shader>& shader,
                        const Ref<BlendStateDesc>& blendStateOverride = nullptr,
                        const Ref<DepthStencilStateDesc>& depthStateOverride = nullptr);
        bool Initialize(const AssetHandle<Shader>& shader, const ShaderVariation& variation,
                        const Ref<BlendStateDesc>& blendStateOverride = nullptr,
                        const Ref<DepthStencilStateDesc>& depthStateOverride = nullptr);
        void Reset();
        bool IsValid() const { return m_Pipeline != nullptr && m_Uniforms != nullptr; }
        const String& GetError() const { return m_Error; }

        bool WriteUniformBlock(uint32_t set, uint32_t slot, const void* data, uint32_t size, uint32_t offset = 0);
        bool SetBuffer(uint32_t set, uint32_t slot, const Ref<GenericGpuBuffer>& buffer);
        bool SetTexture(uint32_t set, uint32_t slot, const Ref<Texture>& texture,
                        const TextureSurface& surface = TextureSurface::COMPLETE);
        bool SetTextureArray(uint32_t set, uint32_t slot, const Ref<Texture>* textures, uint32_t count,
                             const TextureSurface* surfaces = nullptr);
        bool SetSamplerState(uint32_t set, uint32_t slot, const Ref<SamplerState>& sampler);
        bool Bind(const Ref<CommandBuffer>& commandBuffer = nullptr);
        const Ref<GraphicsPipeline>& GetPipeline() const { return m_Pipeline; }

    private:
        struct UniformBlock
        {
            Ref<UniformBufferBlock> Buffer;
            uint32_t Size = 0;
        };

        AssetHandle<Shader> m_Shader;
        Ref<GraphicsPipeline> m_Pipeline;
        Ref<UniformParams> m_Uniforms;
        UnorderedMap<uint64_t, UniformBlock> m_UniformBlocks;
        String m_Error;
    };
} // namespace Crowny
