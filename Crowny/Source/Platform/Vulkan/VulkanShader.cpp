#include "cwpch.h"

#include "Platform/Vulkan/VulkanShader.h"

#include "Platform/Vulkan/VulkanRenderAPI.h"

#include "Crowny/Common/VirtualFileSystem.h"

namespace Crowny
{

    VulkanShaderModule::VulkanShaderModule(VulkanResourceManager* owner, VkShaderModule module)
      : VulkanResource(owner, true), m_Module(module)
    {
    }

    VulkanShaderModule::~VulkanShaderModule()
    {
        vkDestroyShaderModule(m_Owner->GetDevice().GetLogicalDevice(), m_Module, gVulkanAllocator);
    }

    VulkanShader::VulkanShader(const BinaryShaderData& data) : m_ShaderDesc(data)
    {
        VulkanDevice& device = *gVulkanRenderAPI().GetPresentDevice().get();
        m_ShaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        m_ShaderStage.pNext = nullptr;
        m_ShaderStage.flags = 0;
        m_ShaderStage.stage = VulkanUtils::GetShaderFlags(data.Type);
        // m_ShaderStage.pName = data.EntryPoint.c_str();
        m_ShaderStage.pName = "main";
        m_ShaderStage.pSpecializationInfo = nullptr;

        VkShaderModuleCreateInfo moduleCreateInfo{};
        moduleCreateInfo.pNext = nullptr;
        moduleCreateInfo.flags = 0;
        moduleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        moduleCreateInfo.codeSize = data.Size;
        moduleCreateInfo.pCode = (uint32_t*)data.Data;
        vkCreateShaderModule(device.GetLogicalDevice(), &moduleCreateInfo, gVulkanAllocator, &m_ShaderStage.module);
        m_Module = device.GetResourceManager().Create<VulkanShaderModule>(m_ShaderStage.module);
    }

    VulkanShader::VulkanShader(const std::string& name, const std::string& vertSrc, const std::string& fragSrc) {}
    VulkanShader::VulkanShader(const std::string& filepath, ShaderType shaderType) {}

    VulkanShader::~VulkanShader() { m_Module->Destroy(); }

} // namespace Crowny
