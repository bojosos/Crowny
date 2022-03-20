#pragma once

#include "Crowny/Assets/Asset.h"
#include "Crowny/Assets/AssetManifest.h"

#include "Crowny/Common/Module.h"

#include "Crowny/Import/ImportOptions.h"

namespace Crowny
{
    class AssetManager : public Module<AssetManager>
    {
    public:
        Ref<Asset> Load(const Path& path, bool keepSourceData = false);

        template <class T> Ref<T> Load(const Path& filepath, bool keepSourceData = false)
        {
            return std::static_pointer_cast<T>(Load(filepath, keepSourceData));
        }

        Ref<Asset> LoadFromUUID(const UUID& uuid, bool keepSourceData = false);

        void Save(const Ref<Asset>& resource); // TODO: Compression
        void Save(const Ref<Asset>& resource, const Path& filepath);

        void RegisterAssetManifest(const Ref<AssetManifest>& manifest);
        void UnregisterAssetManifest(const Ref<AssetManifest>& manifest);

    private:
        Ref<Asset> Load(const UUID& uuid, const Path& filepath, bool keepSourceData);
        void GetFilepathFromUUID(const UUID& uuid, Path& outFilepath);
        bool GetUUIDFromFilepath(const Path& filepath, UUID& outUUID);

    private:
        UnorderedMap<UUID, Ref<Asset>> m_LoadedAssets;
        // UnorderedMap<UUID, WeakAssetHandle<Asset>> m_Handles;
        Vector<Ref<AssetManifest>> m_Manifests;
    };
} // namespace Crowny