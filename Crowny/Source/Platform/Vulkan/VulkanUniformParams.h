#pragma once

#include "Crowny/Renderer/UniformParams.h"

namespace Crowny
{
    
    class VulkanUniformParameters : public UniformParams
    {
    public:
        virtual void SetUniformBlockBuffer(uint32_t set, uint32_t slot, const Ref<UniformBufferBlock>* uniformBlock) override;
        
        //virtual void SetTexture(uint32_t set, uint32_t slot, const Ref<Texture>& texture) override;
        
        //virtual void SetLoadStoreTexture(uint32_t set, uint32_t slot, const Ref<Texture>& texture) override;
        
        //virtual void SetBuffer(uint32_t set, uint32_t slot, const Ref<GpuBuffer>& buffer) override;
        
        //virtual void SetSampler(uint32_t set, uint32_t slot, const Ref<Sampler>& sampler) override;
        
        uint32_t GetNumSets() const;
        void Prepare(VulkanCommandBuffer& buffer, VkDescriptorSet* sets);
    private:
        union WriteInfo
        {
            VkDescriptorImageInfo Image;
            VkDescriptorBufferInfo Buffer;
        };
        
        struct SetData
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
        SetData* m_SetData;
        
        bool* m_SetsDirty = nullptr;
    };
    
}