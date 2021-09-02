#include "cwpch.h"

#include "Platform/Vulkan/VulkanGpuBuffer.h"

#include "Platform/Vulkan/VulkanCommandBuffer.h"
#include "Platform/Vulkan/VulkanRenderAPI.h"
#include "Platform/Vulkan/VulkanTexture.h"

namespace Crowny
{

    VulkanBuffer::VulkanBuffer(VulkanResourceManager* owner, VkBuffer buffer, VmaAllocation allocation,
                               uint32_t rowPitch, uint32_t slicePitch)
      : VulkanResource(owner, false), m_Buffer(buffer), m_Allocation(allocation), m_RowPitch(rowPitch),
        m_SliceHeight(slicePitch)
    {
    }

    VulkanBuffer::~VulkanBuffer()
    {
        VulkanDevice& device = m_Owner->GetDevice();
        for (auto& view : m_Views)
            vkDestroyBufferView(device.GetLogicalDevice(), view.View, gVulkanAllocator);

        vkDestroyBuffer(device.GetLogicalDevice(), m_Buffer, gVulkanAllocator);
        device.FreeMemory(m_Allocation);
    }

    uint8_t* VulkanBuffer::Map(VkDeviceSize offset, VkDeviceSize length) const
    {
        VulkanDevice& device = m_Owner->GetDevice();
        VkDeviceMemory memory;
        VkDeviceSize memoryOffset;
        device.GetAllocationInfo(m_Allocation, memory, memoryOffset);

        uint8_t* data;
        VkResult result =
          vkMapMemory(device.GetLogicalDevice(), memory, memoryOffset + offset, length, 0, (void**)&data);
        CW_ENGINE_ASSERT(result == VK_SUCCESS);

        return data;
    }

    void VulkanBuffer::Unmap()
    {
        VulkanDevice& device = m_Owner->GetDevice();
        VkDeviceMemory memory;
        VkDeviceSize memoryOffset;
        device.GetAllocationInfo(m_Allocation, memory, memoryOffset);
        vkUnmapMemory(device.GetLogicalDevice(), memory);
    }

    void VulkanBuffer::Copy(VulkanCmdBuffer* cmdBuffer, VulkanBuffer* dest, VkDeviceSize srcOffset,
                            VkDeviceSize dstOffset, VkDeviceSize length)
    {
        VkBufferCopy region;
        region.size = length;
        region.srcOffset = srcOffset;
        region.dstOffset = dstOffset;
        vkCmdCopyBuffer(cmdBuffer->GetHandle(), m_Buffer, dest->GetHandle(), 1, &region);
    }

