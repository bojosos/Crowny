#include "cwpch.h"

#include "Crowny/Assets/AssetManifest.h"
#include "Crowny/Serialization/AssetManifestSerializer.h"

namespace Crowny
{

    void AssetManifestSerializer::Serialize(const Ref<AssetManifest>& manifest, YAML::Emitter& out)
    {
        out << YAML::Comment("Crowny manifest");
        out << YAML::BeginMap;
        out << YAML::Key << "Manifest" << YAML::Value << manifest->m_Name;

        out << YAML::Key << "Assets" << YAML::Value;
        out << YAML::BeginSeq;
        for (auto uuidPath : manifest->m_UuidToFilepath)
        {
            out << YAML::BeginMap;
            out << YAML::Key << uuidPath.first << YAML::Value << uuidPath.second.string();
            out << YAML::EndMap;
        }

        out << YAML::EndSeq << YAML::EndMap;
    }

    Ref<AssetManifest> AssetManifestSerializer::Deserialize(const YAML::Node& node)
    {
        Ref<AssetManifest> result = CreateRef<AssetManifest>();
        auto manifestName = node["Manifest"];
        if (manifestName)
            result->m_Name = manifestName.as<String>();
        auto assets = node["Assets"];
        if (assets)
        {
            for (auto asset : assets)
            {
                for (const auto& kv : asset)
                {
                    UUID id = kv.first.as<UUID>();
                    Path path = kv.second.as<String>();
                    result->m_FilepathToUuid[path] = id;
                    result->m_UuidToFilepath[id] = path;
                }
            }
        }
        return result;
    }

} // namespace Crowny