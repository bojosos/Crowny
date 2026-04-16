#include "cwpch.h"

#include "Crowny/RenderAPI/UniformParamInfo.h"
#include "Crowny/RenderAPI/UniformParams.h"

#include "Platform/Vulkan/VulkanUniformParams.h"

namespace Crowny
{

    const GpuDataParameterInfos UniformParams::ParameterInfo;

    UniformParams::UniformParams(const Ref<UniformParamInfo>& paramInfo) : m_ParamInfo(paramInfo)
    {
        const uint32_t numParamBlocks = m_ParamInfo->GetNumElements(UniformParamInfo::ParamType::ParamBlock);
        const uint32_t numTextures = m_ParamInfo->GetNumElements(UniformParamInfo::ParamType::Texture);
        const uint32_t numStorageTextures = m_ParamInfo->GetNumElements(UniformParamInfo::ParamType::LoadStoreTexture);
        const uint32_t numBuffers = m_ParamInfo->GetNumElements(UniformParamInfo::ParamType::Buffer);
        const uint32_t numSamplers = m_ParamInfo->GetNumElements(UniformParamInfo::ParamType::SamplerState);
        const uint32_t numAccelStructs = m_ParamInfo->GetNumElements(UniformParamInfo::ParamType::AccelStruct);

        m_BufferBlocks = new Ref<UniformBufferBlock>[numParamBlocks];
        m_SampledTextureData = new TextureData[numTextures];
        m_SamplerStates = new Ref<SamplerState>[numSamplers];
        m_LoadStoreTextures = new TextureData[numStorageTextures];
        m_Buffers = new Ref<GenericGpuBuffer>[numBuffers];
        m_AccelStructs = new Ref<AccelerationStructure>[numAccelStructs];
    }

    UniformParams::~UniformParams()
    {
        delete[] m_BufferBlocks;
        delete[] m_SampledTextureData;
        delete[] m_SamplerStates;
        delete[] m_Buffers;
        delete[] m_LoadStoreTextures;
        delete[] m_AccelStructs;
    }

    void UniformParams::SetUniformBlockBuffer(uint32_t set, uint32_t slot, const Ref<UniformBufferBlock>& uniformBlock)
    {
        const uint32_t globalSlot = m_ParamInfo->GetSequentialSlot(UniformParamInfo::ParamType::ParamBlock, set, slot);
        if (globalSlot == -1)
            return;

        m_BufferBlocks[globalSlot] = uniformBlock;
    }

    void UniformParams::SetUniformBlockBuffer(ShaderType type, const String& name, const Ref<UniformBufferBlock>& uniformBuffer)
    {
        const Ref<UniformDesc>& paramDesc = m_ParamInfo->GetUniformDesc(type);
        if (paramDesc == nullptr)
        {
            CW_ENGINE_ASSERT(false, "Cannot find uniform block.");
            return;
        }

        const auto iterFind = paramDesc->Uniforms.find(name);
        if (iterFind == paramDesc->Uniforms.end())
        {
            CW_ENGINE_ASSERT(false, "Cannot find uniform block.");
            return;
        }

        SetUniformBlockBuffer(iterFind->second.Set, iterFind->second.Slot, uniformBuffer);
    }

    void UniformParams::SetUniformBlockBuffer(const String& name, const Ref<UniformBufferBlock>& uniformBuffer)
    {
        for (uint32_t i = 0; i < SHADER_COUNT; i++)
        {
            const Ref<UniformDesc>& paramDesc = m_ParamInfo->GetUniformDesc((ShaderType)i);
            if (paramDesc == nullptr)
                continue;

            const auto iterFind = paramDesc->Uniforms.find(name);
            if (iterFind == paramDesc->Uniforms.end())
                continue;

            SetUniformBlockBuffer(iterFind->second.Set, iterFind->second.Slot, uniformBuffer);
        }
    }

    const Ref<UniformBufferBlock>& UniformParams::GetUniformBlockBuffer(uint32_t slot, uint32_t set) const
    {
        const uint32_t globalSlot = m_ParamInfo->GetSequentialSlot(UniformParamInfo::ParamType::ParamBlock, slot, set);

        return m_BufferBlocks[globalSlot];
    }

    Ref<Texture> UniformParams::GetTexture(uint32_t set, uint32_t slot)
    {
        const uint32_t globalSlot = m_ParamInfo->GetSequentialSlot(UniformParamInfo::ParamType::Texture, set, slot);
        return m_SampledTextureData[globalSlot].Texture;
    }

    void UniformParams::SetTexture(ShaderType type, const String& name, const Ref<Texture>& texture, const TextureSurface& surface)
    {
        const UnorderedMap<String, UniformResourceDesc>& textures = m_ParamInfo->GetUniformDesc(type)->Textures;
        const auto iterFind = textures.find(name);
        if (iterFind != textures.cend())
            SetTexture(iterFind->second.Set, iterFind->second.Slot, texture, surface);
        else
            CW_ENGINE_WARN("Texture with name {} does not exist in the fragment shader", name);
    }

    void UniformParams::SetTexture(uint32_t set, uint32_t slot, const Ref<Texture>& texture, const TextureSurface& surface)
    {
        const uint32_t globalSlot = m_ParamInfo->GetSequentialSlot(UniformParamInfo::ParamType::Texture, set, slot);
        if (globalSlot == (uint32_t)-1)
            return;

        m_SampledTextureData[globalSlot].Texture = texture;
        m_SampledTextureData[globalSlot].Surface = surface;
    }

    void UniformParams::SetBuffer(uint32_t set, uint32_t slot, const Ref<GenericGpuBuffer>& buffer)
    {
        const uint32_t globalSlot = m_ParamInfo->GetSequentialSlot(UniformParamInfo::ParamType::Buffer, set, slot);
        if (globalSlot == (uint32_t)-1)
            return;

        m_Buffers[globalSlot] = buffer;
    }

    void UniformParams::SetLoadStoreTexture(uint32_t set, uint32_t slot, const Ref<Texture>& texture, const TextureSurface& surface)
    {
        const uint32_t globalSlot = m_ParamInfo->GetSequentialSlot(UniformParamInfo::ParamType::LoadStoreTexture, set, slot);
        if (globalSlot == (uint32_t)-1)
            return;

        m_LoadStoreTextures[globalSlot].Texture = texture;
        m_LoadStoreTextures[globalSlot].Surface = surface;
    }

    void UniformParams::SetSamplerState(uint32_t set, uint32_t slot, const Ref<SamplerState>& sampler)
    {
        const uint32_t globalSlot = m_ParamInfo->GetSequentialSlot(UniformParamInfo::ParamType::SamplerState, set, slot);
        if (globalSlot == (uint32_t)-1)
            return;

        m_SamplerStates[globalSlot] = sampler;
    }

    void UniformParams::SetAccelerationStructure(uint32_t set, uint32_t slot, const Ref<AccelerationStructure>& accelerationStructure)
    {

        const uint32_t globalSlot = m_ParamInfo->GetSequentialSlot(UniformParamInfo::ParamType::AccelStruct, set, slot);
        if (globalSlot == (uint32_t)-1)
            return;

        m_AccelStructs[globalSlot] = accelerationStructure;
    }

    Ref<UniformParams> UniformParams::Create(const Ref<GraphicsPipeline>& pipeline)
    {
        return Ref<UniformParams>(new VulkanUniformParams(pipeline->GetParamInfo()));
    }

} // namespace Crowny
