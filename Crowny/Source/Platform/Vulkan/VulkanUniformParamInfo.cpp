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

    VulkanUniformParamInfo::VulkanUniformParamInfo(const UniformParamDesc& desc) : UniformParamInfo(desc), m_Layouts(), m_LayoutInfos()
    {
        VulkanDevice& device = *gVulkanRenderAPI().GetPresentDevice().get();
        m_LayoutInfos = new LayoutInfo[m_NumSets];
        m_SetExtraInfos = new SetExtraInfo[m_NumSets];
        for (uint32_t i = 0; i < m_NumSets; i++)
        {
            m_SetExtraInfos[i].SlotIndices = new uint32_t[m_SetInfos[i].NumSlots];
            uint32_t bindingCount = 0;
            for (uint32_t j = 0; j < m_SetInfos[i].NumSlots; j++)
                bindingCount += m_SetInfos[i].SlotIndices[j] != (uint32_t)-1 ? 1u : 0u;
            LayoutInfo& layout = m_LayoutInfos[i];
            layout.NumBindings = bindingCount;
            layout.Bindings = new VkDescriptorSetLayoutBinding[bindingCount]{};
            layout.Types = new UniformResourceType[bindingCount]{};
            layout.ElementTypes = new GpuBufferFormat[bindingCount]{};
            layout.BindingFlags = new VkDescriptorBindingFlags[bindingCount]{};
            uint32_t localIndex = 0;
            for (uint32_t j = 0; j < m_SetInfos[i].NumSlots; j++)
            {
                if (m_SetInfos[i].SlotIndices[j] == (uint32_t)-1)
                {
                    m_SetExtraInfos[i].SlotIndices[j] = (uint32_t)-1;
                    continue;
                }
                VkDescriptorSetLayoutBinding& binding = layout.Bindings[localIndex];
                binding.binding = j;
                m_SetExtraInfos[i].SlotIndices[j] = localIndex++;
            }
        }

        VkShaderStageFlags stageFlagsLookup[SHADER_COUNT];
        stageFlagsLookup[VERTEX_SHADER] = VK_SHADER_STAGE_VERTEX_BIT;
        stageFlagsLookup[HULL_SHADER] = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
        stageFlagsLookup[DOMAIN_SHADER] = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
        stageFlagsLookup[GEOMETRY_SHADER] = VK_SHADER_STAGE_GEOMETRY_BIT;
        stageFlagsLookup[FRAGMENT_SHADER] = VK_SHADER_STAGE_FRAGMENT_BIT;
        stageFlagsLookup[COMPUTE_SHADER] = VK_SHADER_STAGE_COMPUTE_BIT;

        stageFlagsLookup[RAYGEN_SHADER] = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
        stageFlagsLookup[HIT_SHADER] = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
        stageFlagsLookup[MISS_SHADER] = VK_SHADER_STAGE_MISS_BIT_KHR;

        const RenderCapabilities& capabilities = gVulkanRenderAPI().GetCapabilities();
        const bool descriptorArrays = capabilities.HasCapability(CW_DESCRIPTOR_INDEXING) &&
                                      capabilities.HasCapability(CW_NON_UNIFORM_TEXTURE_INDEXING);
        const bool updateAfterBind = descriptorArrays && capabilities.HasCapability(CW_UPDATE_AFTER_BIND);
        const uint32_t runtimeArraySize = std::max(1u, std::min(capabilities.MaxBindlessSampledImages, 4096u));

        for (uint32_t i = 0; i < m_ParamDescs.size(); i++)
        {
            const Ref<UniformDesc>& paramDesc = m_ParamDescs[i];
            if (paramDesc == nullptr)
                continue;

            auto setupBlockBindings = [&](auto& params, VkDescriptorType descType) {
                for (auto& entry : params)
                {
                    const uint32_t bindingIdx = GetBindingIdx(entry.second.Set, entry.second.Slot);
                    VkDescriptorSetLayoutBinding& binding = m_LayoutInfos[entry.second.Set].Bindings[bindingIdx];
                    binding.descriptorCount = 1;
                    binding.descriptorType = descType;
                    binding.stageFlags |= stageFlagsLookup[i];
                }
            };

            auto setupBindings = [&](auto& params, VkDescriptorType descType) {
                for (auto& entry : params)
                {
                    const uint32_t bindingIdx = GetBindingIdx(entry.second.Set, entry.second.Slot);
                    LayoutInfo& layout = m_LayoutInfos[entry.second.Set];
                    VkDescriptorSetLayoutBinding& binding = layout.Bindings[bindingIdx];
                    const uint32_t descriptorCount = entry.second.RuntimeArray ? runtimeArraySize : std::max(entry.second.ArraySize, 1u);
                    binding.descriptorCount = std::max(binding.descriptorCount, descriptorCount);
                    binding.descriptorType = descType;
                    binding.stageFlags |= stageFlagsLookup[i];
                    if (descriptorCount > 1 && updateAfterBind)
                        layout.BindingFlags[bindingIdx] |= VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
                                                           VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;
                    layout.Types[bindingIdx] = entry.second.Type;
                    layout.ElementTypes[bindingIdx] = entry.second.ElementType;
                }
            };

            setupBlockBindings(paramDesc->Uniforms, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
            setupBlockBindings(paramDesc->AccelerationStructures, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR);
            setupBindings(paramDesc->Textures, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
            setupBindings(paramDesc->LoadStoreTextures, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);

            for (const auto& [_, desc] : paramDesc->Samplers)
            {
                const uint32_t bindingIdx = GetBindingIdx(desc.Set, desc.Slot);
                LayoutInfo& layout = m_LayoutInfos[desc.Set];
                VkDescriptorSetLayoutBinding& binding = layout.Bindings[bindingIdx];
                const uint32_t descriptorCount = desc.RuntimeArray ? runtimeArraySize : std::max(desc.ArraySize, 1u);
                binding.descriptorCount = std::max(binding.descriptorCount, descriptorCount);
                if (descriptorCount > 1 && updateAfterBind)
                    layout.BindingFlags[bindingIdx] |= VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
                                                       VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;

                if (binding.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE)
                    binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                else
                {
                    binding.stageFlags |= stageFlagsLookup[i];
                    binding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
                    layout.Types[bindingIdx] = desc.Type;
                    layout.ElementTypes[bindingIdx] = desc.ElementType;
                }
            }

            for (auto& entry : paramDesc->Buffers)
            {
                const uint32_t bindingIdx = GetBindingIdx(entry.second.Set, entry.second.Slot);
                LayoutInfo& layout = m_LayoutInfos[entry.second.Set];
                VkDescriptorSetLayoutBinding& binding = layout.Bindings[bindingIdx];
                binding.descriptorCount = std::max(entry.second.ArraySize, 1u);
                binding.stageFlags |= stageFlagsLookup[i];
                switch (entry.second.Type)
                {
                case BYTE_BUFFER:
                    binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
                    break;
                case RWBYTE_BUFFER:
                    binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
                    break;
                case STRUCTURED_BUFFER:
                case RWSTRUCTURED_BUFFER:
                    binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                    break;
                }

                layout.Types[bindingIdx] = entry.second.Type;
                layout.ElementTypes[bindingIdx] = entry.second.ElementType;
            }
        }
        VulkanDescriptorManager& descManager = device.GetDescriptorManager();
        m_Layouts = new VulkanDescriptorLayout*[m_NumSets];
        for (uint32_t i = 0; i < m_NumSets; i++)
            m_Layouts[i] = descManager.GetLayout(m_LayoutInfos[i].Bindings, m_LayoutInfos[i].NumBindings,
                                                 m_LayoutInfos[i].BindingFlags);
    }

    VulkanUniformParamInfo::~VulkanUniformParamInfo()
    {
        for (uint32_t set = 0; set < m_NumSets; set++)
        {
            delete[] m_SetExtraInfos[set].SlotIndices;
            delete[] m_LayoutInfos[set].Bindings;
            delete[] m_LayoutInfos[set].Types;
            delete[] m_LayoutInfos[set].ElementTypes;
            delete[] m_LayoutInfos[set].BindingFlags;
        }
        delete[] m_SetExtraInfos;
        delete[] m_LayoutInfos;
        delete[] m_Layouts;
    }

} // namespace Crowny
