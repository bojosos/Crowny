#include "cwpch.h"

#include "Platform/Vulkan/VulkanUniformParamInfo.h"

#include "Platform/Vulkan/VulkanDescriptorPool.h"
#include "Platform/Vulkan/VulkanGpuBuffer.h"
#include "Platform/Vulkan/VulkanGpuBufferManager.h"
#include "Platform/Vulkan/VulkanRenderAPI.h"
#include "Platform/Vulkan/VulkanSamplerState.h"
#include "Platform/Vulkan/VulkanUniformBufferBlock.h"
#include "Platform/Vulkan/VulkanUniformParams.h"
#include "Platform/Vulkan/VulkanUtils.h"

namespace Crowny
{

    VulkanUniformParamInfo::VulkanUniformParamInfo(const UniformParamDesc& desc)
      : UniformParamInfo(desc), m_Layouts(), m_LayoutInfos()
    {
        VulkanDevice& device = *gVulkanRenderAPI().GetPresentDevice().get();

        uint32_t totalNumSlots = 0;
        for (uint32_t i = 0; i < m_NumSets; i++)
            totalNumSlots += m_SetInfos[i].NumSlots;

        m_LayoutInfos = new LayoutInfo[m_NumSets];
        VkDescriptorSetLayoutBinding* bindings = new VkDescriptorSetLayoutBinding[m_NumElements];
        UniformResourceType* types = new UniformResourceType[m_NumElements];
        GpuBufferFormat* elementTypes = new GpuBufferFormat[m_NumElements];

        m_SetExtraInfos = new SetExtraInfo[m_NumSets];

        if (bindings != nullptr)
            Cw_ZeroOut(bindings, m_NumElements);
        if (types != nullptr)
            Cw_ZeroOut(types, m_NumElements);
        if (elementTypes != nullptr)
            Cw_ZeroOut(elementTypes, m_NumElements);

        uint32_t globalIdx = 0;
        for (uint32_t i = 0; i < m_NumSets; i++)
        {
            m_SetExtraInfos[i].SlotIndices = new uint32_t[m_SetInfos[i].NumSlots];

            m_LayoutInfos[i].NumBindings = 0;
            m_LayoutInfos[i].Bindings = nullptr;
            m_LayoutInfos[i].Types = nullptr;
            m_LayoutInfos[i].ElementTypes = nullptr;

            for (uint32_t j = 0; j < m_SetInfos[i].NumSlots; j++)
            {
                if (m_SetInfos[i].SlotIndices[j] == (uint32_t)-1)
                {
                    m_SetExtraInfos[i].SlotIndices[j] = (uint32_t)-1;
                    continue;
                }
                VkDescriptorSetLayoutBinding& binding = bindings[globalIdx];
                binding.binding = j;
                m_SetExtraInfos[i].SlotIndices[j] = globalIdx;
                m_LayoutInfos[i].NumBindings++;
                globalIdx++;
            }
        }

        uint32_t offset = 0;
        for (uint32_t i = 0; i < m_NumSets; i++)
        {
            m_LayoutInfos[i].Bindings = &bindings[offset];
            m_LayoutInfos[i].Types = &types[offset];
            m_LayoutInfos[i].ElementTypes = &elementTypes[offset];
            offset += m_LayoutInfos[i].NumBindings;
        }

        VkShaderStageFlags stageFlagsLookup[6];
        stageFlagsLookup[VERTEX_SHADER] = VK_SHADER_STAGE_VERTEX_BIT;
        stageFlagsLookup[HULL_SHADER] = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
        stageFlagsLookup[DOMAIN_SHADER] = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
        stageFlagsLookup[GEOMETRY_SHADER] = VK_SHADER_STAGE_GEOMETRY_BIT;
        stageFlagsLookup[FRAGMENT_SHADER] = VK_SHADER_STAGE_FRAGMENT_BIT;
        stageFlagsLookup[COMPUTE_SHADER] = VK_SHADER_STAGE_COMPUTE_BIT;

        for (uint32_t i = 0; i < m_ParamDescs.size(); i++)
        {
            const Ref<UniformDesc>& paramDesc = m_ParamDescs[i];
            if (paramDesc == nullptr)
                continue;

            auto setupBlockBindings = [&](auto& params, VkDescriptorType descType) {
                for (auto& entry : params)
                {
                    uint32_t bindingIdx = GetBindingIdx(entry.second.Set, entry.second.Slot);
                    VkDescriptorSetLayoutBinding& binding = bindings[bindingIdx];
                    binding.stageFlags |= stageFlagsLookup[i];
                    binding.descriptorType = descType;
                    binding.descriptorCount = 1;
                }
            };

            auto setupBindings = [&](auto& params, VkDescriptorType descType) {
                for (auto& entry : params)
                {
                    uint32_t bindingIdx = GetBindingIdx(entry.second.Set, entry.second.Slot);
                    VkDescriptorSetLayoutBinding& binding = bindings[bindingIdx];
                    binding.descriptorCount = 1;
                    binding.stageFlags |= stageFlagsLookup[i];
                    binding.descriptorType = descType;

                    types[bindingIdx] = entry.second.Type;
                    elementTypes[bindingIdx] = entry.second.ElementType;
                }
            };

            setupBlockBindings(paramDesc->Uniforms, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
            setupBindings(paramDesc->Textures, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
            setupBindings(paramDesc->LoadStoreTextures, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);

            for (auto& entry : paramDesc->Samplers)
            {
                uint32_t bindingIdx = GetBindingIdx(entry.second.Set, entry.second.Slot);
                VkDescriptorSetLayoutBinding& binding = bindings[bindingIdx];

                if (binding.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE)
                    binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                else
                {
                    binding.descriptorCount = 1;
                    binding.stageFlags |= stageFlagsLookup[i];
                    binding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
                    types[bindingIdx] = entry.second.Type;
                    elementTypes[bindingIdx] = entry.second.ElementType;
                }
            }
            /*
            for (auto& entry : paramDesc->Buffers)
            {
                uint32_t bindingidx = GetBindingIdx(entry.second.Set, entry.second.Slot);
                VkDescriptorSetLayoutBinding& binding = bindings[bindingIdx];
                binding.descriptorCount = 1;
                binding.stageFlags |= stageFlagsLookup[i];
                switch(entry.second.type)
                {
                case(BYTE_BUFFER):
                    binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
                    break;
                case(RWBYTE_BUFFER):
                    binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
                    break;
                case(STRUCTURED_BUFFER):
                    binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                    break;
                }

                types[bindingIdx] = entry.second.Type;
                elementTypes[bindingIdx] = entry.second.ElementType;
                binding.descriptorType
            }*/
        }
        VulkanDescriptorManager& descManager = device.GetDescriptorManager();
        m_Layouts = new VulkanDescriptorLayout*[m_NumSets];
        for (uint32_t i = 0; i < m_NumSets; i++)
            m_Layouts[i] = descManager.GetLayout(m_LayoutInfos[i].Bindings, m_LayoutInfos[i].NumBindings);
    }
    
}