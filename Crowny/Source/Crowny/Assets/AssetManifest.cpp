#include "cwpch.h"

#include "Crowny/Assets/AssetManifest.h"

#include "Crowny/Common/FileSystem.h"
#include "Crowny/Common/Yaml.h"

namespace Crowny
{

    AssetManifest::AssetManifest(const String& name) : m_Name(name) {}

    bool AssetManifest::UuidToFilepath(const UUID& uuid, Path& outPath) const
    {
        auto findIter = m_UuidToFilepath.find(uuid);
        if (findIter != m_UuidToFilepath.end())
        {
            outPath = findIter->second;
            return true;
        }
        outPath = Path();
        return false;
    }

    bool AssetManifest::FilepathToUuid(const Path& path, UUID& outUuid) const
    {
        auto findIter = m_FilepathToUuid.find(path);
        if (findIter != m_FilepathToUuid.end())
        {
            outUuid = findIter->second;
            return true;
        }
        outUuid = UUID();
        return false;
    }

    bool AssetManifest::FilepathExists(const Path& path) const
    {
        auto findIter = m_FilepathToUuid.find(path);
        return findIter == m_FilepathToUuid.end();
    }

    bool AssetManifest::UuidExists(const UUID& uuid) const
    {
        auto findIter = m_UuidToFilepath.find(uuid);
        return findIter != m_UuidToFilepath.end();
    }

    void AssetManifest::RegisterAsset(const UUID& uuid, const Path& path)
    {
        auto findIter = m_UuidToFilepath.find(uuid);
        if (findIter != m_UuidToFilepath.end())
        {
            if (findIter->second != path)
            {
                m_FilepathToUuid.erase(findIter->second);
                m_UuidToFilepath[uuid] = path;
                m_FilepathToUuid[path] = uuid;
            }
        }
        else
        {
            auto findIter2 = m_FilepathToUuid.find(path);
            if (findIter2 != m_FilepathToUuid.end())
                m_UuidToFilepath.erase(findIter2->second);

            m_UuidToFilepath[uuid] = path;
            m_FilepathToUuid[path] = uuid;
        }
    }

    void AssetManifest::UnregisterAsset(const UUID& uuid)
    {
        auto findIter = m_UuidToFilepath.find(uuid);
        if (findIter != m_UuidToFilepath.end())
        {
            m_FilepathToUuid.erase(findIter->second);
            m_UuidToFilepath.erase(uuid);
        }
    }

    void AssetManifest::Serialize(const Ref<AssetManifest>& manifest, const Path& filepath, const Path& relativeTo)
    {
        Ref<AssetManifest> copy = manifest;
        if (!relativeTo.empty())
        {
            copy = CreateRef<AssetManifest>(manifest->m_Name);
            for (auto& entry : manifest->m_FilepathToUuid)
            {
                Path relativePath = fs::relative(entry.first, relativeTo);
                copy->m_FilepathToUuid[relativePath] = entry.second;
            }

            for (auto& entry : manifest->m_UuidToFilepath)
            {
                Path relativePath = fs::relative(entry.second, relativeTo);
                copy->m_UuidToFilepath[entry.first] = relativePath;
            }
        }

        YAML::Emitter out;
        out << YAML::Comment("Crowny manfiest");
        out << YAML::BeginMap;
        out << YAML::Key << "Manifest" << YAML::Value << copy->m_Name;

        out << YAML::Key << "Assets" << YAML::Value;
        out << YAML::BeginSeq;
        for (auto uuidPath : copy->m_UuidToFilepath)
        {
            out << YAML::BeginMap;
            out << YAML::Key << uuidPath.first << YAML::Value << uuidPath.second;
            out << YAML::EndMap;
        }

        out << YAML::EndSeq << YAML::EndMap;
        FileSystem::WriteTextFile(filepath, out.c_str());
    }

    Ref<AssetManifest> AssetManifest::Deserialize(const Path& filepath, const Path& relativeTo)
    {
        Ref<AssetManifest> result = CreateRef<AssetManifest>();

        String text = FileSystem::OpenFile(filepath)->GetAsString();
        YAML::Node data = YAML::Load(text);
        auto manifestName = data["Manifest"];
        if (!manifestName)
            CW_ENGINE_WARN("Manifest {0} does not contain a manifest name.", filepath);
        else
            result->m_Name = manifestName.as<String>();
        auto assets = data["Assets"];
        if (!assets)
        {
            CW_ENGINE_WARN("Manifest {0} does not contain an asset key.", filepath);
            return nullptr;
        }
        for (auto asset : assets)
        {
            UUID id = asset["UUID"].as<UUID>();
            String path = asset["Path"].as<String>();
            result->m_FilepathToUuid[path] = id;
            result->m_UuidToFilepath[id] = path;
        }

        if (relativeTo.empty())
            return result;

        Ref<AssetManifest> copy = CreateRef<AssetManifest>(result->m_Name);
        for (auto& entry : result->m_FilepathToUuid)
        {
            Path absolutePath = relativeTo / entry.first;
            copy->m_FilepathToUuid[absolutePath] = entry.second;
        }

        for (auto& entry : result->m_UuidToFilepath)
        {
            Path absolutePath = relativeTo / entry.second;
            copy->m_UuidToFilepath[entry.first] = absolutePath;
        }
        return copy;
    }

} // namespace Crowny