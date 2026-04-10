#pragma once

#include "Crowny/Assets/Asset.h"
#include "Crowny/Common/Timestep.h"
#include "Crowny/Common/Uuid.h"

#include <entt/entt.hpp>

namespace Crowny
{
    class Entity;
    class EnttEntity;
    class ComponentEditor;
    class SceneSerializer;
    class SceneRenderer;
    class ScriptRuntime;
    struct CameraComponent;

    struct Collision2D
    {
        Vector<glm::vec2> Points;
        Vector<Entity> Colliders;
    };

    class Scene : public Asset
    {
    public:
        AssetType GetAssetType() const override { return AssetType::Scene; }
        static AssetType GetStaticType() { return AssetType::Scene; }

        Scene(bool createRootEntity = true);
        Scene(const String& name, bool createRootEntity = true);
        Scene(Scene& other);
        Scene& operator=(Scene& other);
        ~Scene();

        void CreateRootEntity();
        Entity DuplicateEntity(Entity entity, bool includeChildren = true);

        void OnViewportResize(uint32_t width, uint32_t height);

        Entity CreateEntity(const String& name = "");
        Entity CreateEntityWithUuid(const UUID& uuid, const String& name);
        void DestroyEntity(Entity entity);
        Entity FindEntityByName(const String& name) const;
        Entity GetRootEntity() const;
        Entity GetEntityFromUuid(const UUID& uuid) const;

        const String& GetName() const { return m_Name; }
        void SetName(const String& name) { m_Name = name; }
        const Path& GetFilepath() const { return m_Filepath; }

        bool IsEditorScene() const { return m_IsEditorScene; }
        void SetEditorScene(bool isEditor) { m_IsEditorScene = isEditor; }

        void OnRuntimeStart();
        void OnRuntimePause();
        void OnRuntimeResume();
        void OnRuntimeStop();

        void OnFixedUpdate(Timestep ts);

        void OnSimulationStart();
        void OnSimulationUpdate(Timestep ts);
        void OnSimulationEnd();

        void OnUpdateRuntime(Timestep ts);
        void OnUpdateEditor(Timestep ts);

        Entity GetPrimaryCameraEntity() const;

        bool HasScriptComponent(Entity entity, const String& namespaceName, const String& typeName) const;
        void AddScriptComponent(Entity entity, const String& namespaceName, const String& typeName, bool initialize = true);
        void RemoveScriptComponent(Entity entity, const String& namespaceName, const String& typeName);

        template <typename... Components> auto GetAllEntitiesWith() { return m_Registry.view<Components...>(); }
        template <typename... Components> auto GetAllEntitiesWith() const { return m_Registry.view<const Components...>(); }

    private:
        void RegisterEntityCallbacks();

        void OnRigidbody2DComponentConstruct(entt::registry& registry, entt::entity entity);
        void OnRigidbody2DComponentDestroy(entt::registry& registry, entt::entity entity);
        void OnBoxCollider2DComponentConstruct(entt::registry& registry, entt::entity entity);
        void OnBoxCollider2DComponentDestroy(entt::registry& registry, entt::entity entity);
        void OnCircleCollider2DComponentConstruct(entt::registry& registry, entt::entity entity);
        void OnCircleCollider2DComponentDestroy(entt::registry& registry, entt::entity entity);

        void OnAudioSourceComponentConstruct(entt::registry& registry, entt::entity entity);
        void OnAudioSourceComponentDestroy(entt::registry& registry, entt::entity entity);

        void OnTransformComponentDestroy(entt::registry& registry, entt::entity entity);
        void OnMonoScriptComponentDestroy(entt::registry& registry, entt::entity entity);

    private:
        friend class ComponentEditor;
        friend class SceneRenderer;
        friend class SceneSerializer;
        friend class Entity;
        friend class EnttEntity;
        bool m_IsEditorScene = false;

        String m_Name;
        Path m_Filepath;
        entt::registry m_Registry;
        uint32_t m_ViewportWidth = 0, m_ViewportHeight = 0;

        Entity* m_RootEntity = nullptr;
        UnorderedMap<UUID, entt::entity> m_EntityMap;
    };
} // namespace Crowny
