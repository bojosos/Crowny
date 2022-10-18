#include "cwpch.h"

#include "Platform/Vulkan/VulkanDescriptorPool.h"
#include "Platform/Vulkan/VulkanDevice.h"
#include "Platform/Vulkan/VulkanRenderAPI.h"
#include "Platform/Vulkan/VulkanUtils.h"

namespace Crowny
{

    VulkanLayoutKey::VulkanLayoutKey(VkDescriptorSetLayoutBinding* bindings, uint32_t numBindings)
      : NumBindings(numBindings), Bindings(bindings)
    {
    }

    size_t VulkanLayoutKey::HashFunction::operator()(const VulkanLayoutKey& key) const
    {
        size_t hash = 0;
        for (uint32_t i = 0; i < key.NumBindings; i++)
        {
            size_t hashC = 0;
            HashCombine(hash, key.Bindings[i].binding, key.Bindings[i].descriptorCount, key.Bindings[i].descriptorType,
                        key.Bindings[i].stageFlags);
        }

        return hash;
    }

    bool VulkanLayoutKey::EqualsFunction::operator()(const VulkanLayoutKey& lhs, const VulkanLayoutKey& rhs) const
    {
        if (lhs.Layout != nullptr && rhs.Layout != nullptr)
            return lhs.Layout == rhs.Layout;
        if (lhs.NumBindings != rhs.NumBindings)
            return false;
        for (uint32_t i = 0; i < lhs.NumBindings; i++)
        {
            if (lhs.Bindings[i].binding != rhs.Bindings[i].binding)
                return false;
            if (lhs.Bindings[i].descriptorType != rhs.Bindings[i].descriptorType)
                return false;
            if (lhs.Bindings[i].descriptorCount != rhs.Bindings[i].descriptorCount)
                return false;
            if (lhs.Bindings[i].stageFlags != rhs.Bindings[i].stageFlags)
                return false;
        }

        return true;
    }

    VulkanPipelineLayoutKey::VulkanPipelineLayoutKey(VulkanDescriptorLayout** layouts, uint32_t numLayouts)
      : NumLayouts(numLayouts), Layouts(layouts)
    {
    }

    bool VulkanPipelineLayoutKey::EqualsFunction::operator()(const VulkanPipelineLayoutKey& lhs,
                                                             const VulkanPipelineLayoutKey& rhs) const
    {
        if (lhs.NumLayouts != rhs.NumLayouts)
            return false;
        for (uint32_t i = 0; i < lhs.NumLayouts; i++)
            if (lhs.Layouts[i] != rhs.Layouts[i])
                return false;

        return true;
    }

    size_t VulkanPipelineLayoutKey::HashFunction::operator()(const VulkanPipelineLayoutKey& key) const
    {
        size_t hash = 0;
        for (uint32_t i = 0; i < key.NumLayouts; i++)
            HashCombine(hash, key.Layouts[i]->Hash());

        return hash;
    }

    VulkanDescriptorManager::VulkanDescriptorManager(VulkanDevice& device) : m_Device(device)
    {
        m_Pools.push_back(new VulkanDescriptorPool(device));
    }

    VulkanDescriptorManager::~VulkanDescriptorManager()
    {
        for (auto& entry : m_Layouts)
        {
            delete entry.Layout;
            delete[] entry.Bindings;
        }

        for (auto& entry : m_PipelineLayouts)
        {
            delete[] entry.first.Layouts;
            vkDestroyPipelineLayout(m_Device.GetLogicalDevice(), entry.second, gVulkanAllocator);
        }

        for (auto& entry : m_Pools)
            delete entry;
    }

    VulkanDescriptorLayout* VulkanDescriptorManager::GetLayout(VkDescriptorSetLayoutBinding* bindings,
                                                               uint32_t numBindings)
    {
        VulkanLayoutKey key(bindings, numBindings);

        auto iter = m_Layouts.find(key);
        if (iter != m_Layouts.end())
            return iter->Layout;

        key.Bindings = new VkDescriptorSetLayoutBinding[numBindings];
        memcpy(key.Bindings, bindings, numBindings * sizeof(VkDescriptorSetLayoutBinding));
        key.Layout = new VulkanDescriptorLayout(m_Device, key.Bindings, numBindings);
        m_Layouts.insert(key);

        return key.Layout;
    }

    VulkanDescriptorSet* VulkanDescriptorManager::CreateSet(VulkanDescriptorLayout* layout)
    {
        VkDescriptorSetLayout setLayout = layout->GetHandle();
        VkDescriptorSetAllocateInfo allocateInfo;
        allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocateInfo.pNext = nullptr;
        allocateInfo.descriptorPool = m_Pools.back()->GetHandle();
        allocateInfo.descriptorSetCount = 1;
        allocateInfo.pSetLayouts = &setLayout;

        VkDescriptorSet set;
        VkResult result = vkAllocateDescriptorSets(m_Device.GetLogicalDevice(), &allocateInfo, &set);
        if (result > 0)
        {
            m_Pools.push_back(new VulkanDescriptorPool(m_Device));
            allocateInfo.descriptorPool = m_Pools.back()->GetHandle();
            result = vkAllocateDescriptorSets(m_Device.GetLogicalDevice(), &allocateInfo, &set);
            CW_ENGINE_ASSERT(result == VK_SUCCESS);
        }

        return m_Device.GetResourceManager().Create<VulkanDescriptorSet>(set, allocateInfo.descriptorPool);
    }