    void VulkanBuffer::Copy(VulkanCmdBuffer* cmdBuffer, VulkanImage* dest, const VkExtent3D& extent,
                            const VkImageSubresourceLayers& range, VkImageLayout layout)
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
        const auto iter =
          std::find_if(m_Views.begin(), m_Views.end(), [format](const ViewInfo& x) { return x.Format == format; });
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
        const auto iter =
          std::find_if(m_Views.begin(), m_Views.end(), [view](const ViewInfo& x) { return x.View == view; });
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
                iter - m_Views.erase(iter);
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
      : GpuBuffer(size, usage), m_Buffer(nullptr), m_StagingBuffer(nullptr), m_StagingMemory(nullptr),
        m_MappedOffset(0), m_MappedSize(0), m_MappedLockOptions(GpuLockOptions::WRITE_ONLY),
        m_DirectlyMappable(usage == BufferUsage::DYNAMIC_DRAW), m_IsMapped(false), m_SupportsGpuWrites(false)
    {
        auto& device = gVulkanRenderAPI().GetPresentDevice();

        VkBufferUsageFlags usageFlags = 0;
        switch (type)
        {
        case BUFFER_VERTEX:
            usageFlags = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
            break;
        case BUFFER_INDEX:
            usageFlags = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
            break;
        case BUFFER_UNIFORM:
            usageFlags = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
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
        m_Buffer = CreateBuffer(*device.get(), size, false, true);
    }

    VulkanGpuBuffer::~VulkanGpuBuffer() { m_Buffer->Destroy(); }

    VulkanBuffer* VulkanGpuBuffer::CreateBuffer(VulkanDevice& device, uint32_t size, bool staging, bool readable)
    {
        VkBufferUsageFlags usage = m_BufferCreateInfo.usage;
        if (staging)
        {
            m_BufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            // TODO: readable
        }
        // TODO: readable normal

        m_BufferCreateInfo.size = size;
        VkMemoryPropertyFlags flags;
        if (m_DirectlyMappable || staging)
            flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        else
            flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

        VkDevice vkDevice = device.GetLogicalDevice();
        VkBuffer buffer;
        VkResult result = vkCreateBuffer(vkDevice, &m_BufferCreateInfo, gVulkanAllocator, &buffer);
        CW_ENGINE_ASSERT(result == VK_SUCCESS);

        VmaAllocation allocation = device.AllocateMemory(buffer, flags);

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
        if (options == GpuLockOptions::READ_WRITE)
            accessFlags = VK_ACCESS_HOST_READ_BIT | VK_ACCESS_HOST_WRITE_BIT;
        else
            accessFlags = VK_ACCESS_HOST_WRITE_BIT;

        if (m_DirectlyMappable)
        {
            if (options == GpuLockOptions::WRITE_ONLY_NO_OVERWRITE)
                return m_Buffer->Map(offset, length);

            uint32_t useMask = m_Buffer->GetUseInfo(VulkanAccessFlagBits::Read | VulkanAccessFlagBits::Write);

            bool isUsedOnGpu = useMask != 0 || m_SupportsGpuWrites;
            if (!isUsedOnGpu)
            {
                if (m_Buffer->IsBound())
                {
                    VulkanBuffer* newBuffer =
                      CreateBuffer(*gVulkanRenderAPI().GetPresentDevice().get(), m_Size, false, true);

                    if (options != GpuLockOptions::WRITE_DISCARD)
                    {
                        uint8_t* src = m_Buffer->Map(offset, length);
                        uint8_t* dst = newBuffer->Map(offset, length);

                        std::memcpy(dst, src, length);
                        m_Buffer->Unmap();
                        newBuffer->Unmap();
                    }

                    m_Buffer->Destroy();
                    m_Buffer = newBuffer;
                }
                return m_Buffer->Map(offset, length);
            }

            if (options == GpuLockOptions::WRITE_DISCARD)
            {
                m_Buffer->Destroy();
                m_Buffer = CreateBuffer(*gVulkanRenderAPI().GetPresentDevice().get(), m_Size, false, true);
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
                                              VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
                                                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                              VK_PIPELINE_STAGE_HOST_BIT);
                }

                transferCB->Flush(true);
                if (options == GpuLockOptions::READ_WRITE && m_Buffer->IsBound())
                {
                    VulkanBuffer* newBuffer =
                      CreateBuffer(*gVulkanRenderAPI().GetPresentDevice().get(), m_Size, false, true);

                    uint8_t* src = m_Buffer->Map(offset, length);
                    uint8_t* dst = newBuffer->Map(offset, length);

                    std::memcpy(dst, src, length);
                    m_Buffer->Unmap();
                    newBuffer->Unmap();
                    m_Buffer->Destroy();
                    m_Buffer = newBuffer;
                }
                return m_Buffer->Map(offset, length);
            }
        }

        bool needRead = options != GpuLockOptions::WRITE_DISCARD && options != GpuLockOptions::WRITE_DISCARD_RANGE;

        if (!needRead && offset % 4 == 0 && length % 4 == 0 && length <= 65536)
        {
            m_StagingMemory = new uint8_t[length];
            return m_StagingMemory;
        }

