#include "cwpch.h"

#include "Platform/Vulkan/VulkanShader.h"

#include "Platform/Vulkan/VulkanRendererAPI.h"

#include "Crowny/Common/VirtualFileSystem.h"

namespace Crowny
{

	VulkanShader::VulkanShader(const std::string& filepath, ShaderType shaderType)
    {
        m_Device = gVulkanRendererAPI().GetPresentDevice()->GetLogicalDevice();
        m_ShaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        if (shaderType == VERTEX_SHADER)
            m_ShaderStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
        if (shaderType == FRAGMENT_SHADER)
            m_ShaderStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        m_ShaderStage.pName = "main";
        m_ShaderStage.pSpecializationInfo = nullptr;
        m_ShaderStage.pNext = nullptr;
        m_ShaderStage.flags = 0;
        
        auto [data, size] = VirtualFileSystem::Get()->ReadFile(filepath);
        if (size > 0)
        {
            VkShaderModuleCreateInfo moduleCreateInfo{};
            moduleCreateInfo.pNext = nullptr;
            moduleCreateInfo.flags = 0;
            moduleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            moduleCreateInfo.codeSize = size;
            moduleCreateInfo.pCode = (uint32_t*)data; // ???? hmmm
            vkCreateShaderModule(m_Device, &moduleCreateInfo, gVulkanAllocator, &m_ShaderStage.module);
            delete[] data;
        }
    }
    
    VulkanShader::~VulkanShader()
    {
        vkDestroyShaderModule(m_Device, m_ShaderStage.module, gVulkanAllocator);
    }
    
}
