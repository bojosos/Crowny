#include "cwpch.h"

#include "Crowny/Renderer/MaterialPreset.h"

#include "Crowny/Assets/AssetManager.h"
#include "Crowny/RenderAPI/Shader.h"

#include <algorithm>
#include <array>
#include <cctype>

namespace Crowny
{
    namespace
    {
        constexpr std::array<StringView, 6> VALUE_TYPE_NAMES = { "Float", "Float2", "Float3", "Color", "Int", "Bool" };

        bool EqualsIgnoreCase(StringView left, StringView right)
        {
            if (left.size() != right.size())
                return false;
            for (size_t index = 0; index < left.size(); index++)
            {
                if (std::tolower(static_cast<unsigned char>(left[index])) != std::tolower(static_cast<unsigned char>(right[index])))
                    return false;
            }
            return true;
        }

        String ShaderStem(StringView shaderName)
        {
            const Path path{ String(shaderName) };
            return path.stem().string();
        }
    } // namespace

    StringView MaterialPresetValueTypeName(MaterialPresetValueType type)
    {
        const size_t index = static_cast<size_t>(type);
        return index < VALUE_TYPE_NAMES.size() ? VALUE_TYPE_NAMES[index] : StringView("Float");
    }

    bool ParseMaterialPresetValueType(StringView name, MaterialPresetValueType& outType)
    {
        for (size_t index = 0; index < VALUE_TYPE_NAMES.size(); index++)
        {
            if (EqualsIgnoreCase(name, VALUE_TYPE_NAMES[index]))
            {
                outType = static_cast<MaterialPresetValueType>(index);
                return true;
            }
        }
        if (EqualsIgnoreCase(name, "Float4") || EqualsIgnoreCase(name, "Vector4"))
        {
            outType = MaterialPresetValueType::Color;
            return true;
        }
        if (EqualsIgnoreCase(name, "Vector3"))
        {
            outType = MaterialPresetValueType::Float3;
            return true;
        }
        if (EqualsIgnoreCase(name, "Vector2"))
        {
            outType = MaterialPresetValueType::Float2;
            return true;
        }
        return false;
    }

    ShaderDataType MaterialPresetValueTypeToShaderDataType(MaterialPresetValueType type)
    {
        switch (type)
        {
        case MaterialPresetValueType::Float:
            return ShaderDataType::Float;
        case MaterialPresetValueType::Float2:
            return ShaderDataType::Float2;
        case MaterialPresetValueType::Float3:
            return ShaderDataType::Float3;
        case MaterialPresetValueType::Color:
            return ShaderDataType::Float4;
        case MaterialPresetValueType::Int:
            return ShaderDataType::Int;
        case MaterialPresetValueType::Bool:
            return ShaderDataType::Bool;
        }
        return ShaderDataType::None;
    }

    const MaterialPresetParameter* MaterialPreset::Find(StringView name) const
    {
        const auto iter = std::find_if(m_Parameters.begin(), m_Parameters.end(),
                                       [name](const MaterialPresetParameter& parameter) { return parameter.Name == name; });
        return iter != m_Parameters.end() ? &*iter : nullptr;
    }

    bool MaterialPreset::Remove(StringView name)
    {
        const auto iter = std::find_if(m_Parameters.begin(), m_Parameters.end(),
                                       [name](const MaterialPresetParameter& parameter) { return parameter.Name == name; });
        if (iter == m_Parameters.end())
            return false;
        m_Parameters.erase(iter);
        return true;
    }

    MaterialPresetParameter& MaterialPreset::Upsert(const String& name, MaterialPresetValueType type)
    {
        auto iter = std::find_if(m_Parameters.begin(), m_Parameters.end(),
                                 [&name](const MaterialPresetParameter& parameter) { return parameter.Name == name; });
        if (iter == m_Parameters.end())
        {
            MaterialPresetParameter parameter;
            parameter.Name = name;
            m_Parameters.push_back(std::move(parameter));
            iter = std::prev(m_Parameters.end());
        }
        iter->Type = type;
        iter->Vector = glm::vec4(0.0f);
        iter->Integer = 0;
        return *iter;
    }

    void MaterialPreset::SetFloat(const String& name, float value) { Upsert(name, MaterialPresetValueType::Float).Vector.x = value; }

    void MaterialPreset::SetFloat2(const String& name, const glm::vec2& value)
    {
        Upsert(name, MaterialPresetValueType::Float2).Vector = glm::vec4(value, 0.0f, 0.0f);
    }

    void MaterialPreset::SetFloat3(const String& name, const glm::vec3& value)
    {
        Upsert(name, MaterialPresetValueType::Float3).Vector = glm::vec4(value, 0.0f);
    }

    void MaterialPreset::SetColor(const String& name, const glm::vec4& value) { Upsert(name, MaterialPresetValueType::Color).Vector = value; }

