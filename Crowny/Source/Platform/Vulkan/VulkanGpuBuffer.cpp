#include "cwpch.h"

#include "Platform/Vulkan/VulkanGpuBuffer.h"

#include "Platform/Vulkan/VulkanRendererAPI.h"
#include "Platform/Vulkan/VulkanCommandBuffer.h"

namespace Crowny
{
    
    VulkanBuffer::VulkanBuffer(VulkanDevice& device, VkBuffer buffer, VmaAllocation allocation) : m_Device(device), m_Buffer(buffer), m_Allocation(allocation) {}
    
    VulkanBuffer::~VulkanBuffer() { /*TODO: free memory, destroy buffer*/ }        
    
    uint8_t* VulkanBuffer::Map(VkDeviceSize offset, VkDeviceSize length) const
    {
        VkDeviceMemory memory;
        VkDeviceSize memoryOffset;
        m_Device.GetAllocationInfo(m_Allocation, memory, memoryOffset);
        
        uint8_t* data;
        VkResult result = vkMapMemory(m_Device.GetLogicalDevice(), memory, memoryOffset + offset, length, 0, (void**)&data);
        CW_ENGINE_ASSERT(result == VK_SUCCESS);

        return data;
    }

    void VulkanBuffer::Unmap()
    {
        VkDeviceMemory memory;
        VkDeviceSize memoryOffset;
        m_Device.GetAllocationInfo(m_Allocation, memory, memoryOffset);
        vkUnmapMemory(m_Device.GetLogicalDevice(), memory);
    }

    void VulkanBuffer::Copy(VulkanCommandBuffer* cmdBuffer, VulkanBuffer* dest, VkDeviceSize srcOffset, VkDeviceSize dstOffset, VkDeviceSize length)
    {
        VkBufferCopy region;
        region.size = length;
        region.srcOffset = srcOffset;
        region.dstOffset = dstOffset;
        vkCmdCopyBuffer(cmdBuffer->GetHandle(), m_Buffer, dest->GetHandle(), 1, &region);
    }
    
    void VulkanBuffer::Update(VulkanCommandBuffer* buffer, uint8_t* data, VkDeviceSize offset, VkDeviceSize length)
    {
        vkCmdUpdateBuffer(buffer->GetHandle(), m_Buffer, offset, length, (uint32_t*)data);
    }
    
