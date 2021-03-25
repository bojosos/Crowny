#include "cwpch.h"

#include "Platform/Vulkan/VulkanVertexBuffer.h"

namespace Crowny
{
    
    VulkanVertexBuffer::VulkanVertexBuffer(void* data, uint32_t size, const VertexBufferProperties& props)
    {
        VkVertexInputBindingDescription vertexInput{};
        vertexInput.binding = 0;
        vertexInput.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        vertexInput.stride = m_Layout.GetStride();
        
        std::vector<VkVertexInputAttributeDescription> attrs(m_Layout.GetElements().size());
        // loc, binding, format, offset

		for (int i = 0; i < m_Layout.GetElements().size(); i++)
		{
            const auto& element = m_Layout.GetElements().at(i);
			switch (element.Type)
			{
                case ShaderDataType::Float:  { attrs[i] = { i, 0, VK_FORMAT_R32_SFLOAT, element.Offset }; break; }
				case ShaderDataType::Float2: { attrs[i] = { i, 0, VK_FORMAT_R32G32_SFLOAT, element.Offset }; break; }
				case ShaderDataType::Float3: { attrs[i] = { i, 0, VK_FORMAT_R32G32B32_SFLOAT, element.Offset }; break; }
				case ShaderDataType::Float4: { attrs[i] = { i, 0, VK_FORMAT_R32G32B32A32_SFLOAT, element.Offset }; break; }
                case ShaderDataType::Bool:   // dk how to do this one
                case ShaderDataType::Mat3:   //attrs[i] = { i, 0, VK_FORMAT_R32_SFLOAT, element.Offset }; 
                case ShaderDataType::Mat4:   { CW_ENGINE_ASSERT(false); break; }// these will probably be sent as 3/4 vectors }
                case ShaderDataType::Int:    { attrs[i] = { i, 0, VK_FORMAT_R32_SINT, element.Offset }; break; }
				case ShaderDataType::Int2:   { attrs[i] = { i, 0, VK_FORMAT_R32G32_SINT, element.Offset }; break; }
				case ShaderDataType::Int3:   { attrs[i] = { i, 0, VK_FORMAT_R32G32B32_SINT, element.Offset }; break; }
				case ShaderDataType::Int4:   { attrs[i] = { i, 0, VK_FORMAT_R32G32B32A32_SINT, element.Offset }; break; }
				default:
					CW_ENGINE_ASSERT(false, "Unknown ShaderDataType!");
			}
		}

        VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo{};
        vertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputStateCreateInfo.vertexBindingDescriptionCount = 1;
        vertexInputStateCreateInfo.pVertexBindingDescriptions = &vertexInput;
        vertexInputStateCreateInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attrs.size());
        vertexInputStateCreateInfo.pVertexAttributeDescriptions = attrs.data();
        
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
        memAlloc.memoryTypeIndex = device.GetMemoryType(memReqs.memoryTypeBits, memoryProeprtyFlags);
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
        CW_ENGINE_ASSERT(result == VK_SUCCESS);
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