#pragma once

#include "Crowny/Assets/Asset.h"
#include "Crowny/Assets/AssetHandle.h"
#include "Crowny/Common/Timestep.h"
#include "Crowny/Common/Uuid.h"
#include "Crowny/Physics/Physics3DTypes.h"

#include <entt/entt.hpp>

namespace Crowny
{
    class Entity;
    class EnttEntity;
    class ComponentEditor;
    class EnvironmentMap;
    class SceneSerializer;
    class PrefabSerializer;
    class SceneRenderer;
    class ScriptRuntime;
    struct CameraComponent;
    struct PersistedScriptState;
    struct ScriptTypeIdentity;

    class Scene : public Asset
    {
    public:
        AssetType GetAssetType() const override { return AssetType::Scene; }
        static AssetType GetStaticType() { return AssetType::Scene; }

        Scene(bool createRootEntity = true);
        Scene(const String& name, bool createRootEntity = true);
        Scene(const Scene& other);
        Scene& operator=(const Scene& other);
        ~Scene();

        void CreateRootEntity();
        Entity DuplicateEntity(Entity entity, bool includeChildren = true);

        void OnViewportResize(uint32_t width, uint32_t height);

        Entity CreateEntity(const String& name = "");
        Entity CreateEntityWithUuid(const UUID& uuid, const String& name);
        void DestroyEntity(Entity entity);
        Entity FindEntityByName(const String& name) const;
        Entity GetRootEntity() const;
        Entity TryGetEntityFromUuid(const UUID& uuid) const;
        Entity GetEntityFromUuid(const UUID& uuid) const;

        const String& GetName() const { return m_Name; }
        void SetName(const String& name) { m_Name = name; }
        const Path& GetFilepath() const { return m_Filepath; }

        void SetEnvironment(const Ref<EnvironmentMap>& env) { m_Environment = env; }
        const Ref<EnvironmentMap>& GetEnvironment() const { return m_Environment; }

        bool IsEditorScene() const { return m_IsEditorScene; }
        void SetEditorScene(bool isEditor) { m_IsEditorScene = isEditor; }
        bool IsRuntimeActive() const { return m_RuntimeActive; }
        bool IsSimulating() const { return m_SimulationActive; }

        const String& GetImGuiLayout() const { return m_ImGuiLayout; }
        void SetImGuiLayout(const String& layout) { m_ImGuiLayout = layout; }

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

        void RecreatePhysics3DBody(Entity entity);
        void RecreatePhysics3DShapes(Entity entity);
        void UpdatePhysics3DTransform(Entity entity);

        Entity GetPrimaryCameraEntity();

        bool HasScriptComponent(Entity entity, const ScriptTypeIdentity& identity) const;
        bool HasScriptComponent(Entity entity, const String& namespaceName, const String& typeName) const;
        bool AddScriptComponent(Entity entity, const ScriptTypeIdentity& identity, bool initialize = true);
        bool AddScriptComponent(Entity entity, const PersistedScriptState& state, bool initialize = true);
        bool AddScriptComponent(Entity entity, const String& namespaceName, const String& typeName, bool initialize = true);
        void RemoveScriptComponent(Entity entity, const ScriptTypeIdentity& identity);
        void RemoveScriptComponent(Entity entity, const String& namespaceName, const String& typeName);

        template <typename... Components> auto GetAllEntitiesWith() { return m_Registry.view<Components...>(); }
        template <typename... Components> auto GetAllEntitiesWith() const { return m_Registry.view<const Components...>(); }

    private:
        Entity CreateEntityInternal(const UUID& uuid, const String& name, Entity parent);
        Entity DuplicateEntityInternal(Entity entity, bool includeChildren, Entity cloneParent);
        void RegisterEntityCallbacks();
        void RebuildCopiedRelationships(const Scene& source, const UnorderedMap<UUID, entt::entity>& entityMap);

