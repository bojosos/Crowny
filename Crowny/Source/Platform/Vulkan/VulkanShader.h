#pragma once

#include "Crowny/Renderer/Shader.h"

#include "Platform/Vulkan/VulkanUtils.h"
#include "Crowny/Utils/ShaderCompiler.h"

namespace Crowny
{

    class VulkanShader : public Shader
    {
    public:
        VulkanShader(const std::string& name, const std::string& vertSrc, const std::string& fragSrc);
        VulkanShader(const std::string& filepath, ShaderType shaderType);
        VulkanShader(const BinaryShaderData& shaderData);
        ~VulkanShader();

        virtual const std::string& GetName() const override { return m_Name; };
        virtual const std::string& GetFilepath() const override { return m_Filepath; }

        virtual const UniformDescription& GetUniformDesc() const override { return m_ShaderDesc.Description; }

        const VkPipelineShaderStageCreateInfo& GetShaderStage() const { return m_ShaderStage; }

    private:
        BinaryShaderData m_ShaderDesc;
        VkDevice m_Device;
        VkPipelineShaderStageCreateInfo m_ShaderStage;
        std::string m_Filepath;
        std::string m_Name;
        uint32_t m_RendererID;
    };
}