#include "cwpch.h"

#include "Crowny/Serialization/MaterialSerializer.h"

#include "Crowny/Assets/AssetManager.h"
#include "Crowny/Common/FileSystem.h"
#include "Crowny/Common/Yaml.h"
#include "Crowny/RenderAPI/Shader.h"
#include "Crowny/Renderer/ShaderParameter.h"

namespace Crowny
{
    static const uint32_t MATERIAL_YAML_VERSION = 1;

    MaterialSerializer::MaterialSerializer(const Ref<Material>& material) : m_Material(material) {}

    // --- Helpers to serialize param values by type ---

    static void SerializeDataParam(YAML::Emitter& out, const String& name, ShaderDataType type, const Material& mat)
    {
        out << YAML::BeginMap;
        out << YAML::Key << "Name" << YAML::Value << name;
        out << YAML::Key << "Type" << YAML::Value << (uint32_t)type;
        out << YAML::Key << "Value" << YAML::Value;

        switch (type)
        {
        case ShaderDataType::Float:
            out << mat.GetDataParam<float>(name);
            break;
        case ShaderDataType::Float2:
            out << mat.GetDataParam<glm::vec2>(name);
            break;
        case ShaderDataType::Float3:
            out << mat.GetDataParam<glm::vec3>(name);
            break;
        case ShaderDataType::Float4:
            out << mat.GetDataParam<glm::vec4>(name);
            break;
        case ShaderDataType::Int:
            out << mat.GetDataParam<int>(name);
            break;
        case ShaderDataType::Bool:
            out << (mat.GetDataParam<bool>(name) ? 1 : 0);
            break;
        case ShaderDataType::Mat4:
            out << mat.GetDataParam<glm::mat4>(name);
            break;
        default:
            out << 0;
            break;
        }
        out << YAML::EndMap;
    }

    static void DeserializeDataParam(const YAML::Node& node, Material& mat)
    {
        if (!node["Name"] || !node["Type"] || !node["Value"])
            return;

        String name = node["Name"].as<String>();
        ShaderDataType type = (ShaderDataType)node["Type"].as<uint32_t>();

        if (!mat.HasBinding(name))
            return;

        switch (type)
        {
        case ShaderDataType::Float:
            mat.SetFloat(name, node["Value"].as<float>(0.0f));
            break;
        case ShaderDataType::Float2:
            mat.SetFloat2(name, node["Value"].as<glm::vec2>(glm::vec2(0.0f)));
            break;
        case ShaderDataType::Float3:
            mat.SetVector3(name, node["Value"].as<glm::vec3>(glm::vec3(0.0f)));
            break;
        case ShaderDataType::Float4:
            mat.SetColor(name, node["Value"].as<glm::vec4>(glm::vec4(0.0f)));
            break;
        case ShaderDataType::Int:
            mat.SetInt(name, node["Value"].as<int>(0));
            break;
        case ShaderDataType::Bool:
            mat.SetBool(name, node["Value"].as<int>(0) != 0);
            break;
        case ShaderDataType::Mat4:
            mat.SetMatrix(name, node["Value"].as<glm::mat4>(glm::mat4(1.0f)));
            break;
        default:
            break;
        }
    }

    // --- Serialize ---

    String MaterialSerializer::SerializeToString()
    {
        YAML::Emitter out;
        out << YAML::Comment("Crowny Material");

        out << YAML::BeginMap;
        SerializeValueYAML(out, "Version", MATERIAL_YAML_VERSION);

        // Shader reference (by UUID)
        UUID shaderUuid = m_Material->GetShader() ? m_Material->GetShader().GetUUID() : UUID::EMPTY;
        SerializeValueYAML(out, "Shader", shaderUuid);

        // Material name
        SerializeValueYAML(out, "Name", m_Material->GetName());

        // Data parameters — only user-editable ones (skip cw_ blocks)
        out << YAML::Key << "Parameters" << YAML::Value << YAML::BeginSeq;
        for (const auto& [name, member] : m_Material->GetBindings())
        {
            if (member.BufferName.rfind("cw_", 0) == 0)
                continue;
            SerializeDataParam(out, name, member.DataType, *m_Material);
        }
        out << YAML::EndSeq;

        // Texture parameters — only user-editable ones (skip cw_ prefix)
        out << YAML::Key << "Textures" << YAML::Value << YAML::BeginSeq;
        const auto& textures = m_Material->GetTextureDescriptors();
        for (const auto& [name, descInfo] : textures)
        {
            if (name.rfind("cw_", 0) == 0)
                continue;
            AssetHandle<Texture> texHandle = m_Material->GetTextureHandle(name);
            UUID texUuid = texHandle ? texHandle.GetUUID() : UUID::EMPTY;
            out << YAML::BeginMap;
            out << YAML::Key << "Name" << YAML::Value << name;
            out << YAML::Key << "UUID" << YAML::Value << texUuid;
            out << YAML::EndMap;
        }
        out << YAML::EndSeq;

        out << YAML::EndMap;
        return String(out.c_str());
    }

    void MaterialSerializer::Serialize(const Path& filepath)
    {
        const String str = SerializeToString();
        Ref<DataStream> stream = FileSystem::CreateAndOpenFile(filepath);
        stream->Write(str.c_str(), str.size());
        stream->Close();
    }

    // --- Deserialize ---

    bool MaterialSerializer::DeserializeFromString(const String& yamlString)
    {
        if (yamlString.empty())
            return false;

        YAML::Node data;
        try
        {
            data = YAML::Load(yamlString);
        }
        catch (const std::exception& e)
        {
            CW_ENGINE_ERROR("Failed to parse Material YAML: {0}", e.what());
            return false;
        }

        if (!data || !data.IsMap())
            return false;

        // Load shader
        UUID shaderUuid;
        DeserializeValueYAML(data, "Shader", shaderUuid, UUID::EMPTY);
        if (!shaderUuid.Empty())
        {
            AssetHandle<Shader> shader = static_asset_cast<Shader>(AssetManager::TryGet()->LoadFromUUID(shaderUuid));
            if (shader)
            {
                m_Material->SetShader(shader);
            }
            else
            {
                CW_ENGINE_WARN("Material: could not load shader with UUID {}", shaderUuid.ToString());
                return false;
            }
        }

        // Load name
        if (data["Name"])
            m_Material->SetName(data["Name"].as<String>());

        // Load data parameters
        const YAML::Node& params = data["Parameters"];
        if (params && params.IsSequence())
        {
            for (const auto& paramNode : params)
                DeserializeDataParam(paramNode, *m_Material);
        }

        // Load texture parameters
        const YAML::Node& textures = data["Textures"];
        if (textures && textures.IsSequence())
        {
            for (const auto& texNode : textures)
            {
                if (!texNode["Name"] || !texNode["UUID"])
                    continue;
                String name = texNode["Name"].as<String>();
                UUID texUuid = texNode["UUID"].as<UUID>();
                if (!texUuid.Empty())
                {
                    AssetHandle<Texture> tex = static_asset_cast<Texture>(AssetManager::TryGet()->LoadFromUUID(texUuid));
                    if (tex)
                        m_Material->SetTexture(name, tex);
                }
            }
        }

        return true;
    }

    void MaterialSerializer::Deserialize(const Path& filepath)
    {
        Ref<DataStream> stream = FileSystem::OpenFile(filepath);
        if (!stream)
        {
            CW_ENGINE_ERROR("Failed to open material file: {}", filepath.string());
            return;
        }
        String contents = stream->GetAsString();
        stream->Close();
        DeserializeFromString(contents);
    }

} // namespace Crowny
