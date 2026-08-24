#include "cwpch.h"

#include "Platform/Vulkan/VulkanDevice.h"
#include "Platform/Vulkan/VulkanRenderAPI.h"
#include "Platform/Vulkan/VulkanShader.h"

#include "Crowny/Common/VirtualFileSystem.h"

namespace Crowny
{

    VulkanShaderModule::VulkanShaderModule(VulkanResourceManager* owner, VkShaderModule module) : VulkanResource(owner, true), m_Module(module) {}

    VulkanShaderModule::~VulkanShaderModule() { vkDestroyShaderModule(m_Owner->GetDevice().GetLogicalDevice(), m_Module, gVulkanAllocator); }

    VulkanShader::VulkanShader(const Ref<BinaryShaderData>& data) : ShaderStage(data)
    {
        VulkanDevice& device = *gVulkanRenderAPI().GetPresentDevice();
        m_ShaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        m_ShaderStage.pNext = nullptr;
        m_ShaderStage.flags = 0;
        m_ShaderStage.stage = VulkanUtils::GetShaderFlags(m_ShaderData->Type);
        m_ShaderStage.pName = m_ShaderData->EntryPoint.c_str();
        m_ShaderStage.pSpecializationInfo = nullptr;
        m_ShaderStage.module = VK_NULL_HANDLE;

        VkShaderModuleCreateInfo moduleCreateInfo{};
        moduleCreateInfo.pNext = nullptr;
        moduleCreateInfo.flags = 0;
        moduleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        moduleCreateInfo.codeSize = m_ShaderData->Data.size();
        if (moduleCreateInfo.codeSize == 0 || moduleCreateInfo.codeSize % sizeof(uint32_t) != 0)
        {
            CW_ENGINE_ERROR("Cannot create a Vulkan shader module from invalid SPIR-V data.");
            return;
        }
        Vector<uint32_t> shaderWords(m_ShaderData->Data.size() / sizeof(uint32_t));
        std::memcpy(shaderWords.data(), m_ShaderData->Data.data(), m_ShaderData->Data.size());
        moduleCreateInfo.pCode = shaderWords.data();
        const VkResult result = vkCreateShaderModule(device.GetLogicalDevice(), &moduleCreateInfo, gVulkanAllocator, &m_ShaderStage.module);
        if (result != VK_SUCCESS)
        {
            CW_ENGINE_ERROR("Failed to create Vulkan shader module: {}", static_cast<int32_t>(result));
            return;
        }
        m_Module = device.GetResourceManager().Create<VulkanShaderModule>(m_ShaderStage.module);

        if (m_ShaderData->Type == ShaderType::VERTEX_SHADER)
            m_BufferLayout = CreateRef<BufferLayout>(m_ShaderData->VertexLayout);
    }

    VulkanShader::~VulkanShader()
    {
        if (m_Module != nullptr)
            m_Module->Destroy();
    }

} // namespace Crowny