        m_StagingBuffer = CreateBuffer(*gVulkanRenderAPI().GetPresentDevice().get(), length, true, needRead);

        if (needRead)
        {
            VulkanTransferBuffer* transferCB = vtm.GetTransferBuffer(queueType, localQueueIdx);

            uint32_t writeUseMask = m_Buffer->GetUseInfo(VulkanAccessFlagBits::Write);
            if (m_SupportsGpuWrites || writeUseMask != 0)
                transferCB->AppendMask(writeUseMask);

            m_Buffer->Copy(transferCB->GetCB(), m_StagingBuffer, offset, 0, length);

            transferCB->MemoryBarrier(m_StagingBuffer->GetHandle(), VK_ACCESS_TRANSFER_WRITE_BIT, accessFlags,
                                      VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT);

            transferCB->Flush(true);
            CW_ENGINE_ASSERT(!m_Buffer->IsUsed());
        }
        return m_StagingBuffer->Map(0, length);
    }

    void VulkanGpuBuffer::Unmap()
    {
        if (!m_IsMapped)
            return;

        VulkanDevice& device = *gVulkanRenderAPI().GetPresentDevice().get();

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
                uint32_t localQueueIdx = CommandSyncMask::GetQueueIdxAndType(m_MappedGlobalQueueIdx, queueType);

                VulkanTransferBuffer* transferCB = vtm.GetTransferBuffer(queueType, localQueueIdx);

                uint32_t useMask = m_Buffer->GetUseInfo(VulkanAccessFlagBits::Read | VulkanAccessFlagBits::Write);

                bool isNormalWrite = false;
                if (useMask != 0)
                {
                    if (m_MappedLockOptions == GpuLockOptions::WRITE_ONLY_NO_OVERWRITE)
                    {
                        // just go to copy();
                    }
                    else if (m_MappedLockOptions == GpuLockOptions::WRITE_DISCARD)
                    {
                        m_Buffer->Destroy();
                        m_Buffer = CreateBuffer(device, m_Size, false, true);
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
                        VulkanBuffer* newBuffer = CreateBuffer(device, m_Size, false, true);
                        if (m_MappedOffset > 0 || m_MappedSize != m_Size)
                        {
                            m_Buffer->Copy(transferCB->GetCB(), newBuffer, 0, 0, m_Size);
                            transferCB->GetCB()->RegisterBuffer(m_Buffer, BufferUseFlagBits::Transfer,
                                                                VulkanAccessFlagBits::Read);
                        }

                        m_Buffer->Destroy();
                        m_Buffer = newBuffer;
                    }
                }

                if (m_StagingBuffer != nullptr)
                {
                    m_StagingBuffer->Copy(transferCB->GetCB(), m_Buffer, 0, m_MappedOffset, m_MappedSize);
                    transferCB->GetCB()->RegisterBuffer(m_StagingBuffer, BufferUseFlagBits::Transfer,
                                                        VulkanAccessFlagBits::Read);
                }
                else
                    m_Buffer->Update(transferCB->GetCB(), m_StagingMemory, m_MappedOffset, m_MappedSize);

                transferCB->GetCB()->RegisterBuffer(m_Buffer, BufferUseFlagBits::Transfer, VulkanAccessFlagBits::Write);
                // flush done before automatically before command buffer sumbission
            }

            if (m_StagingBuffer != nullptr)
            {
                m_StagingBuffer->Destroy();
                m_StagingBuffer = nullptr;
            }

            if (m_StagingMemory != nullptr)
            {
                delete m_StagingMemory;
                m_StagingMemory = nullptr;
            }
        }

        m_IsMapped = false;
    }

    void VulkanGpuBuffer::CopyData(GpuBuffer& srcBuffer, uint32_t srcOffset, uint32_t dstOffset, uint32_t length,
                                   bool discard, const Ref<CommandBuffer>& commandBuffer)
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

        src->Copy(vkCmdBuffer, dst, srcOffset, dstOffset, length);

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