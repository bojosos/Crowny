#include "cwpch.h"

#include "Crowny/Assets/AssetManager.h"

#include "Crowny/Assets/AssetCodecs.h"
#include "Crowny/Assets/AssetListener.h"
#include "Crowny/Common/FileSystem.h"
#include "Crowny/Physics/PhysicsMaterial.h"
#include "Crowny/RenderAPI/Texture.h"
#include "Crowny/Renderer/Font.h"
#include "Crowny/Serialization/CerealDataStreamArchive.h"

#include <tracy/Tracy.hpp>

namespace Crowny
{
    void AssetManager::OnStartUp() { InitializeAssetCodecs(); }

    void AssetManager::OnShutdown()
    {
        m_Handles.clear();
        m_TransientAssetIds.clear();
        m_Manifests.clear();
    }

    AssetHandle<Asset> AssetManager::Load(const Path& filepath, bool keepInternalRef, bool keepSourceData)
    {
        ZoneScopedN("AssetManager::Load");
        const Path normalizedPath = filepath.lexically_normal();
        if (!FileSystem::FileExists(normalizedPath))
        {
            CW_ENGINE_WARN("Resource {0} does not exist or is not a regular file.", normalizedPath);
            return nullptr;
        }

        UUID uuid;
        if (!GetUUIDFromFilepath(normalizedPath, uuid))
        {
            const auto transientIter = m_TransientAssetIds.find(normalizedPath);
            if (transientIter != m_TransientAssetIds.end())
                uuid = transientIter->second;
            else
            {
                uuid = UuidGenerator::Generate();
                m_TransientAssetIds[normalizedPath] = uuid;
            }
        }
        if (uuid.Empty())
            uuid = UuidGenerator::Generate();
        return Load(uuid, normalizedPath, keepInternalRef, keepSourceData);
    }

    AssetHandle<Asset> AssetManager::LoadFromUUID(const UUID& uuid, bool keepInternalRef, bool keepSourceData)
    {
        const auto iterFind = m_Handles.find(uuid);
        if (iterFind != m_Handles.end() && iterFind->second.IsLoaded())
        {
            AssetHandle<Asset> loaded = iterFind->second.Lock();
            if (keepInternalRef)
                loaded.AddInternalRef();
            return loaded;
        }
        Path filepath;
        GetFilepathFromUUID(uuid, filepath);
        if (filepath.empty() || !FileSystem::FileExists(filepath))
        {
            return AssetHandle<Asset>();
        }
        return Load(uuid, filepath, keepInternalRef, keepSourceData);
    }

    AssetHandle<Asset> AssetManager::Load(const UUID& uuid, const Path& filepath, bool keepInternalRef, bool keepSourceData)
    {
        const auto iterFind = m_Handles.find(uuid);
        if (iterFind != m_Handles.end() && iterFind->second.IsLoaded())
        {
            AssetHandle<Asset> loaded = iterFind->second.Lock();
            if (keepInternalRef)
                loaded.AddInternalRef();
            return loaded;
        }

        Ref<Asset> asset;
        try
        {
            const Ref<DataStream> stream = FileSystem::OpenFile(filepath);
            if (stream == nullptr || !stream->IsReadable())
            {
                CW_ENGINE_ERROR("Unable to open asset '{}'.", filepath);
                return nullptr;
            }

            BinaryDataStreamInputArchive archive(stream);
            archive(asset);
            stream->Close();
        }
        catch (const std::exception& error)
        {
            CW_ENGINE_ERROR("Failed to load asset '{}': {}", filepath, error.what());
            return nullptr;
        }

        if (asset == nullptr)
        {
            CW_ENGINE_ERROR("Asset '{}' contained a null payload.", filepath);
            return nullptr;
        }

        if (!keepSourceData && asset->GetAssetType() == AssetType::Texture)
        {
            const Ref<Texture> texture = StaticRefCast<Texture>(asset);
            if (!texture->IsCpuCached())
                texture->ReleaseSourceData();
        }

        AssetHandle<Asset> output;
        if (iterFind != m_Handles.end())
        {
            output = iterFind->second.Lock();
            output.SetHandleData(asset, uuid);
        }
        else
            output = AssetHandle<Asset>(asset, uuid);

        if (keepInternalRef)
            output.AddInternalRef();
        output.NotifyLoadComplete();
        m_Handles[uuid] = output.GetWeak();
        if (asset->GetAssetType() == AssetType::Font)
            StaticRefCast<Font>(asset)->LoadFallbackFonts();
        if (AssetListenerManager::IsStartedUp())
            AssetListenerManager::Get().NotifyListeners(uuid);
        if (asset->GetAssetType() == AssetType::PhysicsMaterial2D)
            StaticRefCast<PhysicsMaterial2D>(asset)->NotifyChanged();
        else if (asset->GetAssetType() == AssetType::PhysicsMaterial)
            StaticRefCast<PhysicsMaterial3D>(asset)->NotifyChanged();
        return output;
    }

    AssetHandle<Asset> AssetManager::GetAssetHandle(const UUID& uuid)
    {
        const auto iterFind = m_Handles.find(uuid);
        if (iterFind != m_Handles.end())
            return iterFind->second.Lock();
        AssetHandle<Asset> handle(uuid);
        m_Handles[uuid] = handle.GetWeak();
        return handle;
    }

