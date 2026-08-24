#include "cwpch.h"

#include "Platform/Vulkan/VulkanDescriptorPool.h"
#include "Platform/Vulkan/VulkanDevice.h"
#include "Platform/Vulkan/VulkanRenderAPI.h"
#include "Platform/Vulkan/VulkanUtils.h"

namespace Crowny
{

    VulkanLayoutKey::VulkanLayoutKey(VkDescriptorSetLayoutBinding* bindings, const VkDescriptorBindingFlags* bindingFlags,
                                     uint32_t numBindings)
      : NumBindings(numBindings), Bindings(bindings), BindingFlags(const_cast<VkDescriptorBindingFlags*>(bindingFlags))
    {
    }

    size_t VulkanLayoutKey::HashFunction::operator()(const VulkanLayoutKey& key) const
    {
        size_t hash = 0;
        for (uint32_t i = 0; i < key.NumBindings; i++)
        {
            size_t hashC = 0;
            HashCombine(hash, key.Bindings[i].binding, key.Bindings[i].descriptorCount, key.Bindings[i].descriptorType, key.Bindings[i].stageFlags);
            HashCombine(hash, key.BindingFlags != nullptr ? key.BindingFlags[i] : 0u);
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
            const VkDescriptorBindingFlags lhsFlags = lhs.BindingFlags != nullptr ? lhs.BindingFlags[i] : 0u;
            const VkDescriptorBindingFlags rhsFlags = rhs.BindingFlags != nullptr ? rhs.BindingFlags[i] : 0u;
            if (lhsFlags != rhsFlags)
                return false;
        }

        return true;
    }

    VulkanPipelineLayoutKey::VulkanPipelineLayoutKey(VulkanDescriptorLayout** layouts, uint32_t numLayouts) : NumLayouts(numLayouts), Layouts(layouts)
    {
    }

    bool VulkanPipelineLayoutKey::EqualsFunction::operator()(const VulkanPipelineLayoutKey& lhs, const VulkanPipelineLayoutKey& rhs) const
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

    VulkanDescriptorManager::VulkanDescriptorManager(VulkanDevice& device) : m_Device(device) { m_Pools.push_back(new VulkanDescriptorPool(device)); }

    VulkanDescriptorManager::~VulkanDescriptorManager()
    {
        for (auto& entry : m_Layouts)
        {
            delete entry.Layout;
            delete[] entry.Bindings;
            delete[] entry.BindingFlags;
        }

        for (auto& entry : m_PipelineLayouts)
        {
            delete[] entry.first.Layouts;
            vkDestroyPipelineLayout(m_Device.GetLogicalDevice(), entry.second, gVulkanAllocator);
        }

        for (auto& entry : m_Pools)
            delete entry;
        for (auto& entry : m_UpdateAfterBindPools)
            delete entry;
    }

    VulkanDescriptorLayout* VulkanDescriptorManager::GetLayout(VkDescriptorSetLayoutBinding* bindings, uint32_t numBindings,
                                                                const VkDescriptorBindingFlags* bindingFlags)
    {
        VulkanLayoutKey key(bindings, bindingFlags, numBindings);

        auto iter = m_Layouts.find(key);
        if (iter != m_Layouts.end())
            return iter->Layout;

        key.Bindings = new VkDescriptorSetLayoutBinding[numBindings];
        memcpy(key.Bindings, bindings, numBindings * sizeof(VkDescriptorSetLayoutBinding));
        key.BindingFlags = new VkDescriptorBindingFlags[numBindings];
        if (bindingFlags != nullptr)
            memcpy(key.BindingFlags, bindingFlags, numBindings * sizeof(VkDescriptorBindingFlags));
        else
            std::fill_n(key.BindingFlags, numBindings, 0u);
        key.Layout = new VulkanDescriptorLayout(m_Device, key.Bindings, key.BindingFlags, numBindings);
        m_Layouts.insert(key);

        return key.Layout;
    }

