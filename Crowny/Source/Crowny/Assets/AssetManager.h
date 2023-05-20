#pragma once

#include "Crowny/Assets/Asset.h"
#include "Crowny/Assets/AssetHandle.h"
#include "Crowny/Assets/AssetManifest.h"

#include "Crowny/Common/Module.h"

#include "Crowny/Import/ImportOptions.h"

namespace Crowny
{
    class UIUtils; // This is an Editor class

    class AssetManager : public Module<AssetManager>
    {
        struct LoadedAssetData
        {
            LoadedAssetData() = default;
            LoadedAssetData(const WeakAssetHandle<Asset>& asset, uint32_t size) : Asset(asset), Size(size) {}

            WeakAssetHandle<Asset> Asset;
            uint32_t NumInternalRefs = 0;
            uint32_t Size = 0;
        };

    public:
        AssetHandle<Asset> Load(const Path& path, bool keepinternalRef = true, bool keepSourceData = false);

        template <class T>
        AssetHandle<T> Load(const Path& filepath, bool keepInternalRef = true, bool keepSourceData = false)
        {
            return static_asset_cast<T>(Load(filepath, keepSourceData));
        }

        AssetHandle<Asset> LoadFromUUID(const UUID42& uuid, bool keepInternalRef = true, bool keepSourceData = false);

        AssetHandle<Asset> GetAssetHandle(const UUID42& uuid);

        void Save(const Ref<Asset>& resource); // TODO: Compression
        void Save(const Ref<Asset>& resource, const Path& filepath);

        void RegisterAssetManifest(const Ref<AssetManifest>& manifest);
        void UnregisterAssetManifest(const Ref<AssetManifest>& manifest);

        AssetHandle<Asset> CreateAssetHandle(const Ref<Asset>& asset);
        AssetHandle<Asset> CreateAssetHandle(const Ref<Asset>& asset, const UUID42& uuid);
        void Release(AssetHandleBase& handle);

    private:
        AssetHandle<Asset> Load(const UUID42& uuid, const Path& filepath, bool keepInternalRef, bool keepSourceData);
        void GetFilepathFromUUID(const UUID42& uuid, Path& outFilepath);
        bool GetUUIDFromFilepath(const Path& filepath, UUID42& outUUID);

        const UnorderedMap<UUID42, WeakAssetHandle<Asset>>& GetLoadedAssets() const { return m_Handles; }
        friend class UIUtils;

    private:
        UnorderedMap<UUID42, Ref<Asset>> m_LoadedAssets;
        UnorderedMap<UUID42, WeakAssetHandle<Asset>> m_Handles;
        Vector<Ref<AssetManifest>> m_Manifests;
    };
} // namespace Crowny