#pragma once

#include "Crowny/Assets/Asset.h"
#include "Crowny/Common/Uuid.h"

namespace Crowny
{
    class Scene;
    class Entity;

    class Prefab : public Asset
    {
    public:
        AssetType GetAssetType() const override { return AssetType::Prefab; }
        static AssetType GetStaticType() { return AssetType::Prefab; }

        Prefab();
        ~Prefab();

        void CaptureFromEntity(const Scene& sourceScene, const Entity& rootEntity);

        const Ref<Scene>& GetInternalScene() const { return m_PrefabScene; }
        Ref<Scene>& GetInternalScene() { return m_PrefabScene; }
        Entity GetRootEntity() const;
        const UUID& GetRootEntityUuid() const { return m_RootEntityUuid; }
        void SetRootEntityUuid(const UUID& uuid) { m_RootEntityUuid = uuid; }

    private:
        void CaptureEntityRecursive(Scene& source, Entity sourceEntity, Scene& dest, Entity* destParent);

        Ref<Scene> m_PrefabScene;
        UUID m_RootEntityUuid;
    };
} // namespace Crowny