    void MaterialPreset::SetInt(const String& name, int32_t value) { Upsert(name, MaterialPresetValueType::Int).Integer = value; }

    void MaterialPreset::SetBool(const String& name, bool value) { Upsert(name, MaterialPresetValueType::Bool).Integer = value ? 1 : 0; }

    bool MaterialPreset::Validate(const Material::BindingMap& bindings, String* outError) const
    {
        for (const MaterialPresetParameter& parameter : m_Parameters)
        {
            const auto binding = bindings.find(parameter.Name);
            if (binding == bindings.end())
            {
                if (outError != nullptr)
                    *outError = "The shader has no parameter named '" + parameter.Name + "'.";
                return false;
            }
            const ShaderDataType expected = MaterialPresetValueTypeToShaderDataType(parameter.Type);
            if (binding->second.DataType != expected)
            {
                if (outError != nullptr)
                {
                    *outError = "Parameter '" + parameter.Name + "' is " + ShaderDataTypeToString(binding->second.DataType) + " but the preset stores " +
                                ShaderDataTypeToString(expected) + ".";
                }
                return false;
            }
        }
        if (outError != nullptr)
            outError->clear();
        return true;
    }

    StringView MaterialPreset::MaterialModelName(MaterialModel model)
    {
        switch (model)
        {
        case MaterialModel::Standard:
            return "Standard";
        case MaterialModel::Unlit:
            return "Unlit";
        case MaterialModel::Toon:
            return "Toon";
        }
        return "";
    }

    bool MaterialPreset::IsCompatibleWith(MaterialModel model, StringView shaderName) const
    {
        if (m_Target.empty())
            return true;
        if (EqualsIgnoreCase(m_Target, MaterialModelName(model)))
            return true;
        const String stem = ShaderStem(shaderName);
        return !stem.empty() && (EqualsIgnoreCase(m_Target, stem) || EqualsIgnoreCase(m_Target, shaderName));
    }

    bool MaterialPreset::IsCompatibleWith(const Material& material) const
    {
        const AssetHandle<Shader> shader = material.GetShader();
        if (!shader)
            return false;
        const MaterialRenderClassification classification = MaterialRenderClassifier::Classify(material);
        if (classification.IsUnsupported() && m_Target.empty())
            return true;

        String shaderName = shader->GetName();
        if (shaderName.empty() && AssetManager::TryGet() != nullptr)
        {
            Path shaderPath;
            if (AssetManager::TryGet()->GetAssetPath(shader.GetUUID(), shaderPath))
                shaderName = shaderPath.generic_string();
        }
        // An unsupported route has no meaningful model, so only the shader name can match.
        if (classification.IsUnsupported())
            return !ShaderStem(shaderName).empty() && EqualsIgnoreCase(m_Target, ShaderStem(shaderName));
        return IsCompatibleWith(classification.Model, shaderName);
    }

    Ref<MaterialPreset> MaterialPreset::CaptureFromMaterial(const Material& material, const String& name)
    {
        Ref<MaterialPreset> preset = CreateRef<MaterialPreset>();
        preset->SetName(name);
        if (material.GetShader())
        {
            const MaterialRenderClassification classification = MaterialRenderClassifier::Classify(material);
            if (!classification.IsUnsupported())
                preset->SetTarget(String(MaterialModelName(classification.Model)));
        }

        Vector<String> names;
        names.reserve(material.GetBindings().size());
        for (const auto& [bindingName, member] : material.GetBindings())
        {
            if (member.BufferName.rfind("cw_", 0) == 0)
                continue;
            names.push_back(bindingName);
        }
        std::sort(names.begin(), names.end());

        for (const String& bindingName : names)
        {
            const auto binding = material.GetBindings().find(bindingName);
            switch (binding->second.DataType)
            {
            case ShaderDataType::Float:
                preset->SetFloat(bindingName, material.GetDataParam<float>(bindingName));
                break;
            case ShaderDataType::Float2:
                preset->SetFloat2(bindingName, material.GetDataParam<glm::vec2>(bindingName));
                break;
            case ShaderDataType::Float3:
                preset->SetFloat3(bindingName, material.GetDataParam<glm::vec3>(bindingName));
                break;
            case ShaderDataType::Float4:
                preset->SetColor(bindingName, material.GetDataParam<glm::vec4>(bindingName));
                break;
            case ShaderDataType::Int:
                preset->SetInt(bindingName, material.GetDataParam<int>(bindingName));
                break;
            case ShaderDataType::Bool:
                preset->SetBool(bindingName, material.GetDataParam<bool>(bindingName));
                break;
            default:
                break; // Matrices and byte vectors are not preset material.
            }
        }
        return preset;
    }
} // namespace Crowny
