#pragma once

#include "Crowny/Assets/AssetHandle.h"
#include "Crowny/Ecs/Entity.h"
#include "Crowny/Scene/Scene.h"

namespace YAML
{
    class Emitter;
}

namespace Crowny
{

    class SceneSerializer
    {
    public:
        SceneSerializer(const Ref<Scene>& scene);

        void Serialize(const Path& filepath);
        void SerializeEntity(YAML::Emitter& out, Entity entity);
        void SerializeBinary(const Path& filepath);

        template <typename AssetType> AssetHandle<AssetType> LoadAssetHandle(const UUID& assetUUID)
        {
            if (assetUUID == UUID::EMPTY)
                return AssetHandle<AssetType>();

            TAssetHandleBase<false> handle;
            handle.m_Data = CreateRef<AssetHandleData>();
            handle.m_Data->m_RefCount.fetch_add(1, std::memory_order_relaxed);
            handle.m_Data->m_UUID = assetUUID;
            AssetHandle<Asset> loadedAsset = AssetManager::Get().LoadFromUUID(handle.m_Data->m_UUID);
            handle.Release();
            handle.m_Data = loadedAsset.m_Data;
            handle.AddRef();
            return static_asset_cast<AssetType>(loadedAsset);
        }
        void Deserialize(const Path& filepath);
        void DeserializeBinary(const Path& filepath);

    private:
        Ref<Scene> m_Scene;
    };
} // namespace Crowny