    VulkanDescriptorSet* VulkanDescriptorManager::CreateSet(VulkanDescriptorLayout* layout)
    {
        VkDescriptorSetLayout setLayout = layout->GetHandle();
        VkDescriptorSetAllocateInfo allocateInfo{};
        allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocateInfo.pNext = nullptr;
        Vector<VulkanDescriptorPool*>& pools = layout->UsesUpdateAfterBind() ? m_UpdateAfterBindPools : m_Pools;
        if (pools.empty())
            pools.push_back(new VulkanDescriptorPool(m_Device, layout->UsesUpdateAfterBind()));
        allocateInfo.descriptorPool = pools.back()->GetHandle();
        allocateInfo.descriptorSetCount = 1;
        allocateInfo.pSetLayouts = &setLayout;

        VkDescriptorSet set = VK_NULL_HANDLE;
        VkResult result = vkAllocateDescriptorSets(m_Device.GetLogicalDevice(), &allocateInfo, &set);
        if (result == VK_ERROR_OUT_OF_POOL_MEMORY || result == VK_ERROR_FRAGMENTED_POOL)
        {
            pools.push_back(new VulkanDescriptorPool(m_Device, layout->UsesUpdateAfterBind()));
            allocateInfo.descriptorPool = pools.back()->GetHandle();
            result = vkAllocateDescriptorSets(m_Device.GetLogicalDevice(), &allocateInfo, &set);
        }

        CW_ENGINE_ASSERT(result == VK_SUCCESS);
        if (result != VK_SUCCESS)
            return nullptr;

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

    VulkanDescriptorPool::VulkanDescriptorPool(VulkanDevice& device, bool updateAfterBind) : m_Device(device)
    {
        const uint32_t sampledImageCapacity = updateAfterBind ? 8192u : s_MaxSampledImages;
        VkDescriptorPoolSize poolSizes[8];
        poolSizes[0].type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        poolSizes[0].descriptorCount = sampledImageCapacity;

        poolSizes[1].type = VK_DESCRIPTOR_TYPE_SAMPLER;
        poolSizes[1].descriptorCount = sampledImageCapacity;

        poolSizes[2].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSizes[2].descriptorCount = sampledImageCapacity;

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

        // poolSizes[8].type = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
        // poolSizes[8].descriptorCount = s_MaxAccelerationStructures;

        VkDescriptorPoolCreateInfo poolCreateInfo;
        poolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolCreateInfo.pNext = nullptr;
        poolCreateInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT |
                               (updateAfterBind ? VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT : 0u);
        poolCreateInfo.maxSets = s_MaxSets;
        poolCreateInfo.poolSizeCount = sizeof(poolSizes) / sizeof(poolSizes[0]);
        poolCreateInfo.pPoolSizes = poolSizes;

        const VkResult result = vkCreateDescriptorPool(m_Device.GetLogicalDevice(), &poolCreateInfo, gVulkanAllocator, &m_Pool);
        CW_ENGINE_ASSERT(result == VK_SUCCESS);
    }

    VulkanDescriptorPool::~VulkanDescriptorPool() { vkDestroyDescriptorPool(m_Device.GetLogicalDevice(), m_Pool, gVulkanAllocator); }

    VulkanDescriptorSet::VulkanDescriptorSet(VulkanResourceManager* owner, VkDescriptorSet set, VkDescriptorPool pool)
      : VulkanResource(owner, true), m_Set(set), m_Pool(pool)
    {
        m_Device = m_Owner->GetDevice().GetLogicalDevice();
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
                                                   const VkDescriptorBindingFlags* bindingFlags, uint32_t numBindings)
      : m_Device(device)
    {
        m_Hash = 0;
        for (uint32_t i = 0; i < numBindings; i++)
        {
            HashCombine(m_Hash, bindings[i].binding, bindings[i].descriptorCount, bindings[i].descriptorType, bindings[i].stageFlags);
            const VkDescriptorBindingFlags flags = bindingFlags != nullptr ? bindingFlags[i] : 0u;
            HashCombine(m_Hash, flags);
            m_UpdateAfterBind = m_UpdateAfterBind || (flags & VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT) != 0;
        }

        VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsInfo{};
        bindingFlagsInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
        bindingFlagsInfo.bindingCount = numBindings;
        bindingFlagsInfo.pBindingFlags = bindingFlags;

        VkDescriptorSetLayoutCreateInfo layoutCI;
        layoutCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutCI.pNext = bindingFlags != nullptr ? &bindingFlagsInfo : nullptr;
        layoutCI.flags = m_UpdateAfterBind ? VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT : 0u;
        layoutCI.pBindings = bindings;
        layoutCI.bindingCount = numBindings;

        VkResult result = vkCreateDescriptorSetLayout(device.GetLogicalDevice(), &layoutCI, gVulkanAllocator, &m_Layout);
        CW_ENGINE_ASSERT(result == VK_SUCCESS);
    }

    VulkanDescriptorLayout::~VulkanDescriptorLayout() { vkDestroyDescriptorSetLayout(m_Device.GetLogicalDevice(), m_Layout, gVulkanAllocator); }

} // namespace Crowny
