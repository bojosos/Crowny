#pragma once

#include "Platform/Vulkan/VulkanDescriptorPool.h"

#include "Crowny/RenderAPI/UniformParamInfo.h"
#include "Crowny/RenderAPI/UniformParams.h"

namespace Crowny
{

    class VulkanUniformParamInfo : public UniformParamInfo
    {
    public:
        VulkanUniformParamInfo(const UniformParamDesc& desc);
        ~VulkanUniformParamInfo() = default;

        uint32_t GetNumBindings(uint32_t layoutIdx) const { return m_LayoutInfos[layoutIdx].NumBindings; }
        VkDescriptorSetLayoutBinding* GetBindings(uint32_t layoutIdx) const
        {
            return m_LayoutInfos[layoutIdx].Bindings;
        }
        UniformResourceType* GetLayoutTypes(uint32_t layoutIdx) const { return m_LayoutInfos[layoutIdx].Types; }
        GpuBufferFormat* GetLayoutElementTypes(uint32_t layourIdx) const
        {
            return m_LayoutInfos[layourIdx].ElementTypes;
        }
        uint32_t GetBindingIdx(uint32_t set, uint32_t slot) const { return m_SetExtraInfos[set].SlotIndices[slot]; }

        VulkanDescriptorLayout* GetLayout(uint32_t layoutIdx) const { return m_Layouts[layoutIdx]; }

    private:
        struct LayoutInfo
        {
            VkDescriptorSetLayoutBinding* Bindings;
            UniformResourceType* Types;
            GpuBufferFormat* ElementTypes;
            uint32_t NumBindings;
        };

        struct SetExtraInfo
        {
            uint32_t* SlotIndices;
        };

        SetExtraInfo* m_SetExtraInfos = nullptr;

        VulkanDescriptorLayout** m_Layouts;
        LayoutInfo* m_LayoutInfos;
    };
}