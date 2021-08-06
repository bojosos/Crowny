#include "cwpch.h"

#include "Crowny/RenderAPI/Shader.h"
#include "Crowny/Renderer/Renderer.h"

#include "Platform/OpenGL/OpenGLShader.h"
#include "Platform/Vulkan/VulkanShader.h"

namespace Crowny
{
    void ShaderLibrary::Add(const std::string& name, const Ref<Shader>& shader)
    {
        CW_ENGINE_ASSERT(!Exists(name), "Shader already exists!");
        m_Shaders[name] = shader;
    }

    void ShaderLibrary::Add(const Ref<Shader>& shader)
    {
        // auto& name = shader->GetName();
        // Add(name, shader);
        Add("test", shader);
    }

    Ref<Shader> ShaderLibrary::Load(const std::string& filepath)
    {
        auto shader = Shader::Create(filepath);
        Add(shader);
        return shader;
    }

    Ref<Shader> ShaderLibrary::Load(const std::string& name, const std::string& filepath)
    {
        auto shader = Shader::Create(filepath);
        Add(name, shader);
        return shader;
    }

    Ref<Shader> ShaderLibrary::Get(const std::string& name)
    {
        CW_ENGINE_ASSERT(Exists(name), "Shader not found!");
        return m_Shaders[name];
    }

    bool ShaderLibrary::Exists(const std::string& name) const { return m_Shaders.find(name) != m_Shaders.end(); }

    Ref<Shader> Shader::Create(const std::string& name, const std::string& vertSrc, const std::string& fragSrc)
    {
        switch (Renderer::GetAPI())
        {
        case RenderAPI::API::OpenGL:
            return CreateRef<OpenGLShader>(name, vertSrc, fragSrc);
        default:
            CW_ENGINE_ASSERT(false, "Renderer API not supporter");
            return nullptr;
        }

        return nullptr;
    }

    Ref<Shader> Shader::Create(const std::string& m_Filepath, ShaderType shaderType)
    {
        switch (Renderer::GetAPI())
        {
        case RenderAPI::API::OpenGL:
            return CreateRef<OpenGLShader>(m_Filepath);
        case RenderAPI::API::Vulkan:
            return CreateRef<VulkanShader>(m_Filepath, shaderType);
        default:
            CW_ENGINE_ASSERT(false, "Renderer API not supporter");
            return nullptr;
        }

        return nullptr;
    }

    Ref<Shader> Shader::Create(const BinaryShaderData& data)
    {
        switch (Renderer::GetAPI())
        {
        // TODO: Add support for binary OpenGL shaders
        // case RenderAPI::API::OpenGL: return CreateRef<OpenGLShader>(m_Filepath);
        case RenderAPI::API::Vulkan:
            return CreateRef<VulkanShader>(data);
        default:
            CW_ENGINE_ASSERT(false, "Renderer API not supporter");
            return nullptr;
        }

        return nullptr;
    }
} // namespace Crowny