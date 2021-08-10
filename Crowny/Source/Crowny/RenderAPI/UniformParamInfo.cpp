#include "cwpch.h"

#include "Crowny/RenderAPI/UniformParamInfo.h"

#include "Crowny/Renderer/Renderer.h"

#include "Platform/Vulkan/VulkanUniformParamInfo.h"

namespace Crowny
{

    UniformParamInfo::UniformParamInfo(const UniformParamDesc& desc)
      : m_NumSets(0), m_NumElements(0), m_SetInfos(nullptr), m_ResourceInfos()
    {
        Cw_ZeroOut(m_NumElementsPerType);
        m_ParamDescs[FRAGMENT_SHADER] = desc.FragmentParams;
        m_ParamDescs[VERTEX_SHADER] = desc.VertexParams;
        m_ParamDescs[GEOMETRY_SHADER] = desc.GeometryParams;
        m_ParamDescs[HULL_SHADER] = desc.HullParams;
        m_ParamDescs[DOMAIN_SHADER] = desc.DomainParams;
        m_ParamDescs[COMPUTE_SHADER] = desc.ComputeParams;

        auto countElements = [&](auto& entry, ParamType type) {
            int typeIdx = (int)type;
            if ((entry.Set + 1) > m_NumSets)
                m_NumSets = entry.Set + 1;
            m_NumElementsPerType[typeIdx]++;
            m_NumElements++;
        };

        for (uint32_t i = 0; i < m_ParamDescs.size(); i++)
        {
            const Ref<UniformDesc>& paramDesc = m_ParamDescs[i];
            if (paramDesc == nullptr)
                continue;

            for (auto& paramBlock : paramDesc->Uniforms)
                countElements(paramBlock.second, ParamType::ParamBlock);

            for (auto& texture : paramDesc->Textures)
                countElements(texture.second, ParamType::Texture);

            for (auto& texture : paramDesc->LoadStoreTextures)
                countElements(texture.second, ParamType::LoadStoreTexture);

            // for (auto& buffer : paramDesc->Buffers)
            //     countElements(buffer.second, ParamType::Buffer);

            for (auto& sampler : paramDesc->Samplers)
                countElements(sampler.second, ParamType::SamplerState);
        }

        uint32_t* numSlotsPerSet = new uint32_t[m_NumSets];
        Cw_ZeroOut(numSlotsPerSet, m_NumSets);
        for (uint32_t i = 0; i < m_ParamDescs.size(); i++)
        {
            const Ref<UniformDesc>& paramDesc = m_ParamDescs[i];
            if (paramDesc == nullptr)
                continue;

            for (auto& paramBlock : paramDesc->Uniforms)
                numSlotsPerSet[paramBlock.second.Set] =
                  std::max(numSlotsPerSet[paramBlock.second.Set], paramBlock.second.Slot + 1);

            for (auto& texture : paramDesc->Textures)
                numSlotsPerSet[texture.second.Set] =
                  std::max(numSlotsPerSet[texture.second.Set], texture.second.Slot + 1);

            for (auto& sampler : paramDesc->Samplers)
                numSlotsPerSet[sampler.second.Set] =
                  std::max(numSlotsPerSet[sampler.second.Set], sampler.second.Slot + 1);
        }

        uint32_t totalNumSlots = 0;
        for (uint32_t i = 0; i < m_NumSets; i++)
            totalNumSlots += numSlotsPerSet[i];

        m_SetInfos = new SetInfo[m_NumSets];
        if (m_SetInfos != nullptr)
            Cw_ZeroOut(m_SetInfos, m_NumSets);

        for (uint32_t i = 0; i < m_NumSets; i++)
            m_SetInfos[i].NumSlots = numSlotsPerSet[i];

        delete[] numSlotsPerSet;

        for (uint32_t i = 0; i < m_NumSets; i++)
        {
            m_SetInfos[i].SlotIndices = new uint32_t[m_SetInfos[i].NumSlots];
            std::memset(m_SetInfos[i].SlotIndices, -1, sizeof(uint32_t) * m_SetInfos[i].NumSlots);

            m_SetInfos[i].SlotTypes = new ParamType[m_SetInfos[i].NumSlots];

            m_SetInfos[i].SlotSamplers = new uint32_t[m_SetInfos[i].NumSlots];
            std::memset(m_SetInfos[i].SlotSamplers, -1, sizeof(uint32_t) * m_SetInfos[i].NumSlots);
        }

        for (uint32_t i = 0; i < (uint32_t)ParamType::Count; i++)
        {
            m_ResourceInfos[i] = new ResourceInfo[m_NumElementsPerType[i]];
            m_NumElementsPerType[i] = 0;
        }

        auto populateSetInfo = [&](auto& entry, ParamType type) {
            int typeIdx = (int)type;
            uint32_t seqIdx = m_NumElementsPerType[typeIdx];
            SetInfo& setInfo = m_SetInfos[entry.Set];
            setInfo.SlotIndices[entry.Slot] = seqIdx;
            setInfo.SlotTypes[entry.Slot] = type;

            m_ResourceInfos[typeIdx][seqIdx].Set = entry.Set;
            m_ResourceInfos[typeIdx][seqIdx].Slot = entry.Slot;
            m_NumElementsPerType[typeIdx]++;
        };

        for (uint32_t i = 0; i < m_ParamDescs.size(); i++)
        {
            const Ref<UniformDesc>& paramDesc = m_ParamDescs[i];
            if (paramDesc == nullptr)
                continue;
            for (auto& paramBlock : paramDesc->Uniforms)
                populateSetInfo(paramBlock.second, ParamType::ParamBlock);
            for (auto& texture : paramDesc->Textures)
                populateSetInfo(texture.second, ParamType::Texture);

            int typeIdx = (int)ParamType::SamplerState;
            for (auto& entry : paramDesc->Samplers)
            {
                const UniformResourceDesc& samplerDesc = entry.second;
                uint32_t seqIdx = m_NumElementsPerType[typeIdx];

                SetInfo& setInfo = m_SetInfos[samplerDesc.Set];
                if (setInfo.SlotIndices[samplerDesc.Slot] == (uint32_t)-1)
                {
                    setInfo.SlotIndices[samplerDesc.Slot] = seqIdx;
                    setInfo.SlotTypes[samplerDesc.Slot] = ParamType::SamplerState;
                }
                else
                    setInfo.SlotSamplers[samplerDesc.Slot] = seqIdx;
                m_ResourceInfos[typeIdx][seqIdx].Set = samplerDesc.Set;
                m_ResourceInfos[typeIdx][seqIdx].Slot = samplerDesc.Slot;
                m_NumElementsPerType[typeIdx]++;
            }
        }
    }

