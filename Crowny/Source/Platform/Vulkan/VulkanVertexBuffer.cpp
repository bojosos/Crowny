#include "cwpch.h"

#include "Crowny/Common/Constants.h"
#include "Platform/Vulkan/VulkanRenderAPI.h"
#include "Platform/Vulkan/VulkanVertexBuffer.h"

namespace Crowny
{
    namespace
    {
        uint32_t ResolveVertexLocation(const BufferElement& shaderElement, uint32_t fallback)
        {
            if (shaderElement.Location != UINT32_MAX)
                return shaderElement.Location;

            // Older shader assets omit explicit locations. Keep the standard semantic gaps instead of
            // compacting locations by the number of attributes provided by a particular mesh.
            if (shaderElement.Attribute == VertexAttribute::TexCoord0)
                return 4;
            if (shaderElement.Attribute == VertexAttribute::Color)
                return 5;
            return fallback;
        }

        bool IsVertexTypeCompatible(ShaderDataType meshType, ShaderDataType shaderType)
        {
            if (meshType == ShaderDataType::None || meshType == ShaderDataType::Mat3 || meshType == ShaderDataType::Mat4 ||
                shaderType == ShaderDataType::None || shaderType == ShaderDataType::Mat3 || shaderType == ShaderDataType::Mat4)
                return false;

            return meshType == shaderType || (meshType == ShaderDataType::Color && shaderType == ShaderDataType::Float4);
        }
    } // namespace

