#pragma once

#include "Platform/Vulkan/VulkanDescriptorPool.h"

#include "Crowny/Common/Module.h"
#include "Crowny/RenderAPI/UniformParamInfo.h"
#include "Crowny/RenderAPI/UniformParams.h"

namespace Crowny
{
    class VulkanCmdBuffer;

    class VulkanTextureManager : public Module<VulkanTextureManager>
    {
    public:
        virtual void OnStartUp() override;
        virtual void OnShutdown() override;
        VulkanTexture* GetDummyTexture(UniformResourceType type);
        static VkFormat GetDummyViewFormat(GpuBufferFormat format);

    private:
        Ref<VulkanTexture> m_DummyReadTextures[7];
        Ref<VulkanTexture> m_DummyStorageTextures[7];
    };

    class VulkanUniformParams : public UniformParams
    {
    public:
        friend class UniformParams;
        ~VulkanUniformParams();

        virtual void SetUniformBlockBuffer(uint32_t set, uint32_t slot, const Ref<UniformBufferBlock>& uniformBlock) override;
        virtual void SetTexture(uint32_t set, uint32_t slot, const Ref<Texture>& texture, const TextureSurface& surface) override;
        virtual void SetTextureArray(uint32_t set, uint32_t slot, const Ref<Texture>* textures, uint32_t count,
                                     const TextureSurface* surfaces = nullptr) override;
        virtual void SetSamplerState(uint32_t set, uint32_t slot, const Ref<SamplerState>& sampler) override;
        virtual void SetLoadStoreTexture(uint32_t set, uint32_t slot, const Ref<Texture>& texture, const TextureSurface& surface) override;
        virtual void SetBuffer(uint32_t set, uint32_t slot, const Ref<GenericGpuBuffer>& buffer) override;
        virtual void SetAccelerationStructure(uint32_t set, uint32_t slot, const Ref<AccelerationStructure>& accelStruct) override;

        uint32_t GetNumSets() const { return m_ParamInfo->GetNumSets(); };

        void Prepare(VulkanCmdBuffer& buffer, VkDescriptorSet* sets);

    protected:
        VulkanUniformParams(const Ref<UniformParamInfo>& params);

    private:
        struct AccelStructWriteInfo
        {
            VkWriteDescriptorSetAccelerationStructureKHR AccelStructDesc;
            VkAccelerationStructureKHR AccelStruct;
        };

        union WriteInfo {
            VkDescriptorImageInfo Image;
            VkDescriptorBufferInfo Buffer;
            AccelStructWriteInfo AccelStruct;
            VkBufferView BufferView;
        };

        struct PerSetData
        {
            VulkanDescriptorSet* LatestSet = nullptr;
            Vector<VulkanDescriptorSet*> Sets;
            VkWriteDescriptorSet* WriteSetInfos = nullptr;
            WriteInfo* WriteInfos = nullptr;
            VkDescriptorImageInfo** ImageArrayInfos = nullptr;
            uint32_t Count = 0;
        };

        VkImage* m_SampledImages = nullptr;
        VkImage* m_StorageImages = nullptr;
        VkBuffer* m_UniformBuffers = nullptr;
        VkBuffer* m_Buffers = nullptr;
        VkSampler* m_Samplers = nullptr;
        VkAccelerationStructureKHR* m_AccelerationStructs = nullptr;
        PerSetData* m_PerSetData = nullptr;

        bool* m_SetsDirty = nullptr;
    };

} // namespace Crowny
