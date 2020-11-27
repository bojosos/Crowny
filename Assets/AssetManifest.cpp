#include "cwpch.h"

#include "Crowny/Assets/AssetManifest.h"

#include <yaml-cpp/yaml.h>

namespace Crowny
{
    
    void AssetManifest::Serialize()
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

        std::ofstream fout(filepath);
        fout << out.c_str();
    }

    void AssetManifest::Deseralize()
    {
        std::ifstream fin(filepath);
        std::stringstream sstream;
        sstream << fin.rdbuf();
        
        YAML::Node data = YAML::Load(sstream.str());
        if (!data["Manifest"])
            return;

        m_Name = data["Manifest"].as<std::string>();
        auto rss = data["Resources"];
        for (auto resource : rss)
        {
            Uuid id = resource["Uuid"].as<Uuid>();
            std::string path = resource["Path"].as<std::string>();
            AssetManager::CreateResource(id, path);
        }
    }

    Ref<Texture2D> AssetManifest::LoadTexture(const std::string& texture)
    {
        return Texture2D::Create(texture); // Some path in front of this
    }

}