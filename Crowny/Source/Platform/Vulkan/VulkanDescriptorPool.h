#pragma once

#include "Platform/Vulkan/VulkanDevice.h"
#include "Platform/Vulkan/VulkanUtils.h"

namespace Crowny
{
    class VulkanDescriptorLayout;

    struct VulkanLayoutKey
    {
        VulkanLayoutKey(VkDescriptorSetLayoutBinding* bindings, uint32_t numBindings);

        struct EqualsFunction
        {
            bool operator()(const VulkanLayoutKey& lhs, const VulkanLayoutKey& rhs) const;
        };

        struct HashFunction
        {
            size_t operator()(const VulkanLayoutKey& key) const;
        };

        uint32_t NumBindings;
        VkDescriptorSetLayoutBinding* Bindings;
        VulkanDescriptorLayout* Layout = nullptr;
    };

    struct VulkanPipelineLayoutKey
    {
        VulkanPipelineLayoutKey(VulkanDescriptorLayout** layouts, uint32_t numLayouts);

        struct EqualsFunction
        {
            bool operator()(const VulkanPipelineLayoutKey& lhs, const VulkanPipelineLayoutKey& key) const;
        };

        struct HashFunction
        {
            size_t operator()(const VulkanPipelineLayoutKey& key) const;
        };

        uint32_t NumLayouts;
        VulkanDescriptorLayout** Layouts;
    };

    class VulkanDescriptorSet : public VulkanResource
    {
    public:
        VulkanDescriptorSet(VulkanResourceManager* owner, VkDescriptorSet set, VkDescriptorPool pool);
        ~VulkanDescriptorSet();

        VkDescriptorSet GetHandle() const { return m_Set; }
        void Write(VkWriteDescriptorSet* entries, uint32_t count);

    private:
        VkDescriptorSet m_Set;
        VkDescriptorPool m_Pool;
        VkDevice m_Device;
    };

    class VulkanDescriptorLayout
    {
    public:
        VulkanDescriptorLayout(VulkanDevice& device, VkDescriptorSetLayoutBinding* bindings, uint32_t numBindings);
        ~VulkanDescriptorLayout();

        VkDescriptorSetLayout GetHandle() const { return m_Layout; }

        size_t Hash() const { return m_Hash; }

    private:
        VulkanDevice& m_Device;
        VkDescriptorSetLayout m_Layout;
        size_t m_Hash;
    };

    class VulkanDescriptorPool
    {
    public:
        VulkanDescriptorPool(VulkanDevice& device);
        ~VulkanDescriptorPool();

        VkDescriptorPool GetHandle() const { return m_Pool; }

    private:
        static const uint32_t s_MaxSets = 2048;
        static const uint32_t s_MaxSampledImages = 1024;
        static const uint32_t s_MaxImages = 512;
        static const uint32_t s_MaxSampledBuffers = 512;
        static const uint32_t s_MaxBuffers = 512;
        static const uint32_t s_MaxUniformBuffers = 512;

        VulkanDevice& m_Device;
        VkDescriptorPool m_Pool;
    };

    class VulkanDescriptorManager
    {
    public:
        VulkanDescriptorManager(VulkanDevice& device);
        ~VulkanDescriptorManager();

        VulkanDescriptorSet* CreateSet(VulkanDescriptorLayout* layout);
        VulkanDescriptorLayout* GetLayout(VkDescriptorSetLayoutBinding* bindings, uint32_t numBindings);

        VkPipelineLayout GetPipelineLayout(VulkanDescriptorLayout** layouts, uint32_t numLayouts);

    private:
        VulkanDevice& m_Device;
        UnorderedSet<VulkanLayoutKey, VulkanLayoutKey::HashFunction, VulkanLayoutKey::EqualsFunction> m_Layouts;
        UnorderedMap<VulkanPipelineLayoutKey, VkPipelineLayout, VulkanPipelineLayoutKey::HashFunction,
                     VulkanPipelineLayoutKey::EqualsFunction>
          m_PipelineLayouts;
        Vector<VulkanDescriptorPool*> m_Pools;
    };

} // namespace Crowny
