#include "cwpch.h"

#include "Crowny/Assets/AssetManager.h"
#include "Crowny/Common/Constants.h"
#include "Crowny/RenderAPI/UniformParams.h"
#include "Crowny/Renderer/BuiltInShaderCatalog.h"
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

        template <typename Name>
        bool SetTextureForReflectedPasses(const Vector<Material::PassData>& passes, const Name& name, const Ref<Texture>& texture)
        {
            bool assigned = false;
            for (const Material::PassData& pass : passes)
            {
                if (!pass.Pipeline || !pass.Uniforms)
                    continue;

                const Ref<UniformParamInfo>& paramInfo = pass.Pipeline->GetParamInfo();
                for (uint32_t stage = 0; paramInfo && stage < SHADER_COUNT; ++stage)
                {
                    const Ref<UniformDesc>& description = paramInfo->GetUniformDesc(static_cast<ShaderType>(stage));
                    if (!description)
                        continue;
                    const auto binding = description->Textures.find(name);
                    if (binding == description->Textures.end())
                        continue;
                    pass.Uniforms->SetTexture(binding->second.Set, binding->second.Slot, texture);
                    assigned = true;
                }
            }
            return assigned;
        }
    } // namespace

    Material::Material(const AssetHandle<Shader>& shader) : m_Shader(shader)
    {
        if (m_Shader)
            ReloadParams();
    }

    Ref<Material> Material::Create(const AssetHandle<Shader>& shader) { return CreateRef<Material>(shader); }

    Ref<Material> Material::CreatePBR(const AssetHandle<Shader>& shader)
    {
        Ref<Material> material = Create(shader);
        material->ApplyStandardDefaults();
        return material;
    }

    Ref<Material> Material::CreateDefault()
    {
        BuiltInShaderCatalog::EnsureRegistered();
        AssetManager* manager = AssetManager::TryGet();
        const AssetHandle<Shader> shader = manager ? manager->Load<Shader>(PBRIBL_SHADER_PATH) : AssetHandle<Shader>{};
        if (!shader)
        {
            CW_ENGINE_ERROR("Cannot create a material because the built-in PBR shader is unavailable.");
            return nullptr;
        }
        return CreatePBR(shader);
    }

    MaterialDomain Material::GetDomain() const
    {
        if (m_Shader)
        {
            const auto technique = m_Shader->GetTechnique(m_Variation);
            if (technique)
                for (const auto& tag : technique->GetTags())
                    if (tag == "material_model=decal")
                        return MaterialDomain::Decal;
        }
        return MaterialDomain::Surface;
    }

    Ref<Material> Material::CreateDecal()
    {
        BuiltInShaderCatalog::EnsureRegistered();
        auto* manager = AssetManager::TryGet();
        const auto shader = manager ? manager->Load<Shader>("Resources/Shaders/Decal.asset") : AssetHandle<Shader>{};
        if (!shader)
            return nullptr;
        auto material = Create(shader);
        material->SetTexture("decalColorMap", Texture::WHITE);
        material->SetTexture("decalNormalMap", Texture::NORMAL);
        material->SetTexture("decalSurfaceMap", Texture::WHITE);
        material->SetTexture("decalEmissionMap", Texture::WHITE);
        material->SetTexture("decalMaskMap", Texture::WHITE);
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
        SetTexture("emissiveMap", Texture::WHITE);
        SetColor("emissive", glm::vec4(0.0f));
        SetFloat("emissiveIntensity", 1.0f);
        SetFloat("alphaCutoff", 0.5f);
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
        const auto technique = m_Shader->GetTechnique(m_Variation);
        if (technique && std::find(technique->GetTags().begin(), technique->GetTags().end(), "procedural_texture") != technique->GetTags().end())
        {
            ApplyStandardDefaults();
            return;
        }
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
        MarkAssetsDirty();
    }

    void Material::NotifyAssetChanged(const AssetHandle<Asset>& asset)
    {
        if (!asset || asset->GetAssetType() != AssetType::Shader || asset.GetUUID() != m_Shader.GetUUID())
            return;
        const auto technique = m_Shader->GetTechnique(m_Variation);
        if (!technique || technique->GetRenderPasses().empty())
            return;
        if (std::any_of(technique->GetRenderPasses().begin(), technique->GetRenderPasses().end(),
                        [](const auto& pass) { return !pass || pass->IsCompute() || pass->IsRayTrace(); }))
            return;
        // First-load notifications also reach already initialized materials.
        if (!m_Passes.empty() && m_Passes.front().Pipeline == technique->GetRenderPasses().front()->GetGraphicsPipeline())
            return;
        struct Value
        {
            String Name;
            ShaderDataType Type;
            Vector<uint8_t> Bytes;
        };
        Vector<Value> values;
        for (const auto& [name, binding] : m_Bindings)
        {
            if (binding.BufferName.starts_with("cw_"))
                continue;
            for (const auto& pass : m_Passes)
            {
                const auto block = pass.UniformBlocks.find(binding.BufferID);
                if (block == pass.UniformBlocks.end())
                    continue;
                Value value{ name, binding.DataType, Vector<uint8_t>(ShaderDataTypeSize(binding.DataType)) };
                block->second->Read(binding.Offset, value.Bytes.data(), static_cast<uint32_t>(value.Bytes.size()));
                values.push_back(std::move(value));
                break;
            }
        }
        const auto textureHandles = m_TextureHandles;
        UnorderedMap<String, Pair<UniformResourceType, Ref<Texture>>> textures;
        for (const auto& [name, descriptor] : m_TextureDescriptors)
            if (!name.starts_with("cw_"))
                textures.emplace(name, Pair{ descriptor.Type, GetTexture(descriptor.Set, descriptor.Slot) });
        ReloadParams();
        ApplyModelDefaults();
        for (const auto& value : values)
        {
            const auto binding = m_Bindings.find(value.Name);
            if (binding == m_Bindings.end() || binding->second.DataType != value.Type)
                continue;
            for (const auto& pass : m_Passes)
            {
                const auto block = pass.UniformBlocks.find(binding->second.BufferID);
                if (block != pass.UniformBlocks.end())
                    block->second->Write(binding->second.Offset, value.Bytes.data(), static_cast<uint32_t>(value.Bytes.size()));
            }
        }
        for (const auto& [name, previous] : textures)
        {
            const auto descriptor = m_TextureDescriptors.find(name);
            if (descriptor == m_TextureDescriptors.end() || descriptor->second.Type != previous.first)
                continue;
            const auto handle = textureHandles.find(name);
            if (handle != textureHandles.end())
                SetTexture(name, handle->second);
            else
                SetTexture(name, previous.second);
        }
        if (m_HasAlphaModeOverride)
            SetFloat("alphaMode", static_cast<float>(m_AlphaMode));
        MarkAssetsDirty();
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
        SetFloat("alphaMode", static_cast<float>(alphaMode));
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
        m_TextureDescriptors.clear();
        for (auto& annotations : m_Annotations)
            annotations.clear();

        if (!m_Shader)
            return;

        const auto& technique = m_Shader->GetTechnique(m_Variation);
        const auto& renderPasses = technique->GetRenderPasses();
        m_Passes.resize(renderPasses.size());

        for (uint32_t p = 0; p < renderPasses.size(); p++)
        {
            // A shader pass owns its pipeline; material instances only own
            // parameter buffers. Replacing the shader supplies fresh passes.
            if (!renderPasses[p]->GetGraphicsPipeline())
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
            for (const auto& [name, texture] : paramDesc->Textures)
                m_TextureDescriptors.insert_or_assign(name, texture);
            for (const auto& [name, annotation] : paramDesc->Annotations)
                m_Annotations[i].insert_or_assign(name, annotation);
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

    Ref<Texture> Material::GetTexture(uint32_t set, uint32_t slot) const
    {
        for (const PassData& pass : m_Passes)
        {
            for (uint32_t stage = 0; stage < SHADER_COUNT; ++stage)
            {
                const Ref<UniformDesc>& description = pass.Pipeline->GetParamInfo()->GetUniformDesc(static_cast<ShaderType>(stage));
                if (!description)
                    continue;
                for (const auto& [name, texture] : description->Textures)
                {
                    if (texture.Set == set && texture.Slot == slot)
                    {
                        const Ref<Texture> value = pass.Uniforms->GetTexture(set, slot);
                        if (value)
                            return value;
                    }
                }
            }
        }
        return nullptr;
    }

    uint32_t Material::GetBlockBindingSlot(const String& blockName) const
    {
        for (const PassData& pass : m_Passes)
        {
            for (uint32_t stage = 0; stage < SHADER_COUNT; ++stage)
            {
                const Ref<UniformDesc>& description = pass.Pipeline->GetParamInfo()->GetUniformDesc(static_cast<ShaderType>(stage));
                if (!description)
                    continue;
                const auto block = description->Uniforms.find(blockName);
                if (block != description->Uniforms.end())
                    return block->second.Slot;
            }
        }
        return 0;
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

    void Material::OnDependentAssigned(const Ref<Asset>& dependent, const UUID& uuid)
    {
        if (!dependent || dependent->GetAssetType() != AssetType::Texture || uuid.Empty())
            return;
        for (const auto& [name, descriptor] : GetTextureDescriptors())
        {
            if (GetTexture(descriptor.Set, descriptor.Slot).get() == dependent.get())
                SetTexture(name, static_asset_cast<Texture>(AssetManager::Get().CreateAssetHandle(dependent, uuid)));
        }
    }

    void Material::SetTexture(const String& name, const AssetHandle<Texture>& texture)
    {
        if (!texture.HasUUID())
        {
            Ref<Texture> fallback;
            for (uint32_t stage = 0; stage < SHADER_COUNT; ++stage)
            {
                const auto& annotations = GetAnnotations(static_cast<ShaderType>(stage));
                const auto annotation = annotations.find(name);
                if (annotation != annotations.end() && annotation->second.HasDefault)
                {
                    const String& value = annotation->second.DefaultValueStr;
                    if (value == "white")
                        fallback = Texture::WHITE;
                    else if (value == "black")
                        fallback = Texture::BLACK;
                    else if (value == "normal")
                        fallback = Texture::NORMAL;
                }
            }
            if (name == "normalMap" && MaterialRenderClassifier::Classify(*this).Model == MaterialModel::Standard)
                fallback = Texture::NORMAL;
            SetTexture(name, fallback);
            return;
        }
        m_TextureHandles[name] = texture;
        if (!SetTextureForReflectedPasses(m_Passes, name, texture.GetInternalPtr()))
            CW_ENGINE_WARN("Texture with name {} does not exist in any fragment shader pass", name);
        ++m_ParamVersion;
    }

    void Material::SetTexture(const String& name, const Ref<Texture>& texture)
    {
        m_TextureHandles.erase(name);
        if (!SetTextureForReflectedPasses(m_Passes, name, texture))
            CW_ENGINE_WARN("Texture with name {} does not exist in any fragment shader pass", name);
        ++m_ParamVersion;
    }

    void Material::SetTexture(HashedString name, const Ref<Texture>& texture)
    {
        m_TextureHandles.erase(String(name.GetView()));
        if (!SetTextureForReflectedPasses(m_Passes, name, texture))
            CW_ENGINE_WARN("Texture with name {} does not exist in any fragment shader pass", name.GetView());
        ++m_ParamVersion;
    }

    void Material::SetTexture(MaterialPropertyID name, const Ref<Texture>& texture) { SetTexture(GetPropertyName(name), texture); }

    void Material::ApplyDefaults()
    {
        for (const PassData& pass : m_Passes)
        {
            for (uint32_t stage = 0; stage < SHADER_COUNT; ++stage)
            {
                const Ref<UniformDesc>& description = pass.Pipeline->GetParamInfo()->GetUniformDesc(static_cast<ShaderType>(stage));
                if (!description)
                    continue;
                for (const auto& [blockName, block] : description->Uniforms)
                {
                    const auto uniformBlock = pass.UniformBlocks.find(StringID(blockName));
                    if (uniformBlock == pass.UniformBlocks.end())
                        continue;
                    for (const auto& member : block.Members)
                    {
                        if (!member.DefaultValue.empty())
                            uniformBlock->second->Write(member.Offset, member.DefaultValue.data(), static_cast<uint32_t>(member.DefaultValue.size()));
                    }
                }
                for (const auto& [name, texture] : description->Textures)
                {
                    const auto annotation = description->Annotations.find(name);
                    if (annotation == description->Annotations.end() || !annotation->second.HasDefault)
                        continue;
                    Ref<Texture> value;
                    const String& defaultName = annotation->second.DefaultValueStr;
                    if (defaultName == "white")
                        value = Texture::WHITE;
                    else if (defaultName == "black")
                        value = Texture::BLACK;
                    else if (defaultName == "normal")
                        value = Texture::NORMAL;
                    if (value)
                        pass.Uniforms->SetTexture(texture.Set, texture.Slot, value);
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

    void MaterialTextureHandle::Set(const AssetHandle<Texture>& tex) { m_Material->SetTexture(m_Name, tex); }

    void MaterialTextureHandle::Set(const Ref<Texture>& tex) { m_Material->SetTexture(m_Name, tex); }

    AssetHandle<Texture> MaterialTextureHandle::Get() const { return m_Material->GetTextureHandle(m_Name); }

} // namespace Crowny