        void OnRigidbody2DComponentConstruct(entt::registry& registry, entt::entity entity);
        void OnRigidbody2DComponentUpdate(entt::registry& registry, entt::entity entity);
        void OnRigidbody2DComponentDestroy(entt::registry& registry, entt::entity entity);
        void OnBoxCollider2DComponentConstruct(entt::registry& registry, entt::entity entity);
        void OnBoxCollider2DComponentUpdate(entt::registry& registry, entt::entity entity);
        void OnBoxCollider2DComponentDestroy(entt::registry& registry, entt::entity entity);
        void OnCircleCollider2DComponentConstruct(entt::registry& registry, entt::entity entity);
        void OnCircleCollider2DComponentUpdate(entt::registry& registry, entt::entity entity);
        void OnCircleCollider2DComponentDestroy(entt::registry& registry, entt::entity entity);

        void OnRigidbody3DComponentConstruct(entt::registry& registry, entt::entity entity);
        void OnRigidbody3DComponentUpdate(entt::registry& registry, entt::entity entity);
        void OnRigidbody3DComponentDestroy(entt::registry& registry, entt::entity entity);
        void OnBoxCollider3DComponentConstruct(entt::registry& registry, entt::entity entity);
        void OnBoxCollider3DComponentUpdate(entt::registry& registry, entt::entity entity);
        void OnBoxCollider3DComponentDestroy(entt::registry& registry, entt::entity entity);
        void OnSphereCollider3DComponentConstruct(entt::registry& registry, entt::entity entity);
        void OnSphereCollider3DComponentUpdate(entt::registry& registry, entt::entity entity);
        void OnSphereCollider3DComponentDestroy(entt::registry& registry, entt::entity entity);
        void OnCapsuleCollider3DComponentConstruct(entt::registry& registry, entt::entity entity);
        void OnCapsuleCollider3DComponentUpdate(entt::registry& registry, entt::entity entity);
        void OnCapsuleCollider3DComponentDestroy(entt::registry& registry, entt::entity entity);

        bool BeginPhysics3D();
        void EndPhysics3D();
        void StepPhysics3D(Timestep ts);
        PhysicsBody3DHandle CreatePhysics3DBody(Entity entity);
        void DestroyPhysics3DBody(entt::entity entity);
        void QueuePhysics3DRebuild(entt::entity entity);
        void CreatePhysics3DShapes(Entity entity, PhysicsBody3DHandle body);
        void DestroyPhysics3DShapes(Entity entity, PhysicsBody3DHandle body);
        void HandlePhysics3DContact(const PhysicsContactEvent3D& event);

        void OnAudioSourceComponentConstruct(entt::registry& registry, entt::entity entity);
        void OnAudioSourceComponentUpdate(entt::registry& registry, entt::entity entity);
        void OnAudioSourceComponentDestroy(entt::registry& registry, entt::entity entity);

        void OnTransformComponentDestroy(entt::registry& registry, entt::entity entity);
        void OnMonoScriptComponentDestroy(entt::registry& registry, entt::entity entity);

    private:
        friend class ComponentEditor;
        friend class SceneRenderer;
        friend class SceneSerializer;
        friend class PrefabSerializer;
        friend class Entity;
        friend class EnttEntity;
        bool m_IsEditorScene = false;

        String m_Name;
        Path m_Filepath;
        String m_ImGuiLayout;
        entt::registry m_Registry;
        uint32_t m_ViewportWidth = 0, m_ViewportHeight = 0;

        Entity* m_RootEntity = nullptr;
        UnorderedMap<UUID, entt::entity> m_EntityMap;
        Ref<EnvironmentMap> m_Environment;
        UnorderedMap<entt::entity, PhysicsBody3DHandle> m_Physics3DBodies;
        UnorderedMap<PhysicsBody3DHandle, entt::entity> m_Physics3DEntities;
        UnorderedMap<entt::entity, glm::vec3> m_Physics3DScales;
        Vector<entt::entity> m_PendingPhysics3DRebuilds;
        Vector<PhysicsContactEvent3D> m_PendingPhysics3DContacts;
        Vector<PhysicsContactEvent3D> m_DispatchPhysics3DContacts;
        bool m_Physics3DActive = false;
        bool m_Physics2DActive = false;
        bool m_RuntimeActive = false;
        bool m_SimulationActive = false;
    };
} // namespace Crowny
