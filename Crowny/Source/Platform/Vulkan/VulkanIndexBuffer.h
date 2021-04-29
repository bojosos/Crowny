#pragma once

#include "Platform/Vulkan/VulkanUtils.h"

#include "Crowny/Renderer/IndexBuffer.h"

namespace Crowny
{

    class VulkanIndexBuffer : public IndexBuffer
    {
    public:
        VulkanIndexBuffer(uint32_t count);
        VulkanIndexBuffer(uint32_t* indices, uint32_t count);
        VkBuffer GetHandle() const { return m_Buffer; }
    private:
        uint32_t m_Size;
        VkBuffer m_Buffer; 
    };

}