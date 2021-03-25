#pragma once

#include "Crowny/Renderer/Shader.h"

#include "Platform/Vulkan/VulkanUtils.h"

namespace Crowny
{

    class VulkanShader : public Shader
    {
    public:
        VulkanShader(const std::string& name, const std::string& vertSrc, const std::string& fragSrc);
        VulkanShader(const std::string& filepath);
        ~VulkanShader();

        virtual void Reload() override;

        virtual void Bind() const override;
        virtual void Unbind() const override;

        virtual void SetVSSystemUniformBuffer(byte* data, uint32_t size, uint32_t slot) override;
        virtual void SetFSSystemUniformBuffer(byte* data, uint32_t size, uint32_t slot) override;

        virtual void SetVSUserUniformBuffer(byte* data, uint32_t size) override;
        virtual void SetFSUserUniformBuffer(byte* data, uint32_t size) override;

        virtual const std::string& GetName() const override { return m_Name; };
        virtual const std::string& GetFilepath() const override { return m_Filepath; }

        void Load(const std::string& path);
        std::unordered_map<uint32_t, std::string> ShaderPreProcess(const std::string& source);
        void Compile(const std::unordered_map<uint32_t, std::string>& shaderSources);
        void Parse(const std::string& vertSrc, const std::string& fragSrc);
        void ParseUniform(const std::string& statement, uint32_t shaderType);
        void ParseUniformStruct(const std::string& block, uint32_t shaderType);
        void ResolveUniforms();

        const VkPipelineShaderStageCreateInfo& GetShaderStage() const { return m_ShaderStage; }

    private:
        VkPipelineShaderStageCreateInfo m_ShaderStage;
        std::string m_Filepath;
        std::string m_Name;
        uint32_t m_RendererID;
    };
}