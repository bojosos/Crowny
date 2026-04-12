#include "cwpch.h"

#include "Crowny/RenderAPI/UniformParams.h"
#include "Crowny/Renderer/Material.h"

namespace Crowny
{

    Material::Material(const AssetHandle<Shader>& shader) : m_Shader(shader)
    {
        if (m_Shader)
            ReloadParams();
        else
            CW_ENGINE_ERROR("Material created with null shader handle");
    }

    Ref<Material> Material::Create(const AssetHandle<Shader>& shader) { return CreateRef<Material>(shader); }

    Ref<Material> Material::CreatePBR(const AssetHandle<Shader>& shader)
    {
        Ref<Material> material = Create(shader);
        material->SetTexture("albedoMap", Texture::WHITE);
        material->SetTexture("metallicMap", Texture::WHITE);
        material->SetTexture("roughnessMap", Texture::WHITE);
        material->SetTexture("normalMap", Texture::WHITE);
        material->SetTexture("aoMap", Texture::WHITE);
        material->SetColor("albedo", glm::vec4(1.0f));
        material->SetFloat("roughness", 0.5f);
        material->SetFloat("metalness", 0.0f);
        return material;
    }

    Ref<Material> Material::CreateToon(const AssetHandle<Shader>& shader)
    {
        Ref<Material> material = Create(shader);
        material->SetTexture("albedoMap", Texture::WHITE);
        material->SetColor("outlineColor", glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
        material->SetFloat("thickness", 1.0f);
        material->SetColor("tint", glm::vec4(1.0f));
        material->SetFloat("bands", 4.0f);
        material->SetFloat("specularSize", 0.5f);
        material->SetFloat("specularSmoothness", 1.0f);
        material->SetFloat("rimPower", 4.0f);
        material->SetFloat("rimThreshold", 0.1f);
        material->SetFloat("shadowBrightness", 0.2f);
        material->SetVector3("camPos", glm::vec3(0.0f));
        return material;
    }

    Ref<Material> Material::CreateUnlit(const AssetHandle<Shader>& shader)
    {
        Ref<Material> material = Create(shader);
        material->SetTexture("albedoMap", Texture::WHITE);
        material->SetColor("tint", glm::vec4(1.0f));
        return material;
    }

    void Material::SetShader(const AssetHandle<Shader>& shader)
    {
        m_Shader = shader;
        ReloadParams();
    }

    void Material::ReloadParams()
    {
        m_Passes.clear();
        m_Bindings.clear();

        const auto& renderPasses = m_Shader->GetTechniques()[0]->GetRenderPasses();
        m_Passes.resize(renderPasses.size());

        for (uint32_t p = 0; p < renderPasses.size(); p++)
        {
            renderPasses[p]->Compile();
            m_Passes[p].Pipeline = renderPasses[p]->GetGraphicsPipeline();
            CreateAndAppendUniforms(p);
        }
    }

    void Material::CreateAndAppendUniforms(uint32_t passIndex)
    {
        PassData& pass = m_Passes[passIndex];
        const Ref<UniformParamInfo>& uniformParamInfo = pass.Pipeline->GetParamInfo();
        pass.Uniforms = UniformParams::Create(pass.Pipeline);

        for (uint32_t i = 0; i < SHADER_COUNT; i++)
        {
            const Ref<UniformDesc>& paramDesc = uniformParamInfo->GetUniformDesc((ShaderType)i);
            if (!paramDesc)
                continue;
            for (const auto& [name, uniformBuffer] : paramDesc->Uniforms)
            {
                for (uint32_t j = 0; j < uniformBuffer.Members.size(); j++)
                {
                    // Unified bindings — if same name exists across passes, same meaning
                    m_Bindings[uniformBuffer.Members[j].Name] =
                      UniformMember{ uniformBuffer.Members[j].Offset, uniformBuffer.Members[j].DataType, name };
                }

                pass.UniformBlocks[name] = UniformBufferBlock::Create(uniformBuffer.BlockSize, BufferUsage::BU_DYNAMIC_DRAW);
                pass.Uniforms->SetUniformBlockBuffer(name, pass.UniformBlocks[name]);
            }
        }
    }

    void Material::FlushUniformBuffers()
    {
        for (auto& pass : m_Passes)
            for (const auto& [_, block] : pass.UniformBlocks)
                block->FlushToGpu();
    }

    void Material::SetFloat(const String& name, float value)
    {
        const auto& iterFind = m_Bindings.find(name);
        if (iterFind == m_Bindings.cend())
            return;
        if (iterFind->second.DataType != ShaderDataType::Float)
        {
            CW_ENGINE_WARN("Trying to write the wrong data type {}, expected {}, got float", name, ShaderDataTypeToString(iterFind->second.DataType));
            return;
        }
        for (auto& pass : m_Passes)
        {
            const auto blockIt = pass.UniformBlocks.find(iterFind->second.BufferName);
            if (blockIt != pass.UniformBlocks.end())
                blockIt->second->Write(iterFind->second.Offset, &value, sizeof(value));
        }
    }

    void Material::SetFloat2(const String& name, const glm::vec2& value)
    {
        const auto& iterFind = m_Bindings.find(name);
        if (iterFind == m_Bindings.cend())
            return;
        if (iterFind->second.DataType != ShaderDataType::Float2)
        {
            CW_ENGINE_WARN("Trying to write the wrong data type {}, expected {}, got float2", name,
                           ShaderDataTypeToString(iterFind->second.DataType));
            return;
        }
        for (auto& pass : m_Passes)
        {
            const auto blockIt = pass.UniformBlocks.find(iterFind->second.BufferName);
            if (blockIt != pass.UniformBlocks.end())
                blockIt->second->Write(iterFind->second.Offset, &value, sizeof(value));
        }
    }

    void Material::SetInt(const String& name, int value)
    {
        const auto& iterFind = m_Bindings.find(name);
        if (iterFind == m_Bindings.cend())
            return;
        if (iterFind->second.DataType != ShaderDataType::Int)
        {
            CW_ENGINE_WARN("Trying to write the wrong data type {}, expected {}, got int", value, ShaderDataTypeToString(iterFind->second.DataType));
            return;
        }
        for (auto& pass : m_Passes)
        {
            const auto blockIt = pass.UniformBlocks.find(iterFind->second.BufferName);
            if (blockIt != pass.UniformBlocks.end())
                blockIt->second->Write(iterFind->second.Offset, &value, sizeof(value));
        }
    }

    void Material::SetColor(const String& name, const glm::vec4& value)
    {
        const auto& iterFind = m_Bindings.find(name);
        if (iterFind == m_Bindings.cend())
            return;
        if (iterFind->second.DataType != ShaderDataType::Float4)
        {
            CW_ENGINE_WARN("Trying to write the wrong data type {}, expected {}, got color", name, ShaderDataTypeToString(iterFind->second.DataType));
            return;
        }
        for (auto& pass : m_Passes)
        {
            const auto blockIt = pass.UniformBlocks.find(iterFind->second.BufferName);
            if (blockIt != pass.UniformBlocks.end())
                blockIt->second->Write(iterFind->second.Offset, &value, sizeof(value));
        }
    }

    void Material::SetVector3(const String& name, const glm::vec3& value)
    {
        const auto& iterFind = m_Bindings.find(name);
        if (iterFind == m_Bindings.cend())
            return;
        if (iterFind->second.DataType != ShaderDataType::Float3)
        {
            CW_ENGINE_WARN("Trying to write the wrong data type {}, expected {}, got vector", name,
                           ShaderDataTypeToString(iterFind->second.DataType));
            return;
        }
        for (auto& pass : m_Passes)
        {
            const auto blockIt = pass.UniformBlocks.find(iterFind->second.BufferName);
            if (blockIt != pass.UniformBlocks.end())
                blockIt->second->Write(iterFind->second.Offset, &value, sizeof(value));
        }
    }

    void Material::SetMatrix(const String& name, const glm::mat4& value)
    {
        const auto& iterFind = m_Bindings.find(name);
        if (iterFind == m_Bindings.cend())
            return;
        if (iterFind->second.DataType != ShaderDataType::Mat4)
        {
            CW_ENGINE_WARN("Trying to write the wrong data type {}, expected {}, got matrix", name,
                           ShaderDataTypeToString(iterFind->second.DataType));
            return;
        }
        for (auto& pass : m_Passes)
        {
            const auto blockIt = pass.UniformBlocks.find(iterFind->second.BufferName);
            if (blockIt != pass.UniformBlocks.end())
                blockIt->second->Write(iterFind->second.Offset, &value, sizeof(value));
        }
    }

    void Material::SetTexture(const String& name, const AssetHandle<Texture>& texture)
    {
        for (auto& pass : m_Passes)
            pass.Uniforms->SetTexture(FRAGMENT_SHADER, name, texture.GetInternalPtr());
    }

    void Material::SetTexture(const String& name, const Ref<Texture>& texture)
    {
        for (auto& pass : m_Passes)
            pass.Uniforms->SetTexture(FRAGMENT_SHADER, name, texture);
    }

} // namespace Crowny