    bool AssetManager::Save(const AssetHandle<Asset>& asset, const Path& filepath, bool overwrite)
    {
        if (!asset)
            return false;

        if (fs::exists(filepath) && !overwrite)
        {
            CW_ENGINE_ERROR("File exists, not saving");
            return false;
        }

        return Save(asset.GetInternalPtr(), filepath);
    }

    bool AssetManager::Save(const Ref<Asset>& asset, const Path& filepath)
    {
        if (asset == nullptr || filepath.empty())
        {
            CW_ENGINE_ERROR("Cannot save a null asset or use an empty filepath.");
            return false;
        }

        try
        {
            const Ref<MemoryDataStream> stream = CreateRef<MemoryDataStream>(4096);
            {
                BinaryDataStreamOutputArchive archive(stream);
                archive(asset);
            }
            String writeError;
            if (!FileSystem::WriteFileAtomic(filepath, stream->Data(), stream->Tell(), &writeError))
            {
                CW_ENGINE_ERROR("Unable to publish asset '{}': {}", filepath, writeError);
                return false;
            }

            UUID uuid;
            if (GetUUIDFromFilepath(filepath.lexically_normal(), uuid))
            {
                const auto handleIter = m_Handles.find(uuid);
                if (handleIter != m_Handles.end())
                {
                    AssetHandle<Asset> loaded = handleIter->second.Lock();
                    loaded.SetHandleData(asset, uuid);
                    loaded.NotifyLoadComplete();
                }
                if (AssetListenerManager::IsStartedUp())
                    AssetListenerManager::Get().NotifyListeners(uuid);
            }
            return true;
        }
        catch (const std::exception& error)
        {
            CW_ENGINE_ERROR("Failed to save asset '{}': {}", filepath, error.what());
            return false;
        }
    }

    void AssetManager::RegisterAssetManifest(const Ref<AssetManifest>& manifest)
    {
        if (manifest == nullptr)
            return;
        const auto iterFind = std::find(m_Manifests.begin(), m_Manifests.end(), manifest);
        if (iterFind == m_Manifests.end())
            m_Manifests.push_back(manifest);
        else
            *iterFind = manifest;
    }

    void AssetManager::UnregisterAssetManifest(const Ref<AssetManifest>& manifest)
    {
        const auto iterFind = std::find(m_Manifests.begin(), m_Manifests.end(), manifest);
        if (iterFind != m_Manifests.end())
            m_Manifests.erase(iterFind);
    }

    bool AssetManager::GetAssetPath(const UUID& uuid, Path& outPath) const
    {
        for (const auto& manifest : m_Manifests)
        {
            if (manifest->UuidToFilepath(uuid, outPath))
                return true;
        }
        outPath.clear();
        return false;
    }

    bool AssetManager::IsAssetRegistered(const UUID& uuid) const
    {
        for (const Ref<AssetManifest>& manifest : m_Manifests)
        {
            if (manifest && manifest->UuidExists(uuid))
                return true;
        }
        return false;
    }

    void AssetManager::GetFilepathFromUUID(const UUID& uuid, Path& outFilepath) const
    {
        outFilepath.clear();
        for (const auto& manifest : m_Manifests)
        {
            if (manifest->UuidToFilepath(uuid, outFilepath))
                return;
        }
    }

    bool AssetManager::GetUUIDFromFilepath(const Path& filepath, UUID& outUUID) const
    {
        for (const auto& manifest : m_Manifests)
        {
            if (manifest->FilepathToUuid(filepath, outUUID))
                return true;
        }
        // No manifest has this filepath registered â€” caller will generate a new UUID.
        // This happens for assets loaded directly by path that weren't imported through the ProjectLibrary.
        return false;
    }

    AssetHandle<Asset> AssetManager::CreateAssetHandle(const Ref<Asset>& asset)
    {
        const UUID uuid = UuidGenerator::Generate();
        return CreateAssetHandle(asset, uuid);
    }

    AssetHandle<Asset> AssetManager::CreateAssetHandle(const Ref<Asset>& asset, const UUID& uuid)
    {
        if (asset == nullptr || uuid.Empty())
            return nullptr;

        const auto existing = m_Handles.find(uuid);
        if (existing != m_Handles.end())
        {
            AssetHandle<Asset> handle = existing->second.Lock();
            handle.SetHandleData(asset, uuid);
            handle.NotifyLoadComplete();
            return handle;
        }

        const AssetHandle<Asset> newHandle(asset, uuid);
        m_Handles[uuid] = newHandle.GetWeak();
        return newHandle;
    }

    void AssetManager::Release(AssetHandleBase& handle)
    {
        const UUID uuid = handle.GetUUID();
        const Ref<AssetHandleData> data = handle.GetHandleData();
        const auto iter = m_Handles.find(uuid);
        if (iter != m_Handles.end() && iter->second.GetHandleData() == data)
            m_Handles.erase(iter);

        handle.ClearHandleData();
    }

    // ---- NodeGraph Serialization ----
} // namespace Crowny