    VulkanBufferLayout::VulkanBufferLayout(uint32_t id, Vector<VkVertexInputAttributeDescription> attributes,
                                           Vector<VkVertexInputBindingDescription> bindings, uint32_t fallbackColorBinding)
      : m_Id(id), m_Attributes(std::move(attributes)), m_Bindings(std::move(bindings)), m_FallbackColorBinding(fallbackColorBinding)
    {
        m_CreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        m_CreateInfo.pVertexBindingDescriptions = m_Bindings.data();
        m_CreateInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(m_Bindings.size());
        m_CreateInfo.pVertexAttributeDescriptions = m_Attributes.data();
        m_CreateInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(m_Attributes.size());
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

    VulkanBufferLayoutManager::VulkanBufferLayoutManager(const VkPhysicalDeviceLimits& limits) : VulkanBufferLayoutManager()
    {
        ApplyLimits(limits);
    }

    void VulkanBufferLayoutManager::OnStartUp()
    {
        ApplyLimits(gVulkanRenderAPI().GetPresentDevice()->GetDeviceProperties().limits);

        const glm::vec4 white(1.0f);
        VertexBufferDesc desc;
        desc.Size = sizeof(white);
        desc.Usage = BufferUsage::BU_STATIC_DRAW;
        desc.Data = &white;
        m_FallbackColorBuffer = StaticRefCast<VulkanVertexBuffer>(VertexBuffer::Create(desc));
        CW_ENGINE_ASSERT(m_FallbackColorBuffer != nullptr, "Failed to create the Vulkan fallback vertex-color buffer");
    }

    void VulkanBufferLayoutManager::ApplyLimits(const VkPhysicalDeviceLimits& limits)
    {
        m_MaxVertexInputAttributes = limits.maxVertexInputAttributes;
        m_MaxVertexInputBindings = limits.maxVertexInputBindings;
        m_MaxVertexInputAttributeOffset = limits.maxVertexInputAttributeOffset;
        m_MaxVertexInputBindingStride = limits.maxVertexInputBindingStride;
    }

    VulkanBufferLayoutManager::~VulkanBufferLayoutManager()
    {
        Lock lock(m_Mutex);
        m_BufferLayoutMap.clear();
        m_FallbackColorBuffer = nullptr;
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

    VulkanBufferLayoutManager::BufferLayoutMap::iterator VulkanBufferLayoutManager::AddNew(const Ref<BufferLayout>& meshLayout,
                                                                                           const Ref<BufferLayout>& shaderLayout)
    {
        const auto& meshElements = meshLayout->GetElements();
        const auto& shaderElements = shaderLayout->GetElements();

        const uint32_t bindingLimit = std::min(m_MaxVertexInputBindings, static_cast<uint32_t>(MAX_BOUND_VERTEX_BUFFERS));
        uint32_t numBindings = meshLayout->GetStreamCount();
        bool valid = numBindings <= bindingLimit && shaderElements.size() <= m_MaxVertexInputAttributes;
        if (numBindings > bindingLimit)
        {
            CW_ENGINE_ERROR("Vertex layout uses {} streams, exceeding the Vulkan binding limit of {}", numBindings,
                            bindingLimit);
            numBindings = bindingLimit;
        }
        if (shaderElements.size() > m_MaxVertexInputAttributes)
        {
            CW_ENGINE_ERROR("Vertex shader uses {} inputs, exceeding the Vulkan attribute limit of {}", shaderElements.size(),
                            m_MaxVertexInputAttributes);
        }

        BufferLayoutEntry newEntry;
        Vector<VkVertexInputBindingDescription> bindings(numBindings);
        for (uint32_t i = 0; i < numBindings; i++)
        {
            VkVertexInputBindingDescription& binding = bindings[i];
            binding.binding = i;
            binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
            binding.stride = meshLayout->GetStride(i);
            if (binding.stride > m_MaxVertexInputBindingStride)
            {
                CW_ENGINE_ERROR("Vertex stream {} has stride {}, exceeding the Vulkan limit of {}", i, binding.stride,
                                m_MaxVertexInputBindingStride);
                valid = false;
            }
        }

        Vector<VkVertexInputAttributeDescription> attributes;
        attributes.reserve(shaderElements.size());
        Vector<bool> bindingRateInitialized(numBindings, false);
        uint32_t fallbackColorBinding = UINT32_MAX;
        for (const BufferElement& shaderElement : shaderElements)
        {
            const auto meshElement = std::find_if(meshElements.begin(), meshElements.end(), [&](const BufferElement& candidate) {
                const bool semanticMatch = shaderElement.Attribute != VertexAttribute::None && candidate.Attribute == shaderElement.Attribute;
                const bool nameMatch = shaderElement.Attribute == VertexAttribute::None && !shaderElement.Name.empty() &&
                                       candidate.Name == shaderElement.Name;
                return semanticMatch || nameMatch;
            });

            const uint32_t location = ResolveVertexLocation(shaderElement, static_cast<uint32_t>(attributes.size()));
            if (location >= m_MaxVertexInputAttributes)
            {
                CW_ENGINE_ERROR("Vertex input '{}' uses location {}, exceeding the Vulkan limit of {}", shaderElement.Name, location,
                                m_MaxVertexInputAttributes - 1);
                valid = false;
                continue;
            }

            if (meshElement == meshElements.end())
            {
                if (shaderElement.Attribute != VertexAttribute::Color || shaderElement.Type != ShaderDataType::Float4 ||
                    numBindings >= bindingLimit)
                {
                    CW_ENGINE_ERROR("Vertex layout is missing required shader input '{}' at location {}", shaderElement.Name, location);
                    valid = false;
                    continue;
                }

                if (fallbackColorBinding == UINT32_MAX)
                {
                    fallbackColorBinding = numBindings++;
                    bindings.push_back({ fallbackColorBinding, 0u, VK_VERTEX_INPUT_RATE_VERTEX });
                }
                attributes.push_back({ location, fallbackColorBinding, VK_FORMAT_R32G32B32A32_SFLOAT, 0u });
                continue;
            }

            if (!IsVertexTypeCompatible(meshElement->Type, shaderElement.Type))
            {
                CW_ENGINE_ERROR("Vertex input '{}' at location {} has incompatible mesh type {} and shader type {}", shaderElement.Name,
                                location, static_cast<uint32_t>(meshElement->Type), static_cast<uint32_t>(shaderElement.Type));
                valid = false;
                continue;
            }

            if (meshElement->StreamIdx >= bindings.size())
            {
                CW_ENGINE_ERROR("Vertex input '{}' uses unavailable stream {}", shaderElement.Name, meshElement->StreamIdx);
                valid = false;
                continue;
            }
            if (meshElement->Offset > m_MaxVertexInputAttributeOffset)
            {
                CW_ENGINE_ERROR("Vertex input '{}' has offset {}, exceeding the Vulkan limit of {}", shaderElement.Name,
                                meshElement->Offset, m_MaxVertexInputAttributeOffset);
                valid = false;
                continue;
            }

            VkVertexInputAttributeDescription attr{};
            attr.location = location;
            attr.binding = meshElement->StreamIdx;
            attr.format = VulkanUtils::GetVertexFormat(meshElement->Type);
            attr.offset = meshElement->Offset;
            attributes.push_back(attr);

            VkVertexInputBindingDescription& binding = bindings[attr.binding];
            const bool isPerVertex = meshElement->InstanceRate == 0;
            if (!bindingRateInitialized[attr.binding])
            {
                binding.inputRate = isPerVertex ? VK_VERTEX_INPUT_RATE_VERTEX : VK_VERTEX_INPUT_RATE_INSTANCE;
                bindingRateInitialized[attr.binding] = true;
            }
            else
            {
                if ((binding.inputRate == VK_VERTEX_INPUT_RATE_VERTEX && !isPerVertex) ||
                    (binding.inputRate == VK_VERTEX_INPUT_RATE_INSTANCE && isPerVertex))
                {
                    CW_ENGINE_ERROR("Vertex stream {} mixes per-vertex and per-instance inputs", attr.binding);
                    valid = false;
                }
            }
        }

        BufferLayoutKey key;
        key.BufferId = meshLayout->GetId();
        key.ShaderId = shaderLayout->GetId();

        if (valid)
        {
            newEntry.BufferLayout =
              CreateRef<VulkanBufferLayout>(m_NextId++, std::move(attributes), std::move(bindings), fallbackColorBinding);
        }
        newEntry.LastUsedIdx = ++m_LastUsedCounter;
        return m_BufferLayoutMap.emplace(key, std::move(newEntry)).first;
    }

    void VulkanBufferLayoutManager::RemoveLeastUsed()
    {
        if (!m_WarningShown)
        {
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

    void VulkanVertexBuffer::CopyData(GpuBuffer& src, uint32_t srcOffset, uint32_t dstOffset, uint32_t length, bool discard,
                                      const Ref<CommandBuffer>& commandBuffer)
    {
        VulkanVertexBuffer& source = static_cast<VulkanVertexBuffer&>(src);
        m_Buffer->CopyData(*source.m_Buffer, srcOffset, dstOffset, length, discard, commandBuffer);
    }

    void* VulkanVertexBuffer::Map(uint32_t offset, uint32_t size, GpuLockOptions options) { return m_Buffer->Map(offset, size, options); }

    void VulkanVertexBuffer::Unmap() { m_Buffer->Unmap(); }

} // namespace Crowny
