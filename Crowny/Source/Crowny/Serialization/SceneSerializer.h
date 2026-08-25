#pragma once

#include "Crowny/Assets/AssetHandle.h"
#include "Crowny/Assets/AssetManager.h"
#include "Crowny/Ecs/Entity.h"
#include "Crowny/Scene/Scene.h"

namespace YAML
{
    class Emitter;
    class Node;
} // namespace YAML

namespace Crowny
{

    class SceneSerializer
    {
    public:
        static constexpr uint32_t FORMAT_VERSION = 8;

        SceneSerializer(const Ref<Scene>& scene);

        void Serialize(const Path& filepath);
        void SerializeEntity(YAML::Emitter& out, Entity entity);
        void SerializeBinary(const Path& filepath);

        template <typename AssetType> AssetHandle<AssetType> LoadAssetHandle(const UUID& assetUUID)
        {
            if (assetUUID == UUID::EMPTY)
                return {};
            return AssetManager::TryGet()->LoadFromUUID<AssetType>(assetUUID);
        }
        bool Deserialize(const Path& filepath);
        void DeserializeEntities(const YAML::Node& entitiesNode, uint32_t serializedSceneVersion = FORMAT_VERSION);
        bool DeserializeBinary(const Path& filepath);

    private:
        Ref<Scene> m_Scene;
    };
} // namespace Crowny
