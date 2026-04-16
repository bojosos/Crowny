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
        if (m_Shader)
            ReloadParams();
    }

    void Material::SetVariation(const ShaderVariation& variation)
    {
        m_Variation = variation;
        ReloadParams();
    }

    void Material::ReloadParams()
    {
        m_Passes.clear();
        m_Bindings.clear();
        m_TextureHandles.clear();

        const auto& technique = m_Shader->GetTechnique(m_Variation);
        const auto& renderPasses = technique->GetRenderPasses();
        m_Passes.resize(renderPasses.size());

        for (uint32_t p = 0; p < renderPasses.size(); p++)
        {
            renderPasses[p]->Compile();
            m_Passes[p].Pipeline = renderPasses[p]->GetGraphicsPipeline();
            CreateAndAppendUniforms(p);
        }

        ApplyDefaults();
        ++m_ParamVersion;
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

    void Material::SetBool(const String& name, bool value)
    {
        const auto& iterFind = m_Bindings.find(name);
        if (iterFind == m_Bindings.cend())
            return;
        if (iterFind->second.DataType != ShaderDataType::Bool)
        {
            CW_ENGINE_WARN("Type mismatch for {}: expected {}, got Bool", name, ShaderDataTypeToString(iterFind->second.DataType));
            return;
        }
        int intVal = value ? 1 : 0; // GLSL bools are 4 bytes
        for (auto& pass : m_Passes)
        {
            const auto blockIt = pass.UniformBlocks.find(iterFind->second.BufferName);
            if (blockIt != pass.UniformBlocks.end())
                blockIt->second->Write(iterFind->second.Offset, &intVal, sizeof(intVal));
        }
        ++m_ParamVersion;
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
        ++m_ParamVersion;
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
        ++m_ParamVersion;
    }

    void Material::SetFloat3(const String& name, const glm::vec3& value)
    {
        SetVector3(name, value);
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
        ++m_ParamVersion;
    }

    void Material::SetInt2(const String& name, const glm::ivec2& value)
    {
        const auto& iterFind = m_Bindings.find(name);
        if (iterFind == m_Bindings.cend())
            return;
        if (iterFind->second.DataType != ShaderDataType::Int2)
        {
            CW_ENGINE_WARN("Type mismatch for {}: expected {}, got Int2", name, ShaderDataTypeToString(iterFind->second.DataType));
            return;
        }
        for (auto& pass : m_Passes)
        {
            const auto blockIt = pass.UniformBlocks.find(iterFind->second.BufferName);
            if (blockIt != pass.UniformBlocks.end())
                blockIt->second->Write(iterFind->second.Offset, &value, sizeof(value));
        }
        ++m_ParamVersion;
    }

    void Material::SetInt3(const String& name, const glm::ivec3& value)
    {
        const auto& iterFind = m_Bindings.find(name);
        if (iterFind == m_Bindings.cend())
            return;
        if (iterFind->second.DataType != ShaderDataType::Int3)
        {
            CW_ENGINE_WARN("Type mismatch for {}: expected {}, got Int3", name, ShaderDataTypeToString(iterFind->second.DataType));
            return;
        }
        for (auto& pass : m_Passes)
        {
            const auto blockIt = pass.UniformBlocks.find(iterFind->second.BufferName);
            if (blockIt != pass.UniformBlocks.end())
                blockIt->second->Write(iterFind->second.Offset, &value, sizeof(value));
        }
        ++m_ParamVersion;
    }

    void Material::SetInt4(const String& name, const glm::ivec4& value)
    {
        const auto& iterFind = m_Bindings.find(name);
        if (iterFind == m_Bindings.cend())
            return;
        if (iterFind->second.DataType != ShaderDataType::Int4)
        {
            CW_ENGINE_WARN("Type mismatch for {}: expected {}, got Int4", name, ShaderDataTypeToString(iterFind->second.DataType));
            return;
        }
        for (auto& pass : m_Passes)
        {
            const auto blockIt = pass.UniformBlocks.find(iterFind->second.BufferName);
            if (blockIt != pass.UniformBlocks.end())
                blockIt->second->Write(iterFind->second.Offset, &value, sizeof(value));
        }
        ++m_ParamVersion;
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
        ++m_ParamVersion;
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
        ++m_ParamVersion;
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
        ++m_ParamVersion;
    }

    void Material::SetMat3(const String& name, const glm::mat3& value)
    {
        const auto& iterFind = m_Bindings.find(name);
        if (iterFind == m_Bindings.cend())
            return;
        if (iterFind->second.DataType != ShaderDataType::Mat3)
        {
            CW_ENGINE_WARN("Type mismatch for {}: expected {}, got Mat3", name, ShaderDataTypeToString(iterFind->second.DataType));
            return;
        }
        for (auto& pass : m_Passes)
        {
            const auto blockIt = pass.UniformBlocks.find(iterFind->second.BufferName);
            if (blockIt != pass.UniformBlocks.end())
                blockIt->second->Write(iterFind->second.Offset, &value, sizeof(value));
        }
        ++m_ParamVersion;
    }

    void Material::SetTexture(const String& name, const AssetHandle<Texture>& texture)
    {
        m_TextureHandles[name] = texture;
        for (auto& pass : m_Passes)
            pass.Uniforms->SetTexture(FRAGMENT_SHADER, name, texture.GetInternalPtr());
        ++m_ParamVersion;
    }

    void Material::SetTexture(const String& name, const Ref<Texture>& texture)
    {
        for (auto& pass : m_Passes)
            pass.Uniforms->SetTexture(FRAGMENT_SHADER, name, texture);
        ++m_ParamVersion;
    }

    void Material::ApplyDefaults()
    {
        // Collect annotations from all shader stages
        for (uint32_t i = 0; i < SHADER_COUNT; i++)
        {
            const Ref<UniformDesc>& desc =
              m_Passes.empty() ? nullptr : m_Passes[0].Pipeline->GetParamInfo()->GetUniformDesc((ShaderType)i);
            if (!desc)
                continue;

            // Apply data param defaults from annotations
            for (const auto& [blockName, block] : desc->Uniforms)
            {
                for (const auto& member : block.Members)
                {
                    if (!member.DefaultValue.empty())
                    {
                        for (auto& pass : m_Passes)
                        {
                            auto blockIt = pass.UniformBlocks.find(blockName);
                            if (blockIt != pass.UniformBlocks.end())
                                blockIt->second->Write(member.Offset, member.DefaultValue.data(),
                                                       (uint32_t)member.DefaultValue.size());
                        }
                    }
                }
            }

            // Apply texture defaults from annotations
            for (const auto& [texName, texDesc] : desc->Textures)
            {
                auto annoIt = desc->Annotations.find(texName);
                if (annoIt == desc->Annotations.end() || !annoIt->second.HasDefault)
                    continue;

                const String& defStr = annoIt->second.DefaultValueStr;
                Ref<Texture> defaultTex;
                if (defStr == "white")
                    defaultTex = Texture::WHITE;
                else if (defStr == "black")
                    defaultTex = Texture::BLACK;
                // Add more builtin texture names as needed

                if (defaultTex)
                {
                    for (auto& pass : m_Passes)
                        pass.Uniforms->SetTexture(FRAGMENT_SHADER, texName, defaultTex);
                }
            }
        }
    }

    // --- MaterialParamHandle explicit instantiations ---

    template <typename T> void MaterialParamHandle<T>::Set(const T& value)
    {
        for (auto& pass : m_Material->m_Passes)
        {
            auto blockIt = pass.UniformBlocks.find(m_BufferName);
            if (blockIt != pass.UniformBlocks.end())
                blockIt->second->Write(m_Offset, &value, sizeof(T));
        }
        ++m_Material->m_ParamVersion;
    }

    template <typename T> T MaterialParamHandle<T>::Get() const
    {
        T value{};
        for (const auto& pass : m_Material->m_Passes)
        {
            auto blockIt = pass.UniformBlocks.find(m_BufferName);
            if (blockIt != pass.UniformBlocks.end())
            {
                blockIt->second->Read(m_Offset, &value, sizeof(value));
                return value;
            }
        }
        return value;
    }

    template class MaterialParamHandle<float>;
    template class MaterialParamHandle<glm::vec2>;
    template class MaterialParamHandle<glm::vec3>;
    template class MaterialParamHandle<glm::vec4>;
    template class MaterialParamHandle<int>;
    template class MaterialParamHandle<glm::ivec2>;
    template class MaterialParamHandle<glm::ivec3>;
    template class MaterialParamHandle<glm::ivec4>;
    template class MaterialParamHandle<bool>;
    template class MaterialParamHandle<glm::mat3>;
    template class MaterialParamHandle<glm::mat4>;

    // --- MaterialTextureHandle ---

    void MaterialTextureHandle::Set(const AssetHandle<Texture>& tex)
    {
        m_Material->m_TextureHandles[m_Name] = tex;
        for (auto& pass : m_Material->m_Passes)
            pass.Uniforms->SetTexture(FRAGMENT_SHADER, m_Name, tex.GetInternalPtr());
        ++m_Material->m_ParamVersion;
    }

    void MaterialTextureHandle::Set(const Ref<Texture>& tex)
    {
        for (auto& pass : m_Material->m_Passes)
            pass.Uniforms->SetTexture(FRAGMENT_SHADER, m_Name, tex);
        ++m_Material->m_ParamVersion;
    }

    AssetHandle<Texture> MaterialTextureHandle::Get() const { return m_Material->GetTextureHandle(m_Name); }

} // namespace Crowny
