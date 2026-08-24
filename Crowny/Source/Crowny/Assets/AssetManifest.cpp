#include "cwpch.h"

#include "Crowny/Assets/AssetManifest.h"

#include "Crowny/Common/FileSystem.h"
#include "Crowny/Common/Yaml.h"

#include "Crowny/Serialization/FileEncoder.h"

namespace Crowny
{
    namespace
    {
        Path NormalizeManifestPath(const Path& path) { return path.lexically_normal(); }
    } // namespace

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
        const auto findIter = m_FilepathToUuid.find(NormalizeManifestPath(path));
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
        const auto findIter = m_FilepathToUuid.find(NormalizeManifestPath(path));
        return findIter != m_FilepathToUuid.end();
    }

    bool AssetManifest::UuidExists(const UUID& uuid) const
    {
        const auto findIter = m_UuidToFilepath.find(uuid);
        return findIter != m_UuidToFilepath.end();
    }

    void AssetManifest::RegisterAsset(const UUID& uuid, const Path& path)
    {
        if (uuid.Empty() || path.empty())
        {
            CW_ENGINE_WARN("Cannot register an asset with an empty UUID or filepath in manifest '{}'.", m_Name);
            return;
        }

        const Path normalizedPath = NormalizeManifestPath(path);

        const auto uuidIter = m_UuidToFilepath.find(uuid);
        if (uuidIter != m_UuidToFilepath.end() && uuidIter->second != normalizedPath)
            m_FilepathToUuid.erase(uuidIter->second);

        const auto pathIter = m_FilepathToUuid.find(normalizedPath);
        if (pathIter != m_FilepathToUuid.end() && pathIter->second != uuid)
            m_UuidToFilepath.erase(pathIter->second);

        m_UuidToFilepath[uuid] = normalizedPath;
        m_FilepathToUuid[normalizedPath] = uuid;
    }

    void AssetManifest::UnregisterAsset(const UUID& uuid)
    {
        const auto findIter = m_UuidToFilepath.find(uuid);
        if (findIter != m_UuidToFilepath.end())
        {
            m_FilepathToUuid.erase(findIter->second);
            m_UuidToFilepath.erase(uuid);
        }
    }

    void AssetManifest::Serialize(const Ref<AssetManifest>& manifest, const Path& filepath, const Path& relativeTo)
    {
        if (manifest == nullptr)
        {
            CW_ENGINE_ERROR("Cannot serialize a null asset manifest to '{}'.", filepath);
            return;
        }

        Ref<AssetManifest> copy = manifest;
        if (!relativeTo.empty())
        {
            copy = CreateRef<AssetManifest>(manifest->m_Name);
            for (const auto& entry : manifest->m_UuidToFilepath)
            {
                std::error_code error;
                const Path relativePath = fs::relative(entry.second, relativeTo, error);
                copy->RegisterAsset(entry.first, error ? entry.second : relativePath);
            }
        }
        FileEncoder<AssetManifest, SerializerType::Yaml> encoder(filepath);
        encoder.Encode(copy);
    }

    Ref<AssetManifest> AssetManifest::Deserialize(const Path& filepath, const Path& relativeTo)
    {
        Ref<AssetManifest> result;
        try
        {
            FileDecoder<AssetManifest, SerializerType::Yaml> decoder(filepath);
            result = decoder.Decode();
        }
        catch (const std::exception& error)
        {
            CW_ENGINE_ERROR("Failed to deserialize asset manifest '{}': {}", filepath, error.what());
            return nullptr;
        }

        if (result == nullptr)
        {
            CW_ENGINE_ERROR("Asset manifest '{}' did not contain a valid manifest.", filepath);
            return nullptr;
        }

        if (relativeTo.empty())
            return result;

        Ref<AssetManifest> copy = CreateRef<AssetManifest>(result->m_Name);
        for (const auto& entry : result->m_UuidToFilepath)
            copy->RegisterAsset(entry.first, entry.second.is_absolute() ? entry.second : relativeTo / entry.second);
        return copy;
    }

} // namespace Crowny
