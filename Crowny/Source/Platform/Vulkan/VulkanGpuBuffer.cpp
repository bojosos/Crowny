#include "cwpch.h"

#include "Platform/Vulkan/VulkanGpuBuffer.h"

#include "Platform/Vulkan/VulkanCommandBuffer.h"
#include "Platform/Vulkan/VulkanRenderAPI.h"
#include "Platform/Vulkan/VulkanTexture.h"

namespace Crowny
{
    namespace
    {
        class VulkanBufferReadback final : public GpuBufferReadback
        {
        public:
            VulkanBufferReadback(VulkanBuffer* buffer, uint32_t size) : m_Buffer(buffer), m_Size(size) {}
            ~VulkanBufferReadback() override { m_Buffer->Destroy(); }

            VulkanBuffer* GetBuffer() const { return m_Buffer; }
            uint32_t GetSize() const { return m_Size; }
            bool IsIdle() const { return !m_Buffer->IsBound() && !m_Buffer->IsUsed(); }

            bool TryRead(void* destination, uint32_t length) override
            {
                if (!destination || length != m_Size || m_Buffer->IsBound() || m_Buffer->IsUsed())
                    return false;
                const uint8_t* data = m_Buffer->Map(0, m_Size);
                if (!data)
                    return false;
                std::memcpy(destination, data, m_Size);
                m_Buffer->Unmap();
                return true;
            }

        private:
            VulkanBuffer* m_Buffer;
            uint32_t m_Size;
        };
    } // namespace

