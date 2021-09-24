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
        Ref<Asset> Load(const std::string& path, bool keepSourceData = false);

        template <class T> Ref<T> Load(const std::string& filepath, bool keepSourceData = false)
        {
            return std::static_pointer_cast<T>(Load(filepath, keepSourceData));
        }

        Ref<Asset> LoadFromUUID(const Uuid& uuid, bool keepSourceData = false);

        void Save(const Ref<Asset>& resource); // TODO: Compression
        void Save(const Ref<Asset>& resource, const std::string& filepath);

    private:
        Ref<Asset> Load(const Uuid& uuid, const std::string& filepath, bool keepSourceData);
        void GetFilepathFromUuid(const Uuid& uuid, std::string& outFilepath);
        bool GetUuidFromFilepath(const std::string& filepath, Uuid& outUuid);

    private:
        std::unordered_map<std::string, Ref<AssetManifest>> m_Manifests;

    public:
        Ref<Asset> Import(const std::string& filepath, const Ref<ImportOptions>& importOptions, const Uuid& uuid);
        Ref<AssetManifest>& ImportManifest(const std::string& path, const std::string& name);
        Ref<AssetManifest>& CreateManifest(const std::string& path, const std::string& name);
    };
} // namespace Crowny