    uint32_t UniformParamInfo::GetSequentialSlot(ParamType paramType, uint32_t set, uint32_t slot) const
    {
        if (set >= m_NumSets)
        {
            CW_ENGINE_ASSERT(false, "Set index out of range.");
            return -1;
        }

        if (slot >= m_SetInfos[set].NumSlots)
        {
            CW_ENGINE_ASSERT(false, "Slot index out of range.");
            return -1;
        }

        ParamType type = m_SetInfos[set].SlotTypes[slot];
        if (type != paramType)
        {
            if (type == ParamType::SamplerState)
            {
                if (m_SetInfos[set].SlotSamplers[set] != (uint32_t)-1)
                    return m_SetInfos[set].SlotSamplers[slot];
            }
            CW_ENGINE_ERROR("Parameters are of the wrong type. Requested: {0}, actual:", (uint32_t)type,
                            uint32_t(m_SetInfos[set].SlotTypes[slot]));
            return -1;
        }

        return m_SetInfos[set].SlotIndices[slot];
    }

    void UniformParamInfo::GetBinding(ParamType type, uint32_t seqSlot, uint32_t& set, uint32_t& slot) const
    {
        if (seqSlot >= m_NumElementsPerType[(int)type])
        {
            CW_ENGINE_ASSERT(false, "Sequential slot index out of range.");
            slot = 0;
            set = 0;
            return;
        }
        set = m_ResourceInfos[(int)type][seqSlot].Set;
        slot = m_ResourceInfos[(int)type][seqSlot].Slot;
    }

    void UniformParamInfo::GetBinding(ShaderType type, ParamType paramType, const std::string& name,
                                      UniformBinding& binding)
    {
        auto findBinding = [](auto& paramMap, const std::string& name, UniformBinding& binding) {
            auto iterFind = paramMap.find(name);
            if (iterFind != paramMap.end())
            {
                binding.Set = iterFind->second.Set;
                binding.Slot = iterFind->second.Slot;
            }
            else
                binding.Set = binding.Slot = (uint32_t)-1;
        };

        const Ref<UniformDesc>& paramDesc = m_ParamDescs[(uint32_t)type];
        if (paramDesc == nullptr)
        {
            binding.Set = binding.Slot = (uint32_t)-1;
            return;
        }

        switch (paramType)
        {
        case (ParamType::ParamBlock):
            findBinding(paramDesc->Uniforms, name, binding);
            break;
        case (ParamType::Texture):
            findBinding(paramDesc->Textures, name, binding);
            break;
        case (ParamType::LoadStoreTexture):
            findBinding(paramDesc->LoadStoreTextures, name, binding);
            break;
        case (ParamType::SamplerState):
            findBinding(paramDesc->Samplers, name, binding);
            break;
        default:
            break;
        }
    }

    Ref<UniformParamInfo> UniformParamInfo::Create(const UniformParamDesc& desc)
    {
        switch (Renderer::GetAPI())
        {
        // case RenderAPI::API::OpenGL: return CreateRef<OpenGLShader>(m_Filepath);
        case RenderAPI::API::Vulkan:
            return CreateRef<VulkanUniformParamInfo>(desc);
        default:
            CW_ENGINE_ASSERT(false, "Renderer API not supporter");
            return nullptr;
        }

        return nullptr;
    }

} // namespace Crowny