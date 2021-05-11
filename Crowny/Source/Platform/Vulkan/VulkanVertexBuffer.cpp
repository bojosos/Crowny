#include "cwpch.h"

#include "Platform/Vulkan/VulkanVertexBuffer.h"

namespace Crowny
{
    
    VulkanVertexBuffer::VulkanVertexBuffer(void* data, uint32_t size, const VertexBufferProperties& props)
    {
        CW_ENGINE_INFO("test");
        /*
        m_Device = gVulkanRendererAPI().GetPresentDevice()->GetLogicalDevice();
        VkBufferCreateInfo bufferCreateInfo{};
        bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        bufferCreateInfo.size = size;
        bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        VkResult result = vkCreateBuffer(m_Device, &bufferCreateInfo, nullptr, &m_Buffer);

        VkMemoryRequirements memReqs;
        VkMemoryAllocateInfo memAlloc{};
        memAlloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        vkGetBufferMemoryRequirements(m_Device, m_Buffer, &memReqs);
        memAlloc.allocationSize = memReqs.size;
        memAlloc.memoryTypeIndex = m_Device.GetMemoryType(memReqs.memoryTypeBits, memoryPropertyFlags);
        result = vkAllocateMemory(m_Device, &memAlloc, nullptr, &m_Memory);
        CW_ENGINE_ASSERT(result == VK_SUCCESS);
        
        if (data != nullptr)
        {
            void* mapped;
            result = vkMapMemory(m_Device, m_Memory, 0, size, 0, &mapped);
            memcpy(mapped, data, size);
            vkUnmapMemory(m_Device, m_Memory);
        }
        result = vkBindBufferMemory(m_Device, m_Buffer, m_Memory, 0);
        CW_ENGINE_ASSERT(result == VK_SUCCESS);*/
    }
    
    void* VulkanVertexBuffer::GetPointer(uint32_t size) const
    {
        void* data;
        VkResult result = vkMapMemory(m_Device, m_Memory, 0, size, 0, &data);
        CW_ENGINE_ASSERT(result == VK_SUCCESS);
        return data;
    }
    
    void VulkanVertexBuffer::FreePointer() const
    {
        vkUnmapMemory(m_Device, m_Memory);
    }

}