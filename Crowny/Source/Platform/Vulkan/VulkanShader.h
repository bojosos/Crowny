#pragma once

#include "Crowny/Renderer/Shader.h"
#include "Crowny/Utils/ShaderCompiler.h"

#include "Platform/Vulkan/VulkanUtils.h"
#include "Platform/Vulkan/VulkanResource.h"

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

    class VulkanShader : public Shader
    {
    public:
        VulkanShader(const std::string& name, const std::string& vertSrc, const std::string& fragSrc);
        VulkanShader(const std::string& filepath, ShaderType shaderType);
        VulkanShader(const BinaryShaderData& shaderData);
        ~VulkanShader();

        //virtual const std::string& GetName() const override { return m_Name; };
        //virtual const std::string& GetFilepath() const override { return m_Filepath; }

        virtual const Ref<UniformDesc>& GetUniformDesc() const override { return m_ShaderDesc.Description; }

        const VkPipelineShaderStageCreateInfo& GetShaderStage() const { return m_ShaderStage; }
        VulkanShaderModule* GetShaderModule() const { return m_Module; }

    private:
        BinaryShaderData m_ShaderDesc;
        VkPipelineShaderStageCreateInfo m_ShaderStage;
        std::string m_Filepath;
        std::string m_Name;
        uint32_t m_RendererID;
        VulkanShaderModule* m_Module;
    };
}