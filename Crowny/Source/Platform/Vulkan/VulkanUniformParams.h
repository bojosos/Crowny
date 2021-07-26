#pragma once

#include "Platform/Vulkan/VulkanDescriptorPool.h"

#include "Crowny/Renderer/UniformParams.h"
#include "Crowny/Renderer/UniformParamInfo.h"

namespace Crowny
{
    
    class VulkanUniformParamInfo : public UniformParamInfo
    {
    public:
        VulkanUniformParamInfo(const UniformParamDesc& desc);
        ~VulkanUniformParamInfo() = default;
        
        uint32_t GetNumBindings(uint32_t layoutIdx) const { return m_LayoutInfos[layoutIdx].NumBindings; }
        VkDescriptorSetLayoutBinding* GetBindings(uint32_t layoutIdx) const { return m_LayoutInfos[layoutIdx].Bindings; }
        UniformResourceType* GetLayoutTypes(uint32_t layoutIdx) const { return m_LayoutInfos[layoutIdx].Types; }
        GpuBufferFormat* GetLayoutElementTypes(uint32_t layourIdx) const { return m_LayoutInfos[layourIdx].ElementTypes; }
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
    
    class VulkanUniformParams : public UniformParams
    {
    public:
        VulkanUniformParams(const Ref<UniformParamInfo>& params);
        ~VulkanUniformParams();
        virtual void SetUniformBlockBuffer(uint32_t set, uint32_t slot, const Ref<UniformBufferBlock>& uniformBlock) override;
        virtual void SetTexture(uint32_t set, uint32_t slot, const Ref<Texture>& texture, const TextureSurface& surface) override;
        
        //virtual void SetLoadStoreTexture(uint32_t set, uint32_t slot, const Ref<Texture>& texture) override;
        
        //virtual void SetBuffer(uint32_t set, uint32_t slot, const Ref<GpuBuffer>& buffer) override;
        
        virtual void SetSamplerState(uint32_t set, uint32_t slot, const Ref<SamplerState>& sampler) override;
        
        uint32_t GetNumSets() const { return m_ParamInfo->GetNumSets(); };
        void Prepare(VulkanCmdBuffer& buffer, VkDescriptorSet* sets);
    private:
        union WriteInfo
        {
            VkDescriptorImageInfo Image;
            VkDescriptorBufferInfo Buffer;
        };
        
        struct PerSetData
        {
            VulkanDescriptorSet* LatestSet;
            std::vector<VulkanDescriptorSet*> Sets;
            VkWriteDescriptorSet* WriteSetInfos;
            WriteInfo* WriteInfos;
            uint32_t Count;
        };
        
        VkImage* m_SampledImages;
        VkImage* m_StorageImages;
        VkBuffer* m_UniformBuffers;
        VkBuffer* m_Buffers;
        VkSampler* m_Samplers;
        PerSetData* m_PerSetData;
        
        bool* m_SetsDirty = nullptr;
    };
    
}