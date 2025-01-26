#include "cwpch.h"

#include "Platform/Vulkan/VulkanVertexBuffer.h"

namespace Crowny
{

    VulkanBufferLayout::VulkanBufferLayout(uint32_t id, const VkPipelineVertexInputStateCreateInfo& createInfo)
      : m_Id(id), m_CreateInfo(createInfo)
    {
    }

    size_t VulkanBufferLayoutManager::HashFunc::operator()(const BufferLayoutKey& key) const
    {
        size_t hash = 0;
        HashCombine(hash, key.BufferId, key.ShaderId);
        return hash;
    }

    bool VulkanBufferLayoutManager::EqualFunc::operator()(const BufferLayoutKey& a, const BufferLayoutKey& b) const
    {
        return a.BufferId == b.BufferId && a.ShaderId == b.ShaderId;
    }

    VulkanBufferLayoutManager::VulkanBufferLayoutManager()
    {
        Lock lock(m_Mutex);
        m_NextId = 1;
        m_WarningShown = false;
        m_LastUsedCounter = 0;
    }

    VulkanBufferLayoutManager::~VulkanBufferLayoutManager()
    {
        Lock lock(m_Mutex);
        m_BufferLayoutMap.clear();
    }

    Ref<VulkanBufferLayout> VulkanBufferLayoutManager::GetBufferLayout(const Ref<BufferLayout>& meshLayout, const Ref<BufferLayout>& shaderLayout)
    {
        Lock lock(m_Mutex);

        BufferLayoutKey key;
        key.BufferId = meshLayout->GetId();
        key.ShaderId = shaderLayout->GetId();

        auto iterFind = m_BufferLayoutMap.find(key);
        if (iterFind == m_BufferLayoutMap.end())
        {
            if (m_BufferLayoutMap.size() > DECL_CACHE_SIZE)
                RemoveLeastUsed();
            iterFind = AddNew(meshLayout, shaderLayout);
        }

        iterFind->second.LastUsedIdx = ++m_LastUsedCounter;
        return iterFind->second.BufferLayout;
    }