    Ref<GpuBufferReadback> VulkanGpuBuffer::QueueReadback(uint32_t offset, uint32_t length)
    {
        if (length == 0 || offset > m_Size || length > m_Size - offset)
            return nullptr;
        auto* command = gVulkanRenderAPI().GetMainCommandBuffer()->GetInternal();
        if (command->IsInRenderPass())
            command->EndRenderPass();
        // Only recycle requests with no external owner and no recorded or
        // submitted copy. Overflow requests remain independently owned.
        constexpr size_t maxCachedReadbacks = 4;
        size_t replace = m_Readbacks.size();
        Ref<GpuBufferReadback> result;
        for (size_t index = 0; index < m_Readbacks.size(); ++index)
        {
            auto* request = static_cast<VulkanBufferReadback*>(m_Readbacks[index].get());
            if (request->GetRefCount() != 1 || !request->IsIdle())
                continue;
            if (request->GetSize() == length)
            {
                result = m_Readbacks[index];
                break;
            }
            replace = index;
        }
        if (!result)
        {
            result = CreateRef<VulkanBufferReadback>(CreateBuffer(*gVulkanRenderAPI().GetPresentDevice(), length, true, true), length);
            if (replace < m_Readbacks.size())
                m_Readbacks[replace] = result;
            else if (m_Readbacks.size() < maxCachedReadbacks)
                m_Readbacks.push_back(result);
        }
        VulkanBuffer* staging = static_cast<VulkanBufferReadback*>(result.get())->GetBuffer();
        command->MemoryBarrier(m_Buffer->GetHandle(), VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                               VK_PIPELINE_STAGE_TRANSFER_BIT);
        m_Buffer->Copy(command, staging, offset, 0, length);
        command->MemoryBarrier(staging->GetHandle(), VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                               VK_PIPELINE_STAGE_HOST_BIT);
        command->MemoryBarrier(m_Buffer->GetHandle(), VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_MEMORY_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                               VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
        command->RegisterBuffer(m_Buffer, BufferUseFlagBits::Transfer, VulkanAccessFlagBits::Read);
        command->RegisterBuffer(staging, BufferUseFlagBits::Transfer, VulkanAccessFlagBits::Write);
        return result;
    }

    VulkanBuffer::VulkanBuffer(VulkanResourceManager* owner, VkBuffer buffer, VmaAllocation allocation, uint32_t rowPitch, uint32_t slicePitch)
      : VulkanResource(owner, false), m_Buffer(buffer), m_Allocation(allocation), m_RowPitch(rowPitch)
    {
        if (rowPitch != 0)
            m_SliceHeight = slicePitch / rowPitch;
        else
            m_SliceHeight = 0;
    }

    VulkanBuffer::~VulkanBuffer()
    {
        VulkanDevice& device = m_Owner->GetDevice();
        for (auto& view : m_Views)
            vkDestroyBufferView(device.GetLogicalDevice(), view.View, gVulkanAllocator);

        vkDestroyBuffer(device.GetLogicalDevice(), m_Buffer, gVulkanAllocator);
        device.FreeMemory(m_Allocation);
    }

    VkDeviceAddress VulkanBuffer::GetDeviceAddress() const
    {
        VkBufferDeviceAddressInfo bufferDeviceAddressInfo = {};
        bufferDeviceAddressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        bufferDeviceAddressInfo.buffer = m_Buffer;
        return vkGetBufferDeviceAddress(m_Owner->GetDevice().GetLogicalDevice(), &bufferDeviceAddressInfo);
    }

    uint8_t* VulkanBuffer::Map(VkDeviceSize offset, CW_MAYBE_UNUSED VkDeviceSize length) const
    {
        VulkanDevice& device = m_Owner->GetDevice();
        uint8_t* data = static_cast<uint8_t*>(device.MapMemory(m_Allocation));
        return data != nullptr ? data + offset : nullptr;
    }

    void VulkanBuffer::Unmap() { m_Owner->GetDevice().UnmapMemory(m_Allocation); }

    void VulkanBuffer::Copy(VulkanCmdBuffer* cmdBuffer, VulkanBuffer* dest, VkDeviceSize srcOffset, VkDeviceSize dstOffset, VkDeviceSize length)
    {
        VkBufferCopy region;
        region.size = length;
        region.srcOffset = srcOffset;
        region.dstOffset = dstOffset;
        vkCmdCopyBuffer(cmdBuffer->GetHandle(), m_Buffer, dest->GetHandle(), 1, &region);
    }

    void VulkanBuffer::Copy(VulkanCmdBuffer* cmdBuffer, VulkanImage* dest, const VkExtent3D& extent, const VkImageSubresourceLayers& range,
                            VkImageLayout layout)
    {
        VkBufferImageCopy region;
        region.bufferRowLength = m_RowPitch;
        region.bufferImageHeight = m_SliceHeight;
        region.bufferOffset = 0;
        region.imageOffset.x = 0;
        region.imageOffset.y = 0;
        region.imageOffset.z = 0;
        region.imageExtent = extent;
        region.imageSubresource = range;

        vkCmdCopyBufferToImage(cmdBuffer->GetHandle(), m_Buffer, dest->GetHandle(), layout, 1, &region);
    }

    void VulkanBuffer::Update(VulkanCmdBuffer* buffer, uint8_t* data, VkDeviceSize offset, VkDeviceSize length)
    {
        vkCmdUpdateBuffer(buffer->GetHandle(), m_Buffer, offset, length, (uint32_t*)data);
    }

    void VulkanBuffer::NotifyDone(uint32_t globalQueueIdx, VulkanAccessFlags useFlags)
    {
        bool isLast = m_NumBoundHandles == 1;
        if (isLast)
            DestroyUnusedViews();
        VulkanResource::NotifyDone(globalQueueIdx, useFlags);
    }

    VkBufferView VulkanBuffer::GetView(VkFormat format)
    {
        const auto iter = std::find_if(m_Views.begin(), m_Views.end(), [format](const ViewInfo& x) { return x.Format == format; });
        if (iter != m_Views.end())
        {
            iter->UseCount++;
            return iter->View;
        }

        VkBufferViewCreateInfo viewCreateInfo;
        viewCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO;
        viewCreateInfo.pNext = nullptr;
        viewCreateInfo.flags = 0;
        viewCreateInfo.offset = 0;
        viewCreateInfo.range = VK_WHOLE_SIZE;
        viewCreateInfo.format = format;
        viewCreateInfo.buffer = m_Buffer;

        VkBufferView view;
        VkResult result = vkCreateBufferView(GetDevice().GetLogicalDevice(), &viewCreateInfo, gVulkanAllocator, &view);
        CW_ENGINE_ASSERT(result == VK_SUCCESS);
        m_Views.push_back(ViewInfo(format, view));
        return view;
    }

    void VulkanBuffer::FreeView(VkBufferView view)
    {
        const auto iter = std::find_if(m_Views.begin(), m_Views.end(), [view](const ViewInfo& x) { return x.View == view; });
        if (iter != m_Views.end())
        {
            CW_ENGINE_ASSERT(iter->UseCount > 0);
            iter->UseCount--;
        }
        else
            CW_ENGINE_ASSERT(false);
    }

    void VulkanBuffer::DestroyUnusedViews()
    {
        for (auto iter = m_Views.begin(); iter != m_Views.end();)
        {
            if (iter->UseCount == 0)
            {
                vkDestroyBufferView(GetDevice().GetLogicalDevice(), iter->View, gVulkanAllocator);
                iter = m_Views.erase(iter);
            }
            else
                iter++;
        }
    }

    void VulkanBuffer::NotifyUnbound()
    {
        bool isLast = m_NumBoundHandles == 1;
        if (isLast)
            DestroyUnusedViews();
        VulkanResource::NotifyUnbound();
    }

    VulkanGpuBuffer::VulkanGpuBuffer(BufferType type, BufferUsage usage, uint32_t size)
      : GpuBuffer(size, usage), m_Buffer(nullptr), m_StagingBuffer(nullptr), m_StagingMemory(nullptr), m_MappedOffset(0), m_MappedSize(0),
        m_MappedLockOptions(GpuLockOptions::WRITE_ONLY), m_DirectlyMappable(usage == BufferUsage::BU_DYNAMIC_DRAW), m_IsMapped(false),
        m_SupportsGpuWrites(type == BufferType::BUFFER_STRUCTURED || type == BufferType::BUFFER_INDIRECT ||
                            ((usage & BufferUsage::BU_LOADSTORE) == BufferUsage::BU_LOADSTORE))
    {
        const Ref<VulkanDevice>& device = gVulkanRenderAPI().GetPresentDevice();

        VkBufferUsageFlags usageFlags = 0;
        switch (type)
        {
        case BUFFER_VERTEX:
            usageFlags = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
            if ((usage & BufferUsage::BU_LOADSTORE) == BufferUsage::BU_LOADSTORE)
                usageFlags |= VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT;
            break;
        case BUFFER_INDEX:
            usageFlags = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;

            if ((usage & BufferUsage::BU_LOADSTORE) == BufferUsage::BU_LOADSTORE)
                usageFlags |= VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT;
            break;
        case BUFFER_UNIFORM:
            usageFlags = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
            break;
        case BUFFER_GENERIC:
            usageFlags = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT;

            if ((usage & BufferUsage::BU_LOADSTORE) == BufferUsage::BU_LOADSTORE)
                usageFlags |= VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT;
            break;
        case BUFFER_STRUCTURED:
            usageFlags = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
            break;
        case BUFFER_INDIRECT:
            usageFlags = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
            break;
        case BUFFER_RAYTRACING:
            usageFlags = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
            break;
        case BUFFER_SHADER_TABLE:
            usageFlags = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR;
            break;
        }

        m_BufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        m_BufferCreateInfo.pNext = nullptr;
        m_BufferCreateInfo.flags = 0;
        m_BufferCreateInfo.usage = usageFlags;
        m_BufferCreateInfo.size = size;
        m_BufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        m_BufferCreateInfo.queueFamilyIndexCount = 0;
        m_BufferCreateInfo.pQueueFamilyIndices = nullptr;
        m_Buffer = CreateBuffer(*device, size, false, true);
    }

    VulkanGpuBuffer::~VulkanGpuBuffer()
    {
        m_Buffer->Destroy();
        for (VulkanBuffer* buffer : m_WriteVersions)
            buffer->Destroy();
    }

    VulkanBuffer* VulkanGpuBuffer::AcquireWriteBuffer()
    {
        for (size_t index = 0; index < m_WriteVersions.size(); ++index)
        {
            VulkanBuffer* buffer = m_WriteVersions[index];
            if (buffer->IsBound() || buffer->IsUsed())
                continue;
            m_WriteVersions[index] = m_WriteVersions.back();
            m_WriteVersions.pop_back();
            return buffer;
        }
        return CreateBuffer(*gVulkanRenderAPI().GetPresentDevice(), m_Size, false, true);
    }

    void VulkanGpuBuffer::RetireWriteBuffer(VulkanBuffer* buffer)
    {
        // Dynamic buffers retain versions until all recorded and submitted uses
        // retire. Texel buffers keep their existing view ownership/destruction path.
        const VkBufferUsageFlags texelUsage = VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT;
        if (m_DirectlyMappable && (m_BufferCreateInfo.usage & texelUsage) == 0)
            m_WriteVersions.push_back(buffer);
        else
            buffer->Destroy();
    }

    VulkanBuffer* VulkanGpuBuffer::CreateBuffer(VulkanDevice& device, uint32_t size, bool staging, bool readable)
    {
        VkBufferUsageFlags usage = m_BufferCreateInfo.usage;
        if (staging)
        {
            // Staging buffers need SRC for upload and DST for read-back
            m_BufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        }
        else if (readable)
        {
            // GPU buffers that support read-back need TRANSFER_SRC in addition to their existing flags
            m_BufferCreateInfo.usage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        }

        m_BufferCreateInfo.size = size;
        VkMemoryPropertyFlags flags;
        if (m_DirectlyMappable || staging)
            flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        else
            flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

        VkDevice vkDevice = device.GetLogicalDevice();
        VkBuffer buffer;
        VkResult result = vkCreateBuffer(vkDevice, &m_BufferCreateInfo, gVulkanAllocator, &buffer);
        CW_ENGINE_ASSERT(result == VK_SUCCESS);

        const char* tag;
        if (staging)
            tag = "GpuBuffer/Staging";
        else
        {
            VkBufferUsageFlags u = m_BufferCreateInfo.usage;
            if (u & VK_BUFFER_USAGE_VERTEX_BUFFER_BIT)
                tag = "GpuBuffer/Vertex";
            else if (u & VK_BUFFER_USAGE_INDEX_BUFFER_BIT)
                tag = "GpuBuffer/Index";
            else if (u & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT)
                tag = "GpuBuffer/Uniform";
            else if (u & VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)
                tag = "GpuBuffer/Storage";
            else
                tag = "GpuBuffer";
        }
        VmaAllocation allocation = device.AllocateMemory(buffer, flags, tag, staging ? VulkanAllocationType::Staging : VulkanAllocationType::Default);

        m_BufferCreateInfo.usage = usage;
        return device.GetResourceManager().Create<VulkanBuffer>(buffer, allocation);
    }

    void* VulkanGpuBuffer::Map(uint32_t offset, uint32_t length, GpuLockOptions options, uint32_t queueIdx)
    {
        if ((offset + length) > m_Size)
        {
            CW_ENGINE_ERROR("Offset is larger than buffer size");
            return nullptr;
        }

        if (length == 0)
            return nullptr;
        if (m_Buffer == nullptr)
            return nullptr;

        m_IsMapped = true;
        m_MappedOffset = offset;
        m_MappedGlobalQueueIdx = queueIdx;
        m_MappedSize = length;
        m_MappedLockOptions = options;

        VulkanTransferManager& vtm = VulkanTransferManager::Get();
        GpuQueueType queueType;
        uint32_t localQueueIdx = CommandSyncMask::GetQueueIdxAndType(queueIdx, queueType);

        VkAccessFlags accessFlags;
        if (options == GpuLockOptions::READ_ONLY)
            accessFlags = VK_ACCESS_HOST_READ_BIT;
        else if (options == GpuLockOptions::READ_WRITE)
            accessFlags = VK_ACCESS_HOST_READ_BIT | VK_ACCESS_HOST_WRITE_BIT;
        else
            accessFlags = VK_ACCESS_HOST_WRITE_BIT;

        if (m_DirectlyMappable)
        {
            if (options == GpuLockOptions::WRITE_ONLY_NO_OVERWRITE)
                return m_Buffer->Map(offset, length);

            if (options == GpuLockOptions::WRITE_DISCARD && m_Buffer->GetBoundCount() > m_Buffer->GetUseCount())
            {
                // A queue wait cannot finish a command buffer that is still being
                // recorded. Preserve its bytes, including per-dispatch constants,
                // even when this allocation is also used by a submitted frame.
                VulkanBuffer* replacement = AcquireWriteBuffer();
                RetireWriteBuffer(m_Buffer);
                m_Buffer = replacement;
                return m_Buffer->Map(offset, length);
            }

            uint32_t useMask = m_Buffer->GetUseInfo(VulkanAccessFlagBits::Read | VulkanAccessFlagBits::Write);

            bool isUsedOnGpu = useMask != 0 || m_SupportsGpuWrites;
            if (!isUsedOnGpu)
            {
                if (m_Buffer->IsBound())
                {
                    VulkanBuffer* newBuffer = AcquireWriteBuffer();

                    if (options != GpuLockOptions::WRITE_DISCARD)
                    {
                        uint8_t* src = m_Buffer->Map(0, m_Size);
                        uint8_t* dst = newBuffer->Map(0, m_Size);

                        // A range update preserves every other byte in the allocation.
                        std::memcpy(dst, src, m_Size);
                        m_Buffer->Unmap();
                        newBuffer->Unmap();
                    }

                    RetireWriteBuffer(m_Buffer);
                    m_Buffer = newBuffer;
                }
                return m_Buffer->Map(offset, length);
            }

            if (options == GpuLockOptions::WRITE_DISCARD)
            {
                // Discard does not need the previous bytes. Rotate to an idle
                // version instead of synchronously waiting for submitted reads.
                if (m_Buffer->IsBound() || m_Buffer->IsUsed())
                {
                    VulkanBuffer* replacement = AcquireWriteBuffer();
                    RetireWriteBuffer(m_Buffer);
                    m_Buffer = replacement;
                }
                return m_Buffer->Map(offset, length);
            }

            if (options == GpuLockOptions::READ_ONLY || options == GpuLockOptions::WRITE_ONLY)
            {
                VulkanTransferBuffer* transferCB = vtm.GetTransferBuffer(queueType, localQueueIdx);

                if (options == GpuLockOptions::READ_ONLY)
                    useMask = m_Buffer->GetUseInfo(VulkanAccessFlagBits::Write);
                else
                    useMask = m_Buffer->GetUseInfo(VulkanAccessFlagBits::Read | VulkanAccessFlagBits::Write);

                transferCB->AppendMask(useMask);

                if (m_SupportsGpuWrites)
                {
                    transferCB->MemoryBarrier(m_Buffer->GetHandle(), VK_ACCESS_SHADER_WRITE_BIT, accessFlags,
                                              VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                              VK_PIPELINE_STAGE_HOST_BIT);
                }

                transferCB->Flush(true);
                if (options == GpuLockOptions::READ_WRITE && m_Buffer->IsBound())
                {
                    VulkanBuffer* newBuffer = AcquireWriteBuffer();

                    uint8_t* src = m_Buffer->Map(offset, length);
                    uint8_t* dst = newBuffer->Map(offset, length);

                    std::memcpy(dst, src, length);
                    m_Buffer->Unmap();
                    newBuffer->Unmap();
                    RetireWriteBuffer(m_Buffer);
                    m_Buffer = newBuffer;
                }
                return m_Buffer->Map(offset, length);
            }
        }

        const bool needRead = options != GpuLockOptions::WRITE_DISCARD && options != GpuLockOptions::WRITE_DISCARD_RANGE;

        if (!needRead && offset % 4 == 0 && length % 4 == 0 && length <= 65536)
        {
            m_StagingMemory = new uint8_t[length];
            return m_StagingMemory;
        }

        m_StagingBuffer = CreateBuffer(*gVulkanRenderAPI().GetPresentDevice().get(), length, true, needRead);

        if (needRead)
        {
            VulkanTransferBuffer* transferCB = vtm.GetTransferBuffer(queueType, localQueueIdx);

            const uint32_t writeUseMask = m_Buffer->GetUseInfo(VulkanAccessFlagBits::Write);
            if (m_SupportsGpuWrites || writeUseMask != 0)
                transferCB->AppendMask(writeUseMask);

            m_Buffer->Copy(transferCB->GetCB(), m_StagingBuffer, offset, 0, length);

            transferCB->MemoryBarrier(m_StagingBuffer->GetHandle(), VK_ACCESS_TRANSFER_WRITE_BIT, accessFlags, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                      VK_PIPELINE_STAGE_HOST_BIT);

            transferCB->Flush(true);
            CW_ENGINE_ASSERT(!m_Buffer->IsUsed());
        }
        return m_StagingBuffer->Map(0, length);
    }

    void VulkanGpuBuffer::Unmap()
    {
        if (!m_IsMapped)
            return;

        if (m_StagingMemory == nullptr && m_StagingBuffer == nullptr) // directly mapped
            m_Buffer->Unmap();
        else // we are using staging buffer/memory
        {
            if (m_StagingBuffer != nullptr)
                m_StagingBuffer->Unmap();

            bool isWrite = m_MappedLockOptions != GpuLockOptions::READ_ONLY;

            if (isWrite)
            {
                VulkanTransferManager& vtm = VulkanTransferManager::Get();
                GpuQueueType queueType;
                const uint32_t localQueueIdx = CommandSyncMask::GetQueueIdxAndType(m_MappedGlobalQueueIdx, queueType);

                VulkanTransferBuffer* transferCB = vtm.GetTransferBuffer(queueType, localQueueIdx);

                const uint32_t useMask = m_Buffer->GetUseInfo(VulkanAccessFlagBits::Read | VulkanAccessFlagBits::Write);

                bool isNormalWrite = false;
                if (useMask != 0)
                {
                    if (m_MappedLockOptions == GpuLockOptions::WRITE_ONLY_NO_OVERWRITE)
                    {
                        // just go to copy();
                    }
                    else if (m_MappedLockOptions == GpuLockOptions::WRITE_DISCARD)
                    {
                        VulkanBuffer* replacement = AcquireWriteBuffer();
                        RetireWriteBuffer(m_Buffer);
                        m_Buffer = replacement;
                    }
                    else // need to issue queue dependency
                    {
                        transferCB->AppendMask(useMask);
                        isNormalWrite = true;
                    }
                }
                else
                    isNormalWrite = true;

                if (isNormalWrite)
                {
                    uint32_t useCount = m_Buffer->GetUseCount();
                    uint32_t boundCount = m_Buffer->GetBoundCount();
                    bool isBoundWithoutUse = boundCount > useCount;
                    if (isBoundWithoutUse)
                    {
                        VulkanBuffer* newBuffer = AcquireWriteBuffer();
                        if (m_MappedOffset > 0 || m_MappedSize != m_Size)
                        {
                            transferCB->MemoryBarrier(m_Buffer->GetHandle(), VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
                                                      VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
                            m_Buffer->Copy(transferCB->GetCB(), newBuffer, 0, 0, m_Size);
                            transferCB->GetCB()->RegisterBuffer(m_Buffer, BufferUseFlagBits::Transfer, VulkanAccessFlagBits::Read);
                            transferCB->MemoryBarrier(newBuffer->GetHandle(), VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
                                                      VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
                        }

                        RetireWriteBuffer(m_Buffer);
                        m_Buffer = newBuffer;
                    }
                }

                if (m_StagingBuffer != nullptr)
                {
                    m_StagingBuffer->Copy(transferCB->GetCB(), m_Buffer, 0, m_MappedOffset, m_MappedSize);
                    transferCB->GetCB()->RegisterBuffer(m_StagingBuffer, BufferUseFlagBits::Transfer, VulkanAccessFlagBits::Read);
                }
                else
                    m_Buffer->Update(transferCB->GetCB(), m_StagingMemory, m_MappedOffset, m_MappedSize);

                transferCB->GetCB()->RegisterBuffer(m_Buffer, BufferUseFlagBits::Transfer, VulkanAccessFlagBits::Write);
                // flush done before automatically before command buffer submission
            }

            if (m_StagingBuffer != nullptr)
            {
                m_StagingBuffer->Destroy();
                m_StagingBuffer = nullptr;
            }

            if (m_StagingMemory != nullptr)
            {
                delete[] m_StagingMemory;
                m_StagingMemory = nullptr;
            }
        }

        m_IsMapped = false;
    }

    void VulkanGpuBuffer::CopyData(GpuBuffer& srcBuffer, uint32_t srcOffset, uint32_t dstOffset, uint32_t length, bool discard,
                                   const Ref<CommandBuffer>& commandBuffer)
    {
        CW_ENGINE_ASSERT(dstOffset + length <= m_Size);
        CW_ENGINE_ASSERT(srcOffset + length <= srcBuffer.GetSize());

        VulkanGpuBuffer& vkSrc = static_cast<VulkanGpuBuffer&>(srcBuffer);
        VulkanRenderAPI& rapi = gVulkanRenderAPI();
        VulkanCmdBuffer* vkCmdBuffer;
        if (commandBuffer == nullptr)
            vkCmdBuffer = rapi.GetMainCommandBuffer()->GetInternal();
        else
            vkCmdBuffer = static_cast<VulkanCommandBuffer*>(commandBuffer.get())->GetInternal();

        VulkanBuffer* src = vkSrc.GetBuffer();
        VulkanBuffer* dst = m_Buffer;

        if (src == nullptr || dst == nullptr)
            return;

        if (vkCmdBuffer->IsInRenderPass())
            vkCmdBuffer->EndRenderPass();

        // Transfer registrations are intentionally excluded from the command
        // buffer's shader hazard tracker. Make buffer copies self-synchronizing
        // so a copied geometry page can be consumed later in this command buffer.
        vkCmdBuffer->MemoryBarrier(src->GetHandle(), VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                                   VK_PIPELINE_STAGE_TRANSFER_BIT);
        vkCmdBuffer->MemoryBarrier(dst->GetHandle(), VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
                                   VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
        src->Copy(vkCmdBuffer, dst, srcOffset, dstOffset, length);
        vkCmdBuffer->MemoryBarrier(src->GetHandle(), VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_MEMORY_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                   VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
        vkCmdBuffer->MemoryBarrier(dst->GetHandle(), VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
                                   VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

        vkCmdBuffer->RegisterBuffer(src, BufferUseFlagBits::Transfer, VulkanAccessFlagBits::Read);
        vkCmdBuffer->RegisterBuffer(dst, BufferUseFlagBits::Transfer, VulkanAccessFlagBits::Write);
    }

    void VulkanGpuBuffer::WriteData(uint32_t offset, uint32_t length, const void* src, BufferWriteOptions writeFlags)
    {
        GpuLockOptions opts = GpuLockOptions::WRITE_DISCARD_RANGE;
        if (writeFlags == BWT_NO_OVERWRITE)
            opts = GpuLockOptions::WRITE_ONLY_NO_OVERWRITE;
        else if (writeFlags == BWT_DISCARD)
            opts = GpuLockOptions::WRITE_DISCARD;

        void* data = Lock(offset, length, opts);
        std::memcpy(data, src, length);
        Unlock();
    }

    void VulkanGpuBuffer::ReadData(uint32_t offset, uint32_t length, void* dest)
    {
        void* data = Lock(offset, length, GpuLockOptions::READ_ONLY);
        std::memcpy(dest, data, length);
        Unlock();
    }

} // namespace Crowny
