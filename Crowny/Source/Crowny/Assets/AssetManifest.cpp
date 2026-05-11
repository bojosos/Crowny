#include "cwpch.h"

#include "Crowny/Assets/AssetManifest.h"

#include "Crowny/Common/FileSystem.h"
#include "Crowny/Common/Yaml.h"

#include "Crowny/Serialization/FileEncoder.h"

namespace Crowny
{

    AssetManifest::AssetManifest(const String& name) : m_Name(name) {}

    bool AssetManifest::UuidToFilepath(const UUID& uuid, Path& outPath) const
    {
        const auto findIter = m_UuidToFilepath.find(uuid);
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
        const auto findIter = m_FilepathToUuid.find(path);
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
        const auto findIter = m_FilepathToUuid.find(path);
        return findIter == m_FilepathToUuid.end();
    }

    bool AssetManifest::UuidExists(const UUID& uuid) const
    {
        const auto findIter = m_UuidToFilepath.find(uuid);
        return findIter != m_UuidToFilepath.end();
    }

    void AssetManifest::RegisterAsset(const UUID& uuid, const Path& path)
    {
        const auto findIter = m_UuidToFilepath.find(uuid);
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
            const auto findIter2 = m_FilepathToUuid.find(path);
            if (findIter2 != m_FilepathToUuid.end())
                m_UuidToFilepath.erase(findIter2->second);

            m_UuidToFilepath[uuid] = path;
            m_FilepathToUuid[path] = uuid;
        }
    }

    void AssetManifest::UnregisterAsset(const UUID& uuid)
    {
        CW_ENGINE_INFO("Unregister");
        const auto findIter = m_UuidToFilepath.find(uuid);
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
            for (const auto& entry : manifest->m_FilepathToUuid)
            {
                const Path relativePath = fs::relative(entry.first, relativeTo);
                copy->m_FilepathToUuid[relativePath] = entry.second;
            }

            for (const auto& entry : manifest->m_UuidToFilepath)
            {
                const Path relativePath = fs::relative(entry.second, relativeTo);
                copy->m_UuidToFilepath[entry.first] = relativePath;
            }
        }
        FileEncoder<AssetManifest, SerializerType::Yaml> encoder(filepath);
        encoder.Encode(copy);
    }

    Ref<AssetManifest> AssetManifest::Deserialize(const Path& filepath, const Path& relativeTo)
    {
        FileDecoder<AssetManifest, SerializerType::Yaml> decoder(filepath);
        Ref<AssetManifest> result = decoder.Decode();

        if (relativeTo.empty())
            return result;

        Ref<AssetManifest> copy = CreateRef<AssetManifest>(result->m_Name);
        for (const auto& entry : result->m_FilepathToUuid)
        {
            const Path absolutePath = relativeTo / entry.first;
            copy->m_FilepathToUuid[absolutePath] = entry.second;
        }

        for (const auto& entry : result->m_UuidToFilepath)
        {
            const Path absolutePath = relativeTo / entry.second;
            copy->m_UuidToFilepath[entry.first] = absolutePath;
        }
        return copy;
    }

} // namespace Crowny