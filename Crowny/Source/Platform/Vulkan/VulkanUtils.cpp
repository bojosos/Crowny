#include "cwpch.h"

#include "Platform/Vulkan/VulkanUtils.h"

namespace Crowny
{
    VkSampleCountFlagBits VulkanUtils::GetSampleFlags(uint32_t numSamples)
    {
        switch(numSamples)
        {
            case 0:
            case 1:  return VK_SAMPLE_COUNT_1_BIT;
            case 2:  return VK_SAMPLE_COUNT_2_BIT;
            case 4:  return VK_SAMPLE_COUNT_4_BIT;
            case 8:  return VK_SAMPLE_COUNT_8_BIT;
            case 16: return VK_SAMPLE_COUNT_16_BIT;
            case 32: return VK_SAMPLE_COUNT_32_BIT;
            case 64: return VK_SAMPLE_COUNT_64_BIT;
        }
        
        return VK_SAMPLE_COUNT_1_BIT;
    }

    VkPrimitiveTopology VulkanUtils::GetDrawFlags(DrawMode drawMode)
    {
        switch(drawMode)
        {
            case DrawMode::TRIANGLE_LIST:  return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            case DrawMode::TRIANGLE_STRIP: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
            case DrawMode::TRIANGLE_FAN:   return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN;
            case DrawMode::POINT_LIST:     return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
            case DrawMode::LINE_LIST:      return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
            case DrawMode::LINE_STRIP:     return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
        }
        
        return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    }
}