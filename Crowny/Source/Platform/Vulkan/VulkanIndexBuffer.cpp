#include "cwpch.h"

#include "Platform/Vulkan/VulkanIndexBuffer.h"

namespace Crowny
{

    VulkanIndexBuffer::VulkanIndexBuffer(uint32_t count)
    {

    }

    VulkanIndexBuffer::VulkanIndexBuffer(uint32_t* indices, uint32_t count)
    {
        VkBufferUsageFlags usageFlags = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
        VkBufferCreateInfo m_BufferCreateInfo;
        m_BufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        m_BufferCreateInfo.pNext = nullptr;
        m_BufferCreateInfo.flags = 0;
        m_BufferCreateInfo.usage = usageFlags;
        m_BufferCreateInfo.queueFamilyIndexCount = 0;
        m_BufferCreateInfo.pQueueFamilyIndices = nullptr;
        m_BufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VkBuffer buffer;
        VkResult result = vkCreateBuffer(device, &m_BufferCreateInfo, nullptr, &buffer);
        CW_ENGINE_ASSERT(result == VK_SUCCESS);

    }

}