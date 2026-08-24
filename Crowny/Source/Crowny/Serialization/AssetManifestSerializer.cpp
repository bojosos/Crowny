#include "cwpch.h"

#include "Crowny/Assets/AssetManifest.h"
#include "Crowny/Serialization/AssetManifestSerializer.h"

namespace Crowny
{
    namespace
    {
        constexpr uint32_t ASSET_MANIFEST_VERSION = 1;
    }

    void AssetManifestSerializer::Serialize(const Ref<AssetManifest>& manifest, YAML::Emitter& out)
    {
        out << YAML::Comment("Crowny manifest");
        out << YAML::BeginMap;
        out << YAML::Key << "Version" << YAML::Value << ASSET_MANIFEST_VERSION;
        out << YAML::Key << "Manifest" << YAML::Value << manifest->m_Name;

        out << YAML::Key << "Assets" << YAML::Value;
        out << YAML::BeginSeq;
        Vector<Pair<UUID, Path>> assets(manifest->m_UuidToFilepath.begin(), manifest->m_UuidToFilepath.end());
        std::sort(assets.begin(), assets.end(), [](const auto& lhs, const auto& rhs) { return lhs.first < rhs.first; });
        for (const auto& uuidPath : assets)
        {
            out << YAML::BeginMap;
            out << YAML::Key << uuidPath.first << YAML::Value << uuidPath.second.string();
            out << YAML::EndMap;
        }

        out << YAML::EndSeq << YAML::EndMap;
    }

    Ref<AssetManifest> AssetManifestSerializer::Deserialize(const YAML::Node& node)
    {
        if (!node || !node.IsMap())
        {
            CW_ENGINE_ERROR("Asset manifest root must be a map.");
            return nullptr;
        }

        const uint32_t version = node["Version"].as<uint32_t>(0);
        if (version > ASSET_MANIFEST_VERSION)
        {
            CW_ENGINE_ERROR("Asset manifest version {} is newer than supported version {}.", version, ASSET_MANIFEST_VERSION);
            return nullptr;
        }

        Ref<AssetManifest> result = CreateRef<AssetManifest>();
        auto manifestName = node["Manifest"];
        if (manifestName)
            result->m_Name = manifestName.as<String>();
        auto assets = node["Assets"];
        if (assets && !assets.IsSequence())
        {
            CW_ENGINE_ERROR("Asset manifest 'Assets' value must be a sequence.");
            return nullptr;
        }
        if (assets)
        {
            for (auto asset : assets)
            {
                if (!asset.IsMap() || asset.size() != 1)
                {
                    CW_ENGINE_WARN("Ignoring malformed asset manifest entry.");
                    continue;
                }

                for (const auto& kv : asset)
                {
                    UUID id = kv.first.as<UUID>();
                    Path path = kv.second.as<String>();
                    result->RegisterAsset(id, path);
                }
            }
        }
        return result;
    }

} // namespace Crowny
