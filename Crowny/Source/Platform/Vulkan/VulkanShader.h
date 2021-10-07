#pragma once

#include "Crowny/RenderAPI/Shader.h"
#include "Crowny/Utils/ShaderCompiler.h"

#include "Platform/Vulkan/VulkanResource.h"
#include "Platform/Vulkan/VulkanUtils.h"

namespace Crowny
{

    class VulkanShaderModule : public VulkanResource
    {
    public:
        VulkanShaderModule(VulkanResourceManager* owner, VkShaderModule module);
        ~VulkanShaderModule();

        VkShaderModule GetHandle() const { return m_Module; }

    private:
        VkShaderModule m_Module;
    };

    class VulkanShader : public ShaderStage
    {
    public:
        VulkanShader(const BinaryShaderData& shaderData);
        ~VulkanShader();

        virtual const Ref<UniformDesc>& GetUniformDesc() const override { return m_ShaderDesc.Description; }

        const VkPipelineShaderStageCreateInfo& GetShaderStage() const { return m_ShaderStage; }
        VulkanShaderModule* GetShaderModule() const { return m_Module; }

    private:
        BinaryShaderData m_ShaderDesc;
        VkPipelineShaderStageCreateInfo m_ShaderStage;
        VulkanShaderModule* m_Module;
    };
} // namespace Crowny
