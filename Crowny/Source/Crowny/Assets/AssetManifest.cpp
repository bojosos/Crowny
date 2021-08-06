#include "cwpch.h"

#include "Crowny/Assets/AssetManifest.h"

#include "Crowny/Common/VirtualFileSystem.h"
#include "Crowny/Common/Yaml.h"

namespace Crowny
{

    AssetManifest::AssetManifest(const std::string& name) : m_Name(name) {}

    void AssetManifest::Serialize(const std::string& filepath)
    {
        YAML::Emitter out;
        out << YAML::Comment("Crowny Manifest");

        out << YAML::BeginMap;
        out << YAML::Key << "Manifest" << YAML::Value << m_Name;
        out << YAML::Key << "Resources" << YAML::Value;

        out << YAML::BeginSeq;
        for (auto& it : m_Paths)
        {
            out << YAML::BeginMap;
            out << YAML::Key << "Uuid" << YAML::Value << it.first;
            out << YAML::Key << "Path" << YAML::Value << it.second;
            out << YAML::EndMap;
        }
        out << YAML::EndSeq << YAML::EndMap;

        VirtualFileSystem::Get()->WriteTextFile(filepath, out.c_str());
    }

    void AssetManifest::Deserialize(const std::string& filepath)
    {
        std::string text = VirtualFileSystem::Get()->ReadTextFile(filepath);
        YAML::Node data = YAML::Load(text);

        if (!data["Manifest"])
            return;

        m_Name = data["Manifest"].as<std::string>();
        auto rss = data["Resources"];
        for (auto resource : rss)
        {
            Uuid id = resource["Uuid"].as<Uuid>();
            std::string path = resource["Path"].as<std::string>();
            m_Uuids[path] = id;
            m_Paths[id] = path;
        }
    }

} // namespace Crowny