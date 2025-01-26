#include "cwpch.h"

#include "Platform/Vulkan/VulkanDevice.h"
#include "Platform/Vulkan/VulkanRenderAPI.h"
#include "Platform/Vulkan/VulkanShader.h"

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

    VulkanShader::VulkanShader(const Ref<BinaryShaderData>& data) : ShaderStage(data)
    {
        VulkanDevice& device = *gVulkanRenderAPI().GetPresentDevice();
        m_ShaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        m_ShaderStage.pNext = nullptr;
        m_ShaderStage.flags = 0;
        m_ShaderStage.stage = VulkanUtils::GetShaderFlags(m_ShaderData->Type);
        // m_ShaderStage.pName = data.EntryPoint.c_str();
        m_ShaderStage.pName = "main";
        m_ShaderStage.pSpecializationInfo = nullptr;
        m_ShaderStage.module = VK_NULL_HANDLE;

        VkShaderModuleCreateInfo moduleCreateInfo{};
        moduleCreateInfo.pNext = nullptr;
        moduleCreateInfo.flags = 0;
        moduleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        moduleCreateInfo.codeSize = m_ShaderData->Data.size();
        moduleCreateInfo.pCode = (uint32_t*)m_ShaderData->Data.data();
        vkCreateShaderModule(device.GetLogicalDevice(), &moduleCreateInfo, gVulkanAllocator, &m_ShaderStage.module);
        m_Module = device.GetResourceManager().Create<VulkanShaderModule>(m_ShaderStage.module

        if (m_ShaderData->Type==ShaderType::VERTEX_SHADER)
            m_BufferLayout = std::move(m_ShaderData->VertexLayout);
    }

    VulkanShader::~VulkanShader() { m_Module->Destroy(); }

} // namespace Crowny