    VulkanGpuBuffer::VulkanGpuBuffer(BufferType type, BufferUsage usage, uint32_t size)
        : m_Buffer(nullptr), m_StagingBuffer(nullptr), m_StagingMemory(nullptr), m_MappedOffset(0), m_MappedSize(0),
          m_MappedLockOptions(GpuLockOptions::WRITE_ONLY), m_DirectlyMappable(usage == BufferUsage::DYNAMIC_DRAW),
        m_IsMapped(false), m_SupportsGpuWrites(false), m_Size(size)
    {
        auto& device = gVulkanRendererAPI().GetPresentDevice();
        
        VkBufferUsageFlags usageFlags = 0;
        switch(type)
        {
            case BUFFER_VERTEX:  usageFlags = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT; break;
            case BUFFER_INDEX:   usageFlags = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT; break;
            case BUFFER_UNIFORM: usageFlags = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT; break;
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
    
    VulkanBuffer* VulkanGpuBuffer::CreateBuffer(VulkanDevice& device, uint32_t size, bool staging, bool readable)
    {
        VkBufferUsageFlags usage = m_BufferCreateInfo.usage;
        if (staging)
        {
            m_BufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            //TODO: readable
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
        return new VulkanBuffer(device, buffer, allocation);
    }
    
    void* VulkanGpuBuffer::Map(uint32_t offset, uint32_t length, GpuLockOptions options)
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
        m_MappedSize = length;
        m_MappedLockOptions = options;
        
        VulkanTransferManager& vtm = VulkanTransferManager::Get();
        
        VkAccessFlags accessFlags;
        if (options == GpuLockOptions::READ_ONLY)
            accessFlags = VK_ACCESS_HOST_READ_BIT;
        else
            accessFlags = VK_ACCESS_HOST_WRITE_BIT;
        
        if (m_DirectlyMappable)
        {
            /*
            uint32_t useMask = m_Buffer->GetUseInfo(VulkanAccessFlag::Read | VulkanAccessFlag::Write);
            
            bool isUsedOnGpu = useMask != 0 || m_SupportsGpuWrites;
            if (!isUsedOnGpu)
            {
                if (m_Buffer->IsBound())
                {
                    VulkanBuffer* newBuffer = CreateBuffer(*gVulkanRendererAPI().GetPresentDevice().get(), m_Size, false, true);
                    
                    if (options != GpuLockOptions::WRITE_DISCARD)
                    {
                        uint8_t* src = m_Buffer->Map(offset, length);
                        uint8_t* dst = newBuffer->Map(offset, length);
                        
                        memcpy(dst, src, length);
                        m_Buffer->Unmap();
                        newBuffer->Unmap();
                    }
                    
                    delete m_Buffer;
                    m_Buffer = newBuffer;
                }
                
                return m_Buffer->Map(offset, length);
            }
            
            if (options == GpuLockOptions::WRITE_DISCARD)
            {
                delete m_Buffer;
                m_Buffer = CreateBuffer(*gVulkanRendererAPI().GetPresentDevice().get(), m_Size, false, true);
                return m_Buffer->Map(offset, length);
            }
            
            if (options == GpuLockOptions::READ_ONLY || options == GpuLockOptions::WRITE_ONLY)
            {
                VulkanTransferBuffer* transferCB = vtm.GetTransferBuffer(GRAPHICS_QUEUE, 0);
                
                if (options == GpuLockOptions::READ_ONLY)
                    useMask = m_Buffer->GetUseInfo(VulkanAccessFlag::Write);
                else
                    useMask = m_Buffer->GetUseInfo(VulkanAccessFlag::Read | VulkanAccessFlag::Write);
                
                transferCB->AppendMask(useMask);
                
                if (m_SupportsGpuWrites)
                {
                    transferCB->MemoryBarrier(m_Buffer->GetHandle(), VK_ACCESS_SHADER_WRITE_BIT, accessFlags,
                                              VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                              VK_PIPELINE_STAGE_HOST_BIT);
                }
                
                transferCB->Flush();
                return m_Buffer->Map(offset, length);
            }*/
        }
        
        bool needRead = options != GpuLockOptions::WRITE_DISCARD;
        
        if (!needRead && offset % 4 == 0 && length % 4 == 0 && length <= 65536)
        {
            m_StagingMemory = new uint8_t[length];
            return m_StagingMemory;
        }
        
        m_StagingBuffer = CreateBuffer(*gVulkanRendererAPI().GetPresentDevice().get(), length, true, needRead);
        
        if (needRead)
        {
            VulkanTransferBuffer* transferCB = vtm.GetTransferBuffer(GRAPHICS_QUEUE, 0);
            /*
            uint32_t writeUseMask = buffer->GetUseInfo(VulkanAccessFlag::Write);
            if (m_SupportsGpuWrites || writeUseMask != 0)
                transferCB->AppendMask(writeUseMask);
            */
            m_Buffer->Copy(transferCB->GetCB(), m_StagingBuffer, offset, 0, length);
            
            transferCB->MemoryBarrier(m_StagingBuffer->GetHandle(), VK_ACCESS_TRANSFER_WRITE_BIT, accessFlags, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT);
            
            transferCB->Flush(true);
            
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

            /* TODO: fix writing
            bool isWrite = m_MappedLockOptions != GpuLockOptions::READ_ONLY;
            
            if (isWrite)
            {*/
                VulkanTransferManager& vtm = VulkanTransferManager::Get();
                VulkanTransferBuffer* transferCB = vtm.GetTransferBuffer(GRAPHICS_QUEUE, 0);
               /* 
                uint32_t useMask = m_Buffer->GetUseInfo(VulkanAccessFlag::Read | VulkanAccessFlag::Write);
                bool isNormalWrite = false;
                if (useMask != 0)
                {
                    if (m_MappedLockOptions == GpuLockOptions::WRITE_DISCARD)
                    {
                        delete m_Buffer;
                        m_Buffer = CreateBuffer(gVulkanRendererAPI().GetPresentDevice(), m_Size, false, true);
                    }
                    else
                    {
                        transferCB->AppendMask(useMask);
                        isNormalWrite = true;
                    }
                }
                else
                    isNormalWrite = false;

                if (isNormalWrite)
                {
                    uint32_t useCount = m_Buffer->GetUseCount();
                    uint32_t boundCount = buffer->GetBoundCount();
                    bool isBoundWithoutUse = boundCount > useCount;
                    if (isBoundWithoutUse)
                    {
                        VulkanBuffer* newBuffer = CreateBuffer(gVulkanRendererAPI().GetPresentDevice(), m_Size, false, true);
                        if (m_MappedOffset > 0 || m_MappedSize != m_Size)
                        {
                            m_Buffer->Copy(transferCB->GetCB(), newBUffer, 0, 0, m_Size);
                            transferCB->GetCB()->RegisterBuffer(m_Buffer, BufferUseFlagBits::Transfer, VulkanAccessFlag::Read);
                        }
                        
                        delete m_Buffer;
                        m_Buffer = newBuffer;
                    }
                }
                */
                if (m_StagingBuffer != nullptr)
                    {
                    m_StagingBuffer->Copy(transferCB->GetCB(), m_Buffer, 0, m_MappedOffset, m_MappedSize);
                  //  transferCB->GetCB()->RegisterBuffer(m_StagingBuffer, BufferUseFlagBits::Transfer, VulkanAccessFlag::Read);
                    }
                else
                    m_Buffer->Update(transferCB->GetCB(), m_StagingMemory, m_MappedOffset, m_MappedSize);
                
                //transferCB->GetCB()->RegisterBuffer(m_Buffer, BufferUsageFLagBits::Transfer, VulkanAccessFlags::Write);
            //}
            
            if (m_StagingBuffer != nullptr)
            {
                delete m_StagingBuffer;
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

    VulkanGpuBuffer::~VulkanGpuBuffer()
    {
        delete m_Buffer;
    }

}