#include "cwpch.h"

#include "Crowny/RenderAPI/Shader.h"
#include "Crowny/Renderer/Renderer.h"

#include "Platform/OpenGL/OpenGLShader.h"
#include "Platform/Vulkan/VulkanShader.h"

namespace Crowny
{
    void ShaderLibrary::Add(const String& name, const Ref<Shader>& shader)
    {
        // CW_ENGINE_ASSERT(!Exists(name), "Shader already exists!");
        // m_Shaders[name] = shader;
    }

    void ShaderLibrary::Add(const Ref<Shader>& shader)
    {
        // auto& name = shader->GetName();
        // Add(name, shader);
        // Add("test", shader);
    }

    Ref<Shader> ShaderLibrary::Load(const Path& filepath)
    {
        // auto shader = Shader::Create(filepath);
        // Add(shader);
        // return shader;
    }

    Ref<Shader> ShaderLibrary::Load(const String& name, const Path& filepath)
    {
        // auto shader = Shader::Create(filepath);
        // Add(name, shader);
        // return shader;
    }

    Ref<Shader> ShaderLibrary::Get(const String& name)
    {
        CW_ENGINE_ASSERT(Exists(name), "Shader not found!");
        return m_Shaders[name];
    }

    bool ShaderLibrary::Exists(const String& name) const { return m_Shaders.find(name) != m_Shaders.end(); }

    Ref<ShaderStage> ShaderStage::Create(const Ref<BinaryShaderData>& data)
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

    Ref<Shader> Shader::Create(const ShaderDesc& shaderDesc)
    {
        Ref<Shader> result = CreateRef<Shader>();
        if (shaderDesc.VertexShader)
            result->m_ShaderStages[VERTEX_SHADER] = ShaderStage::Create(shaderDesc.VertexShader);
        if (shaderDesc.FragmentShader)
            ;
        result->m_ShaderStages[FRAGMENT_SHADER] = ShaderStage::Create(shaderDesc.FragmentShader);
        if (shaderDesc.DomainShader)
            result->m_ShaderStages[DOMAIN_SHADER] = ShaderStage::Create(shaderDesc.DomainShader);
        if (shaderDesc.HullShader)
            result->m_ShaderStages[HULL_SHADER] = ShaderStage::Create(shaderDesc.HullShader);
        if (shaderDesc.GeometryShader)
            result->m_ShaderStages[GEOMETRY_SHADER] = ShaderStage::Create(shaderDesc.GeometryShader);
        if (shaderDesc.ComputeShader)
            result->m_ShaderStages[COMPUTE_SHADER] = ShaderStage::Create(shaderDesc.ComputeShader);
    }

} // namespace Crowny