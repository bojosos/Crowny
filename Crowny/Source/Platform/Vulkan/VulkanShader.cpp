#include "cwpch.h"

#include "Platform/Vulkan/VulkanShader.h"

namespace Crowny
{

	VulkanShader::VulkanShader(const std::string& filepath)
    {
        m_ShaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        m_ShaderStage.stage = stage;
        m_ShaderStage.pName = "main";
        
        auto [data, size] = VirtualFileSystem::ReadFile(filepath);
        if (size > 0)
        {
            VkShaderModuleCreateInfo moduleCreateInfo{};
            moduleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            moduleCreateInfo.codeSize = size;
            moduleCreateInfo.pCode = (uint32_t*)shaderCode; // ???? hmmm
            vkCreateShaderModule(device, &moduleCreateInfo, nullptr, &m_ShaderStage.module);
            delete[] data;
        }
    }
    
    VulkanShader::~VulkanShader
    {
        vkDestroyShaderModule(m_Deivce, )
    }

    VkPipelineShaderStageCreateInfo VulkanShader::GetStageCreateInfo()
    {
        return m_ShaderStage;
    }
}
