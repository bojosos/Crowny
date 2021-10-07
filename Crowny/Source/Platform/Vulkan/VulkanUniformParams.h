#pragma once

#include "Platform/Vulkan/VulkanDescriptorPool.h"

#include "Crowny/RenderAPI/UniformParamInfo.h"
#include "Crowny/RenderAPI/UniformParams.h"

namespace Crowny
{

    class VulkanUniformParams : public UniformParams
    {
    public:
        VulkanUniformParams(const Ref<UniformParamInfo>& params);
        ~VulkanUniformParams();
        virtual void SetUniformBlockBuffer(uint32_t set, uint32_t slot,
                                           const Ref<UniformBufferBlock>& uniformBlock) override;
        virtual void SetTexture(uint32_t set, uint32_t slot, const Ref<Texture>& texture,
                                const TextureSurface& surface) override;

        // virtual void SetLoadStoreTexture(uint32_t set, uint32_t slot, const Ref<Texture>& texture) override;

        // virtual void SetBuffer(uint32_t set, uint32_t slot, const Ref<GpuBuffer>& buffer) override;

        virtual void SetSamplerState(uint32_t set, uint32_t slot, const Ref<SamplerState>& sampler) override;

        uint32_t GetNumSets() const { return m_ParamInfo->GetNumSets(); };
        void Prepare(VulkanCmdBuffer& buffer, VkDescriptorSet* sets);

    private:
        union WriteInfo {
            VkDescriptorImageInfo Image;
            VkDescriptorBufferInfo Buffer;
        };

        struct PerSetData
        {
            VulkanDescriptorSet* LatestSet;
            Vector<VulkanDescriptorSet*> Sets;
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

} // namespace Crowny