#include "cwpch.h"

#include "Crowny/RenderAPI/UniformParamInfo.h"
#include "Crowny/RenderAPI/UniformParams.h"

#include "Platform/Vulkan/VulkanUniformParams.h"

namespace Crowny
{

    UniformParams::UniformParams(const Ref<UniformParamInfo>& paramInfo) : m_ParamInfo(paramInfo)
    {
        uint32_t numParamBlocks = m_ParamInfo->GetNumElements(UniformParamInfo::ParamType::ParamBlock);
        uint32_t numTextures = m_ParamInfo->GetNumElements(UniformParamInfo::ParamType::Texture);
        uint32_t numStorageTextures = m_ParamInfo->GetNumElements(UniformParamInfo::ParamType::LoadStoreTexture);
        uint32_t numBuffers = m_ParamInfo->GetNumElements(UniformParamInfo::ParamType::Buffer);
        uint32_t numSamplers = m_ParamInfo->GetNumElements(UniformParamInfo::ParamType::SamplerState);

        m_BufferBlocks = new Ref<UniformBufferBlock>[numParamBlocks];
        m_SampledTextureData = new TextureData[numTextures];
        m_SamplerStates = new Ref<SamplerState>[numSamplers];
    }

    UniformParams::~UniformParams()
    {
        delete[] m_BufferBlocks;
        delete[] m_SampledTextureData;
        delete[] m_SamplerStates;
    }

    void UniformParams::SetUniformBlockBuffer(uint32_t set, uint32_t slot, const Ref<UniformBufferBlock>& uniformBlock)
    {
        uint32_t globalSlot = m_ParamInfo->GetSequentialSlot(UniformParamInfo::ParamType::ParamBlock, set, slot);
        if (globalSlot == -1)
            return;

        m_BufferBlocks[globalSlot] = uniformBlock;
    }

    void UniformParams::SetUniformBlockBuffer(ShaderType type, const String& name,
                                              const Ref<UniformBufferBlock>& uniformBuffer)
    {
        const Ref<UniformDesc>& paramDesc = m_ParamInfo->GetUniformDesc(type);
        if (paramDesc == nullptr)
        {
            CW_ENGINE_ASSERT(false, "Cannot find uniform block.");
            return;
        }

        auto iterFind = paramDesc->Uniforms.find(name);
        if (iterFind == paramDesc->Uniforms.end())
        {
            CW_ENGINE_ASSERT(false, "Cannot find uniform block.");
            return;
        }

        SetUniformBlockBuffer(iterFind->second.Set, iterFind->second.Slot, uniformBuffer);
    }

    void UniformParams::SetUniformBlockBuffer(const String& name, const Ref<UniformBufferBlock>& uniformBuffer)
    {
        for (uint32_t i = 0; i < 6; i++)
        {
            const Ref<UniformDesc>& paramDesc = m_ParamInfo->GetUniformDesc((ShaderType)i);
            if (paramDesc == nullptr)
                continue;

            auto iterFind = paramDesc->Uniforms.find(name);
            if (iterFind == paramDesc->Uniforms.end())
                continue;

            SetUniformBlockBuffer(iterFind->second.Set, iterFind->second.Slot, uniformBuffer);
        }
    }

    const Ref<UniformBufferBlock>& UniformParams::GetUniformBlockBuffer(uint32_t slot, uint32_t set) const
    {
        uint32_t globalSlot = m_ParamInfo->GetSequentialSlot(UniformParamInfo::ParamType::ParamBlock, slot, set);

        return m_BufferBlocks[globalSlot];
    }

    void UniformParams::SetTexture(ShaderType type, const String& name, const Ref<Texture>& texture,
                                   const TextureSurface& surface)
    {
        /*const Ref<UniformParamDesc>& paramDescs = m_ParamInfo->GetParamDescs(type);
        CW_ENGINE_ASSERT(paramDescs != nullptr);

        auto iter = paramDescs->Textures.find(name)
        CW_ENGINE_ASSERT(iter != paramDescs->Textures.end());

        GetTextureParam(type, name, param);
        param.Set(texture, surface);*/
    }

    void UniformParams::SetTexture(uint32_t set, uint32_t slot, const Ref<Texture>& texture,
                                   const TextureSurface& surface)
    {
        uint32_t globalSlot = m_ParamInfo->GetSequentialSlot(UniformParamInfo::ParamType::Texture, set, slot);
        if (globalSlot == (uint32_t)-1)
            return;

        m_SampledTextureData[globalSlot].Texture = texture;
        m_SampledTextureData[globalSlot].Surface = surface;
    }

    void UniformParams::SetSamplerState(uint32_t set, uint32_t slot, const Ref<SamplerState>& sampler)
    {
        uint32_t globalSlot = m_ParamInfo->GetSequentialSlot(UniformParamInfo::ParamType::SamplerState, set, slot);
        if (globalSlot == (uint32_t)-1)
            return;

        m_SamplerStates[globalSlot] = sampler;
    }

    Ref<UniformParams> UniformParams::Create(const Ref<GraphicsPipeline>& pipeline)
    {
        return CreateRef<VulkanUniformParams>(pipeline->GetParamInfo());
    }

} // namespace Crowny
