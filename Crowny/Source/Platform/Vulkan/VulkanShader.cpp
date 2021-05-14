#include "cwpch.h"

#include "Platform/Vulkan/VulkanShader.h"

#include "Platform/Vulkan/VulkanRendererAPI.h"

#include "Crowny/Common/VirtualFileSystem.h"

namespace Crowny
{

	VulkanShader::VulkanShader(const BinaryShaderData& data)
    {
        m_Device = gVulkanRendererAPI().GetPresentDevice()->GetLogicalDevice();
        m_ShaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        m_ShaderStage.stage = VulkanUtils::GetShaderFlags(data.ShaderType);
        m_ShaderStage.pName = "main";
        m_ShaderStage.pSpecializationInfo = nullptr;
        m_ShaderStage.pNext = nullptr;
        m_ShaderStage.flags = 0;
        
        VkShaderModuleCreateInfo moduleCreateInfo{};
        moduleCreateInfo.pNext = nullptr;
        moduleCreateInfo.flags = 0;
        moduleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        moduleCreateInfo.codeSize = data.Size;
        moduleCreateInfo.pCode = (uint32_t*)data.Data;
        vkCreateShaderModule(m_Device, &moduleCreateInfo, gVulkanAllocator, &m_ShaderStage.module);
    }

    VulkanShader::VulkanShader(const std::string& name, const std::string& vertSrc, const std::string& fragSrc) {}
    VulkanShader::VulkanShader(const std::string& filepath, ShaderType shaderType) {}
    
    VulkanShader::~VulkanShader()
    {
        vkDestroyShaderModule(m_Device, m_ShaderStage.module, gVulkanAllocator);
    }
    
}
