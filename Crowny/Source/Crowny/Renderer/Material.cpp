#include "cwpch.h"

#include "Crowny/RenderAPI/UniformParams.h"
#include "Crowny/Renderer/GpuMaterial.h"
#include "Crowny/Renderer/Material.h"
#include "Crowny/Renderer/MaterialPreset.h"
#include "Crowny/Renderer/MaterialPresetLibrary.h"

namespace Crowny
{
    namespace
    {
        std::atomic<uint64_t> s_NextMaterialLayoutVersion{ 1u };

        StringView GetNameView(const String& name) { return name; }
        StringView GetNameView(HashedString name) { return name.GetView(); }
        HashedString GetPropertyName(MaterialPropertyID name) { return HashedString(StringIDTable::GetString(name.Value)); }
    } // namespace

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
        material->ApplyStandardDefaults();
        return material;
    }

    Ref<Material> Material::CreateToon(const AssetHandle<Shader>& shader)
    {
        Ref<Material> material = Create(shader);
        material->ApplyToonDefaults();
        return material;
    }

    Ref<Material> Material::CreateUnlit(const AssetHandle<Shader>& shader)
    {
        Ref<Material> material = Create(shader);
        material->ApplyUnlitDefaults();
        return material;
    }

    void Material::ApplyStandardDefaults()
    {
        SetTexture("albedoMap", Texture::WHITE);
        SetTexture("metallicMap", Texture::WHITE);
        SetTexture("roughnessMap", Texture::WHITE);
        SetTexture("normalMap", Texture::NORMAL);
        SetTexture("aoMap", Texture::WHITE);
        SetColor("albedo", glm::vec4(1.0f));
        SetFloat("roughness", 0.5f);
        SetFloat("metalness", 0.0f);
        SetFloat("useIBL", 0.0f);
    }

    void Material::ApplyToonDefaults()
    {
        SetTexture("albedoMap", Texture::WHITE);
        SetTexture("toonPatternTexture", Texture::WHITE);
        SetTexture("toonRampTexture", Texture::WHITE);
        SetTexture("toonMatcapTexture", Texture::WHITE);
        ApplyToonPreset(ToonMaterialPreset::Classic);
        SetVector3("camPos", glm::vec3(0.0f));
    }

    void Material::ApplyUnlitDefaults()
    {
        SetTexture("albedoMap", Texture::WHITE);
        SetColor("tint", glm::vec4(1.0f));
    }

    void Material::ApplyModelDefaults()
    {
        if (!m_Shader)
            return;
        const MaterialRenderClassification classification = MaterialRenderClassifier::Classify(*this);
        if (classification.IsUnsupported())
            return;
        switch (classification.Model)
        {
        case MaterialModel::Standard:
            ApplyStandardDefaults();
            break;
        case MaterialModel::Toon:
            ApplyToonDefaults();
            break;
        case MaterialModel::Unlit:
            ApplyUnlitDefaults();
            break;
        }
    }

    bool Material::ApplyPreset(const MaterialPreset& preset)
    {
        if (!m_Shader || !preset.Validate(m_Bindings))
            return false;

        for (const MaterialPresetParameter& parameter : preset.GetParameters())
        {
            switch (parameter.Type)
            {
            case MaterialPresetValueType::Float:
                SetFloat(parameter.Name, parameter.Vector.x);
                break;
            case MaterialPresetValueType::Float2:
                SetFloat2(parameter.Name, glm::vec2(parameter.Vector));
                break;
            case MaterialPresetValueType::Float3:
                SetVector3(parameter.Name, glm::vec3(parameter.Vector));
                break;
            case MaterialPresetValueType::Color:
                SetColor(parameter.Name, parameter.Vector);
                break;
            case MaterialPresetValueType::Int:
                SetInt(parameter.Name, parameter.Integer);
                break;
            case MaterialPresetValueType::Bool:
                SetBool(parameter.Name, parameter.Integer != 0);
                break;
            }
        }
        return true;
    }

    bool Material::ApplyToonPreset(ToonMaterialPreset preset)
    {
        if (preset < ToonMaterialPreset::Classic || preset > ToonMaterialPreset::Hatched)
            return false;
        const Ref<MaterialPreset> data = MaterialPresetLibrary::LoadBuiltIn(MaterialPresetLibrary::BuiltInToonPresetName(preset));
        if (data == nullptr || MaterialRenderClassifier::Classify(*this).Model != MaterialModel::Toon)
            return false;
        return ApplyPreset(*data);
    }

    void Material::SetShader(const AssetHandle<Shader>& shader)
    {
        m_Shader = shader;
        ReloadParams();
    }

    void Material::SetVariation(const ShaderVariation& variation)
    {
        m_Variation = variation;
        ReloadParams();
    }

    void Material::SetAlphaMode(AlphaMode alphaMode)
    {
        if (alphaMode > AlphaMode::WeightedOIT)
        {
            CW_ENGINE_WARN("Ignoring invalid material alpha mode {}", static_cast<uint32_t>(alphaMode));
            return;
        }
        if (m_HasAlphaModeOverride && m_AlphaMode == alphaMode)
            return;
        m_AlphaMode = alphaMode;
        m_HasAlphaModeOverride = true;
        ++m_ParamVersion;
    }

    void Material::ClearAlphaModeOverride()
    {
        if (!m_HasAlphaModeOverride)
            return;
        m_HasAlphaModeOverride = false;
        ++m_ParamVersion;
    }

    void Material::ReloadParams()
    {
        m_LayoutVersion = NextLayoutVersion();
        ++m_ParamVersion;
        m_Passes.clear();
        m_Bindings.clear();
        m_TextureHandles.clear();

        if (!m_Shader)
            return;

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
    }

    uint64_t Material::NextLayoutVersion() { return s_NextMaterialLayoutVersion.fetch_add(1u, std::memory_order_relaxed); }

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
                    // Bindings with the same name have the same meaning across passes.
                    m_Bindings[uniformBuffer.Members[j].Name] =
                      UniformMember{ uniformBuffer.Members[j].Offset, uniformBuffer.Members[j].DataType, name, StringID(name) };
                }

                const StringID bufferID(name);
                pass.UniformBlocks[bufferID] = UniformBufferBlock::Create(uniformBuffer.BlockSize, BufferUsage::BU_DYNAMIC_DRAW);
                pass.Uniforms->SetUniformBlockBuffer(name, pass.UniformBlocks[bufferID]);
            }
        }
    }

    void Material::FlushUniformBuffers()
    {
        for (const auto& pass : m_Passes)
            for (const auto& [_, block] : pass.UniformBlocks)
                block->FlushToGpu();
    }

    template <typename Name, typename Value>
    void Material::SetDataParam(const Name& name, ShaderDataType expectedType, const Value& value, StringView valueType)
    {
        const auto iterFind = m_Bindings.find(name);
        if (iterFind == m_Bindings.cend())
            return;
        if (iterFind->second.DataType != expectedType)
        {
            CW_ENGINE_WARN("Type mismatch for {}: expected {}, got {}", GetNameView(name), ShaderDataTypeToString(iterFind->second.DataType),
                           valueType);
            return;
        }

        for (const auto& pass : m_Passes)
        {
            const auto blockIt = pass.UniformBlocks.find(iterFind->second.BufferID);
            if (blockIt != pass.UniformBlocks.end())
                blockIt->second->Write(iterFind->second.Offset, &value, sizeof(value));
        }
        ++m_ParamVersion;
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
        for (const auto& pass : m_Passes)
        {
            const auto blockIt = pass.UniformBlocks.find(iterFind->second.BufferID);
            if (blockIt != pass.UniformBlocks.end())
                blockIt->second->Write(iterFind->second.Offset, &intVal, sizeof(intVal));
        }
        ++m_ParamVersion;
    }

    void Material::SetFloat(const String& name, float value) { SetDataParam(name, ShaderDataType::Float, value, "Float"); }

    void Material::SetFloat(HashedString name, float value) { SetDataParam(name, ShaderDataType::Float, value, "Float"); }

    void Material::SetFloat(MaterialPropertyID name, float value) { SetFloat(GetPropertyName(name), value); }

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
        for (const auto& pass : m_Passes)
        {
            const auto blockIt = pass.UniformBlocks.find(iterFind->second.BufferID);
            if (blockIt != pass.UniformBlocks.end())
                blockIt->second->Write(iterFind->second.Offset, &value, sizeof(value));
        }
        ++m_ParamVersion;
    }

    void Material::SetFloat3(const String& name, const glm::vec3& value) { SetVector3(name, value); }

    void Material::SetInt(const String& name, int value) { SetDataParam(name, ShaderDataType::Int, value, "Int"); }

    void Material::SetInt(HashedString name, int value) { SetDataParam(name, ShaderDataType::Int, value, "Int"); }

    void Material::SetInt(MaterialPropertyID name, int value) { SetInt(GetPropertyName(name), value); }

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
        for (const auto& pass : m_Passes)
        {
            const auto blockIt = pass.UniformBlocks.find(iterFind->second.BufferID);
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
        for (const auto& pass : m_Passes)
        {
            const auto blockIt = pass.UniformBlocks.find(iterFind->second.BufferID);
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
        for (const auto& pass : m_Passes)
        {
            const auto blockIt = pass.UniformBlocks.find(iterFind->second.BufferID);
            if (blockIt != pass.UniformBlocks.end())
                blockIt->second->Write(iterFind->second.Offset, &value, sizeof(value));
        }
        ++m_ParamVersion;
    }

    void Material::SetColor(const String& name, const glm::vec4& value) { SetDataParam(name, ShaderDataType::Float4, value, "Color"); }

    void Material::SetColor(HashedString name, const glm::vec4& value) { SetDataParam(name, ShaderDataType::Float4, value, "Color"); }

    void Material::SetColor(MaterialPropertyID name, const glm::vec4& value) { SetColor(GetPropertyName(name), value); }

    void Material::SetVector4Array(const String& name, const glm::vec4* values, uint32_t count)
    {
        if (values == nullptr || count == 0)
            return;
        const auto binding = m_Bindings.find(name);
        if (binding == m_Bindings.end() || binding->second.DataType != ShaderDataType::Float4)
            return;
        for (const PassData& pass : m_Passes)
        {
            const auto block = pass.UniformBlocks.find(binding->second.BufferID);
            if (block != pass.UniformBlocks.end())
                block->second->Write(binding->second.Offset, values, count * sizeof(glm::vec4));
        }
        ++m_ParamVersion;
    }

    void Material::SetInt4Array(const String& name, const glm::ivec4* values, uint32_t count)
    {
        if (values == nullptr || count == 0)
            return;
        const auto binding = m_Bindings.find(name);
        if (binding == m_Bindings.end() || binding->second.DataType != ShaderDataType::Int4)
            return;
        for (const PassData& pass : m_Passes)
        {
            const auto block = pass.UniformBlocks.find(binding->second.BufferID);
            if (block != pass.UniformBlocks.end())
                block->second->Write(binding->second.Offset, values, count * sizeof(glm::ivec4));
        }
        ++m_ParamVersion;
    }

    void Material::SetVector3(const String& name, const glm::vec3& value) { SetDataParam(name, ShaderDataType::Float3, value, "Vector3"); }

    void Material::SetVector3(HashedString name, const glm::vec3& value) { SetDataParam(name, ShaderDataType::Float3, value, "Vector3"); }

    void Material::SetVector3(MaterialPropertyID name, const glm::vec3& value) { SetVector3(GetPropertyName(name), value); }

    void Material::SetMatrix(const String& name, const glm::mat4& value) { SetDataParam(name, ShaderDataType::Mat4, value, "Matrix4"); }

    void Material::SetMatrix(HashedString name, const glm::mat4& value) { SetDataParam(name, ShaderDataType::Mat4, value, "Matrix4"); }

    void Material::SetMatrix(MaterialPropertyID name, const glm::mat4& value) { SetMatrix(GetPropertyName(name), value); }

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
        for (const auto& pass : m_Passes)
        {
            const auto blockIt = pass.UniformBlocks.find(iterFind->second.BufferID);
            if (blockIt != pass.UniformBlocks.end())
                blockIt->second->Write(iterFind->second.Offset, &value, sizeof(value));
        }
        ++m_ParamVersion;
    }

    void Material::SetTexture(const String& name, const AssetHandle<Texture>& texture)
    {
        m_TextureHandles[name] = texture;
        for (const auto& pass : m_Passes)
            pass.Uniforms->SetTexture(FRAGMENT_SHADER, name, texture.GetInternalPtr());
        ++m_ParamVersion;
    }

    void Material::SetTexture(const String& name, const Ref<Texture>& texture)
    {
        for (const auto& pass : m_Passes)
            pass.Uniforms->SetTexture(FRAGMENT_SHADER, name, texture);
        ++m_ParamVersion;
    }

    void Material::SetTexture(HashedString name, const Ref<Texture>& texture)
    {
        for (const auto& pass : m_Passes)
            pass.Uniforms->SetTexture(FRAGMENT_SHADER, name, texture);
        ++m_ParamVersion;
    }

    void Material::SetTexture(MaterialPropertyID name, const Ref<Texture>& texture) { SetTexture(GetPropertyName(name), texture); }

    void Material::ApplyDefaults()
    {
        // Collect annotations from all shader stages
        for (uint32_t i = 0; i < SHADER_COUNT; i++)
        {
            const Ref<UniformDesc>& desc = m_Passes.empty() ? nullptr : m_Passes[0].Pipeline->GetParamInfo()->GetUniformDesc((ShaderType)i);
            if (!desc)
                continue;

            // Apply data param defaults from annotations
            for (const auto& [blockName, block] : desc->Uniforms)
            {
                const StringID blockID(blockName);
                for (const auto& member : block.Members)
                {
                    if (!member.DefaultValue.empty())
                    {
                        for (const auto& pass : m_Passes)
                        {
                            const auto blockIt = pass.UniformBlocks.find(blockID);
                            if (blockIt != pass.UniformBlocks.end())
                                blockIt->second->Write(member.Offset, member.DefaultValue.data(), (uint32_t)member.DefaultValue.size());
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
                    for (const auto& pass : m_Passes)
                        pass.Uniforms->SetTexture(FRAGMENT_SHADER, texName, defaultTex);
                }
            }
        }
    }

    // --- MaterialParamHandle explicit instantiations ---

    template <typename T> void MaterialParamHandle<T>::Set(const T& value)
    {
        for (const auto& pass : m_Material->m_Passes)
        {
            auto blockIt = pass.UniformBlocks.find(m_BufferID);
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
            auto blockIt = pass.UniformBlocks.find(m_BufferID);
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
        for (const auto& pass : m_Material->m_Passes)
            pass.Uniforms->SetTexture(FRAGMENT_SHADER, m_Name, tex.GetInternalPtr());
        ++m_Material->m_ParamVersion;
    }

    void MaterialTextureHandle::Set(const Ref<Texture>& tex)
    {
        for (const auto& pass : m_Material->m_Passes)
            pass.Uniforms->SetTexture(FRAGMENT_SHADER, m_Name, tex);
        ++m_Material->m_ParamVersion;
    }

    AssetHandle<Texture> MaterialTextureHandle::Get() const { return m_Material->GetTextureHandle(m_Name); }

} // namespace Crowny