    VkPipelineLayout VulkanDescriptorManager::GetPipelineLayout(VulkanDescriptorLayout** layouts, uint32_t numLayouts)
    {
        VulkanPipelineLayoutKey key(layouts, numLayouts);
        auto iter = m_PipelineLayouts.find(key);
        if (iter != m_PipelineLayouts.end())
            return iter->second;

        VkDescriptorSetLayout* setLayouts = new VkDescriptorSetLayout[numLayouts];
        for (uint32_t i = 0; i < numLayouts; i++)
            setLayouts[i] = layouts[i]->GetHandle();

        VkPipelineLayoutCreateInfo layoutCI;
        layoutCI.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layoutCI.pNext = nullptr;
        layoutCI.flags = 0;
        layoutCI.pushConstantRangeCount = 0;
        layoutCI.pPushConstantRanges = nullptr;
        layoutCI.setLayoutCount = numLayouts;
        layoutCI.pSetLayouts = setLayouts;

        VkPipelineLayout layout;
        VkResult result = vkCreatePipelineLayout(m_Device.GetLogicalDevice(), &layoutCI, gVulkanAllocator, &layout);
        CW_ENGINE_ASSERT(result == VK_SUCCESS);
        delete[] setLayouts;
        key.Layouts = new VulkanDescriptorLayout*[numLayouts];
        memcpy(key.Layouts, layouts, sizeof(VulkanDescriptorLayout*) * numLayouts);
        m_PipelineLayouts.insert(std::make_pair(key, layout));
        return layout;
    }

    VulkanDescriptorPool::VulkanDescriptorPool(VulkanDevice& device) : m_Device(device)
    {
        VkDescriptorPoolSize poolSizes[8];
        poolSizes[0].type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        poolSizes[0].descriptorCount = s_MaxSampledImages;

        poolSizes[1].type = VK_DESCRIPTOR_TYPE_SAMPLER;
        poolSizes[1].descriptorCount = s_MaxSampledImages;

        poolSizes[2].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSizes[2].descriptorCount = s_MaxSampledImages;

        poolSizes[3].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSizes[3].descriptorCount = s_MaxUniformBuffers;

        poolSizes[4].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        poolSizes[4].descriptorCount = s_MaxImages;

        poolSizes[5].type = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
        poolSizes[5].descriptorCount = s_MaxSampledBuffers;

        poolSizes[6].type = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
        poolSizes[6].descriptorCount = s_MaxBuffers;

        poolSizes[7].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        poolSizes[7].descriptorCount = s_MaxBuffers;

        VkDescriptorPoolCreateInfo poolCreateInfo;
        poolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolCreateInfo.pNext = nullptr;
        poolCreateInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        poolCreateInfo.maxSets = s_MaxSets;
        poolCreateInfo.poolSizeCount = sizeof(poolSizes) / sizeof(poolSizes[0]);
        poolCreateInfo.pPoolSizes = poolSizes;

        VkResult result =
          vkCreateDescriptorPool(m_Device.GetLogicalDevice(), &poolCreateInfo, gVulkanAllocator, &m_Pool);
        CW_ENGINE_ASSERT(result == VK_SUCCESS);
    }

    VulkanDescriptorPool::~VulkanDescriptorPool()
    {
        vkDestroyDescriptorPool(m_Device.GetLogicalDevice(), m_Pool, gVulkanAllocator);
    }

    VulkanDescriptorSet::VulkanDescriptorSet(VulkanResourceManager* owner, VkDescriptorSet set, VkDescriptorPool pool)
      : VulkanResource(owner, true), m_Set(set), m_Pool(pool)
    {
        m_Device = gVulkanRenderAPI().GetPresentDevice()->GetLogicalDevice();
    }

    VulkanDescriptorSet::~VulkanDescriptorSet()
    {
        VkResult result = vkFreeDescriptorSets(m_Device, m_Pool, 1, &m_Set);
        CW_ENGINE_ASSERT(result == VK_SUCCESS);
    }

    void VulkanDescriptorSet::Write(VkWriteDescriptorSet* entries, uint32_t count)
    {
        for (uint32_t i = 0; i < count; i++)
            entries[i].dstSet = m_Set;
        vkUpdateDescriptorSets(m_Device, count, entries, 0, nullptr);
    }

    VulkanDescriptorLayout::VulkanDescriptorLayout(VulkanDevice& device, VkDescriptorSetLayoutBinding* bindings,
                                                   uint32_t numBindings)
      : m_Device(device)
    {
        m_Hash = 0;
        for (uint32_t i = 0; i < numBindings; i++)
            HashCombine(m_Hash, bindings[i].binding, bindings[i].descriptorCount, bindings[i].descriptorType,
                        bindings[i].stageFlags);

        VkDescriptorSetLayoutCreateInfo layoutCI;
        layoutCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutCI.pNext = nullptr;
        layoutCI.flags = 0;
        layoutCI.pBindings = bindings;
        layoutCI.bindingCount = numBindings;

        VkResult result =
          vkCreateDescriptorSetLayout(device.GetLogicalDevice(), &layoutCI, gVulkanAllocator, &m_Layout);
        CW_ENGINE_ASSERT(result == VK_SUCCESS);
    }

    VulkanDescriptorLayout::~VulkanDescriptorLayout()
    {
        vkDestroyDescriptorSetLayout(m_Device.GetLogicalDevice(), m_Layout, gVulkanAllocator);
    }

} // namespace Crowny