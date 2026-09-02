#include "cwpch.h"

#include "Crowny/Serialization/MaterialPresetSerializer.h"

#include "Crowny/Common/FileSystem.h"
#include "Crowny/Common/Yaml.h"

namespace Crowny
{
    MaterialPresetSerializer::MaterialPresetSerializer(const Ref<MaterialPreset>& preset) : m_Preset(preset) {}

    String MaterialPresetSerializer::SerializeToString() const
    {
        YAML::Emitter out;
        out << YAML::Comment("Crowny Material Preset");
        out << YAML::BeginMap;
        SerializeValueYAML(out, "Version", YAML_VERSION);
        SerializeValueYAML(out, "Name", m_Preset->GetName());
        SerializeValueYAML(out, "Target", m_Preset->GetTarget());

        out << YAML::Key << "Parameters" << YAML::Value << YAML::BeginSeq;
        for (const MaterialPresetParameter& parameter : m_Preset->GetParameters())
        {
            out << YAML::BeginMap;
            out << YAML::Key << "Name" << YAML::Value << parameter.Name;
            out << YAML::Key << "Type" << YAML::Value << String(MaterialPresetValueTypeName(parameter.Type));
            out << YAML::Key << "Value" << YAML::Value;
            switch (parameter.Type)
            {
            case MaterialPresetValueType::Float:
                out << parameter.Vector.x;
                break;
            case MaterialPresetValueType::Float2:
                out << glm::vec2(parameter.Vector);
                break;
            case MaterialPresetValueType::Float3:
                out << glm::vec3(parameter.Vector);
                break;
            case MaterialPresetValueType::Color:
                out << parameter.Vector;
                break;
            case MaterialPresetValueType::Int:
                out << parameter.Integer;
                break;
            case MaterialPresetValueType::Bool:
                out << (parameter.Integer != 0);
                break;
            }
            out << YAML::EndMap;
        }
        out << YAML::EndSeq;
        out << YAML::EndMap;
        return String(out.c_str());
    }

    bool MaterialPresetSerializer::Serialize(const Path& filepath)
    {
        if (m_Preset == nullptr || filepath.empty())
        {
            CW_ENGINE_ERROR("Cannot save a material preset without a preset and a destination path.");
            return false;
        }
        try
        {
            String writeError;
            if (!FileSystem::WriteTextFileAtomic(filepath, SerializeToString(), &writeError))
            {
                CW_ENGINE_ERROR("Failed to publish material preset '{}': {}", filepath, writeError);
                return false;
            }
            return true;
        }
        catch (const std::exception& error)
        {
            CW_ENGINE_ERROR("Failed to serialize material preset '{}': {}", filepath, error.what());
            return false;
        }
    }

    bool MaterialPresetSerializer::Deserialize(const Path& filepath)
    {
        const String text = FileSystem::ReadTextFile(filepath);
        if (text.empty())
        {
            CW_ENGINE_ERROR("Material preset '{}' is missing or empty.", filepath);
            return false;
        }
        if (!DeserializeFromString(text))
            return false;
        if (m_Preset->GetName().empty())
            m_Preset->SetName(filepath.stem().string());
        return true;
    }

    bool MaterialPresetSerializer::DeserializeFromString(const String& yamlString)
    {
        if (m_Preset == nullptr || yamlString.empty())
            return false;

        YAML::Node data;
        try
        {
            data = YAML::Load(yamlString);
        }
        catch (const std::exception& error)
        {
            CW_ENGINE_ERROR("Failed to parse material preset YAML: {}", error.what());
            return false;
        }
        if (!data || !data.IsMap())
            return false;

        const uint32_t version = data["Version"].as<uint32_t>(1u);
        if (version > YAML_VERSION)
        {
            CW_ENGINE_ERROR("Material preset version {} is newer than supported version {}", version, YAML_VERSION);
            return false;
        }

        m_Preset->Clear();
        m_Preset->SetName(data["Name"].as<String>(""));
        m_Preset->SetTarget(data["Target"].as<String>(""));

        const YAML::Node& parameters = data["Parameters"];
        if (!parameters || parameters.IsNull())
            return true;
        if (!parameters.IsSequence())
        {
            CW_ENGINE_ERROR("Material preset 'Parameters' must be a sequence.");
            return false;
        }

        for (const YAML::Node& node : parameters)
        {
            if (!node.IsMap() || !node["Name"] || !node["Type"] || !node["Value"])
            {
                CW_ENGINE_ERROR("Material preset parameters need Name, Type and Value.");
                return false;
            }
            const String name = node["Name"].as<String>("");
            const String typeName = node["Type"].as<String>("");
            MaterialPresetValueType type;
            if (name.empty() || !ParseMaterialPresetValueType(typeName, type))
            {
                CW_ENGINE_ERROR("Material preset parameter '{}' has an unknown type '{}'.", name, typeName);
                return false;
            }
            const YAML::Node& value = node["Value"];
            try
            {
                switch (type)
                {
                case MaterialPresetValueType::Float:
                    m_Preset->SetFloat(name, value.as<float>());
                    break;
                case MaterialPresetValueType::Float2:
                    m_Preset->SetFloat2(name, value.as<glm::vec2>());
                    break;
                case MaterialPresetValueType::Float3:
                    m_Preset->SetFloat3(name, value.as<glm::vec3>());
                    break;
                case MaterialPresetValueType::Color:
                    m_Preset->SetColor(name, value.as<glm::vec4>());
                    break;
                case MaterialPresetValueType::Int:
                    m_Preset->SetInt(name, value.as<int32_t>());
                    break;
                case MaterialPresetValueType::Bool:
                    m_Preset->SetBool(name, value.IsScalar() && value.Scalar() != "0" && value.Scalar() != "false" && value.Scalar() != "False");
                    break;
                }
            }
            catch (const std::exception& error)
            {
                CW_ENGINE_ERROR("Material preset parameter '{}' has an invalid {} value: {}", name, typeName, error.what());
                return false;
            }
        }
        return true;
    }
} // namespace Crowny
