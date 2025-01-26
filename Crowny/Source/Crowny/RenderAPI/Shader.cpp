#include "cwpch.h"

#include "Crowny/RenderAPI/Shader.h"
#include "Crowny/Renderer/Renderer.h"

#include "Platform/OpenGL/OpenGLShader.h"
#include "Platform/Vulkan/VulkanShader.h"

namespace Crowny
{

    uint32_t BufferLayout::s_NextFreeId = 0;

    ShaderRenderPass::ShaderRenderPass(const ShaderRenderPassDesc& passDesc) : m_ShaderDesc(passDesc) {}

    Ref<ShaderRenderPass> ShaderRenderPass::Create(const ShaderRenderPassDesc& passDesc)
    {
        return CreateRef<ShaderRenderPass>(passDesc);
    }

    bool ShaderRenderPass::HasBlending() const
    {
        bool transparent = false;
        for (uint32_t i = 0; i < MAX_FRAMEBUFFER_COLOR_ATTACHMENTS; i++)
        {
            // TODO: Figure this out(the indices from the for loop, this has to have multiple blend states one for each color)
            if (m_ShaderDesc.BlendState->DstBlend != BlendFactor::Zero ||
                m_ShaderDesc.BlendState->SrcBlend == BlendFactor::DestColor ||
                m_ShaderDesc.BlendState->SrcBlend == BlendFactor::InvDestColor ||
                m_ShaderDesc.BlendState->SrcBlend == BlendFactor::DestAlpha ||
                m_ShaderDesc.BlendState->SrcBlend == BlendFactor::InvDestAlpha)
            {
                transparent = true;
            }
        }
        return transparent;
    }

    void ShaderRenderPass::Compile()
    {
        if (IsCompute())
        {
            const Ref<ShaderStage> shaderStage = ShaderStage::Create(m_ShaderDesc.ComputeShader);
            m_ComputePipeline = ComputePipeline::Create(shaderStage);
        }
        else
        {
            PipelineStateDesc pipelineDesc;
            if (m_ShaderDesc.VertexShader != nullptr)
                pipelineDesc.VertexShader = ShaderStage::Create(m_ShaderDesc.VertexShader);
            if (m_ShaderDesc.FragmentShader != nullptr)
                pipelineDesc.FragmentShader = ShaderStage::Create(m_ShaderDesc.FragmentShader);
            if (m_ShaderDesc.GeometryShader != nullptr)
                pipelineDesc.GeometryShader = ShaderStage::Create(m_ShaderDesc.GeometryShader);
            if (m_ShaderDesc.HullShader != nullptr)
                pipelineDesc.HullShader = ShaderStage::Create(m_ShaderDesc.HullShader);
            if (m_ShaderDesc.DomainShader != nullptr)
                pipelineDesc.DomainShader = ShaderStage::Create(m_ShaderDesc.DomainShader);
            // TODO: Requires both extending the shader lang adding the objects themselves.
            // pipelineDesc.BlendState = BlendState::Create(m_ShaderDesc.BlendState);
            m_GraphicsPipeline = GraphicsPipeline::Create(pipelineDesc);
        }
    }

    ShaderTechnique::ShaderTechnique(const Vector<String>& tags, const ShaderVariation& variation, const Vector<Ref<ShaderRenderPass>>& renderPasses) : m_Tags(tags), m_Variation(variation), m_Passes(renderPasses) {

    }

    Ref<ShaderTechnique> ShaderTechnique::Create(const Vector<String>& tags, const ShaderVariation& variation,
                                                 const Vector<Ref<ShaderRenderPass>>& renderPasses)
    {
        return CreateRef<ShaderTechnique>(tags, variation, renderPasses);
    }

    void ShaderTechnique::Compile()
    {
        for (const auto& pass : m_Passes)
            pass->Compile();
    }

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
        return;
    }

    Ref<Shader> ShaderLibrary::Load(const Path& filepath)
    {
        // auto shader = Shader::Create(filepath);
        // Add(shader);
        // return shader;
        return nullptr;
    }

    Ref<Shader> ShaderLibrary::Load(const String& name, const Path& filepath)
    {
        // auto shader = Shader::Create(filepath);
        // Add(name, shader);
        // return shader;
        return nullptr;
    }

    Ref<Shader> ShaderLibrary::Get(const String& name)
    {
        CW_ENGINE_ASSERT(Exists(name), "Shader not found!");
        return m_Shaders[name];
    }

    bool ShaderLibrary::Exists(const String& name) const { return m_Shaders.find(name) != m_Shaders.end(); }

    void ShaderDefines::Set(const String& name, int value) { m_Defines[name] = std::to_string(value); }
    void ShaderDefines::Set(const String& name, float value) { m_Defines[name] = std::to_string(value); }
    void ShaderDefines::Set(const String& name, const String& value) { m_Defines[name] = value; }
    const UnorderedMap<String, String>& ShaderDefines::Get() const { return m_Defines; }

    ShaderStage::ShaderStage(const Ref<BinaryShaderData>& shaderData) : m_ShaderData(shaderData) {}

    const Ref<UniformDesc>& ShaderStage::GetUniformDesc() const { return m_ShaderData->Description; }

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
        Ref<Shader> result = CreateRef<Shader>(); /*
        if (shaderDesc.VertexShader != nullptr)
            result->m_ShaderStages[VERTEX_SHADER] = ShaderStage::Create(shaderDesc.VertexShader);
        if (shaderDesc.FragmentShader != nullptr)
            result->m_ShaderStages[FRAGMENT_SHADER] = ShaderStage::Create(shaderDesc.FragmentShader);
        if (shaderDesc.DomainShader != nullptr)
            result->m_ShaderStages[DOMAIN_SHADER] = ShaderStage::Create(shaderDesc.DomainShader);
        if (shaderDesc.HullShader != nullptr)
            result->m_ShaderStages[HULL_SHADER] = ShaderStage::Create(shaderDesc.HullShader);
        if (shaderDesc.GeometryShader != nullptr)
            result->m_ShaderStages[GEOMETRY_SHADER] = ShaderStage::Create(shaderDesc.GeometryShader);
        if (shaderDesc.ComputeShader != nullptr)
            result->m_ShaderStages[COMPUTE_SHADER] = ShaderStage::Create(shaderDesc.ComputeShader);*/
        return result;
    }

} // namespace Crowny