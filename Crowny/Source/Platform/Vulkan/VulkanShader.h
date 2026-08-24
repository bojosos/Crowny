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
        friend class ShaderStage;
        ~VulkanShader();

        VulkanShaderModule* GetShaderModule() const { return m_Module; }

    protected:
        VulkanShader(const Ref<BinaryShaderData>& shaderData);

    private:
        VkPipelineShaderStageCreateInfo m_ShaderStage;
        VulkanShaderModule* m_Module = nullptr;
    };
} // namespace Crowny