    VulkanBufferLayoutManager::BufferLayoutMap::iterator VulkanBufferLayoutManager::AddNew(
      const Ref<BufferLayout>& meshLayout, const Ref<BufferLayout>& shaderLayout)
    {
        const Vector<BufferElement>& meshElements = meshLayout->GetElements();
        const Vector<BufferElement>& shaderElements = shaderLayout->GetElements();

        uint32_t numAttrs = 0;
        uint32_t numBindings = 0;
        for (const BufferElement& meshElement : meshElements)
        {
            bool semantic = false;
            for (const BufferElement& shaderElement : shaderElements)
            {
                if (shaderElement.Attribute == meshElement.Attribute)
                {
                    semantic = true;
                    break;
                }
            }
            if (!semantic)
                continue;

            numAttrs++;
            numBindings = std::max(numBindings, meshElement.StreamIdx); // For now always 1, weee neeeed stream idx.
        }

        BufferLayoutEntry newEntry;
        newEntry.Attributes = new VkVertexInputAttributeDescription[numAttrs];
        newEntry.Bindings = new VkVertexInputBindingDescription[numBindings];
        CW_ENGINE_ASSERT(numBindings == 1, "Not supproted currently");
        for (uint32_t i = 0; i < numBindings; i++)
        {
            VkVertexInputBindingDescription& binding = newEntry.Bindings[i];
            binding.binding = i;
            binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
            binding.stride = meshElements[i].Size;
        }

        uint32_t attrIdx = 0;
        bool isFirst = true;
        for (const auto& meshElement : meshElements)
        {
            VkVertexInputAttributeDescription& attr = newEntry.Attributes[attrIdx];
            bool semantic = false;
            for (const auto& shaderElement : shaderElements)
            {
                if (meshElement.Attribute == shaderElement.Attribute)
                {
                    semantic = true;
                    attr.location = meshElement.Offset;
                    break;
                }
            }
            if (!semantic)
                continue;

            attr.binding = meshElement.StreamIdx;
            attr.format = VulkanUtils::GetVertexFormat(meshElement.Type);
            attr.offset = meshElement.Offset;

            VkVertexInputBindingDescription& binding = newEntry.Bindings[attr.binding];
            const bool isPerVertex = meshElement.InstanceRate == 0;
            if (isFirst)
            {
                binding.inputRate = isPerVertex ? VK_VERTEX_INPUT_RATE_VERTEX : VK_VERTEX_INPUT_RATE_INSTANCE;
                isFirst = false;
            }
            else
            {
                if (binding.inputRate == VK_VERTEX_INPUT_RATE_VERTEX && !isPerVertex ||
                    binding.inputRate == VK_VERTEX_INPUT_RATE_INSTANCE && isPerVertex)
                {
                    CW_ENGINE_WARN("This here bad");
                    CW_ENGINE_ASSERT(false);
                }
            }
            attrIdx++;
        }

        numAttrs = attrIdx;
        VkPipelineVertexInputStateCreateInfo vertexInputCI{};
        vertexInputCI.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputCI.pNext = nullptr;
        vertexInputCI.flags = 0;
        vertexInputCI.pVertexBindingDescriptions = newEntry.Bindings;
        vertexInputCI.vertexBindingDescriptionCount = numBindings;
        vertexInputCI.pVertexAttributeDescriptions = newEntry.Attributes;
        vertexInputCI.vertexAttributeDescriptionCount = numAttrs;

        BufferLayoutKey key;
        key.BufferId = meshLayout->GetId();
        key.ShaderId = shaderLayout->GetId();

        newEntry.BufferLayout = CreateRef<VulkanBufferLayout>(m_NextId++, vertexInputCI);
        newEntry.LastUsedIdx = ++m_LastUsedCounter;
        return m_BufferLayoutMap.emplace(key, std::move(newEntry)).first;
    }

    void VulkanBufferLayoutManager::RemoveLeastUsed()
    {
        if (!m_WarningShown) {
            CW_ENGINE_WARN("Pruning: {0}", NUM_MAX_PRUNE);
            m_WarningShown = true;
        }
        Map<uint32_t, BufferLayoutKey> leastUsedMap;
        for (const auto& [key, value] : m_BufferLayoutMap)
            leastUsedMap[value.LastUsedIdx] = key;
        uint32_t removed = 0;
        for (const auto& [key, value] : leastUsedMap)
        {
            const auto iterFind = m_BufferLayoutMap.find(value);
            m_BufferLayoutMap.erase(iterFind);
            removed++;
            if (removed >= NUM_MAX_PRUNE)
                break;
        }
    }


    VulkanVertexBuffer::VulkanVertexBuffer(uint32_t size, BufferUsage usage) : m_Usage(usage)
    {
        m_Buffer = new VulkanGpuBuffer(VulkanGpuBuffer::BUFFER_VERTEX, usage, size);
    }

    VulkanVertexBuffer::VulkanVertexBuffer(void* vertices, uint32_t size, BufferUsage usage) : m_Usage(usage)
    {
        m_Buffer = new VulkanGpuBuffer(VulkanGpuBuffer::BUFFER_VERTEX, usage, size);
        void* dest = m_Buffer->Map(0, size, GpuLockOptions::WRITE_DISCARD);
        memcpy(dest, vertices, size);
        m_Buffer->Unmap();
    }

    VulkanVertexBuffer::~VulkanVertexBuffer() { delete m_Buffer; }

    void* VulkanVertexBuffer::Map(uint32_t offset, uint32_t size, GpuLockOptions options)
    {
        return m_Buffer->Map(offset, size, options);
    }

    void VulkanVertexBuffer::Unmap() { m_Buffer->Unmap(); }

} // namespace Crowny