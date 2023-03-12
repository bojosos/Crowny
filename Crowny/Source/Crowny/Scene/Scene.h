#pragma once

#include "Crowny/Common/Timestep.h"
#include "Crowny/Common/Uuid.h"

#include <entt/entt.hpp>

namespace Crowny
{
    class Entity;
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

    class Scene
    {
    public:
        Scene();
        Scene(const String& name);
        Scene(Scene& other);
        Scene& operator=(Scene& other);
        ~Scene();

        Entity DuplicateEntity(Entity entity, bool includeChildren = true);

        void OnViewportResize(uint32_t width, uint32_t height);

        Entity CreateEntity(const String& name = "");
        Entity CreateEntityWithUuid(const UUID& uuid, const String& name);
        Entity FindEntityByName(const String& name);
        Entity GetRootEntity();
        Entity GetEntityFromUuid(const UUID& uuid);

        const String& GetName() const { return m_Name; }
        const Path& GetFilepath() const { return m_Filepath; }

        void OnRuntimeStart();
        void OnRuntimePause();
        void OnRuntimeStop();

        void OnFixedUpdate(Timestep ts);

        void OnSimulationStart();
        void OnSimulationUpdate(Timestep ts);
        void OnSimulationEnd();

        void OnUpdateRuntime(Timestep ts);
        void OnUpdateEditor(Timestep ts);

        Entity GetPrimaryCameraEntity();

        template <typename... Components> auto GetAllEntitiesWith() { return m_Registry.view<Components...>(); }

    private:
        void RegisterEntityCallbacks();

        void OnRigidbody2DComponentConstruct(entt::registry& registry, entt::entity entity);
        void OnRigidbody2DComponentDestroy(entt::registry& registry, entt::entity entity);
        void OnBoxCollider2DComponentConstruct(entt::registry& registry, entt::entity entity);
        void OnBoxCollider2DComponentDestroy(entt::registry& registry, entt::entity entity);
        void OnCircleCollider2DComponentConstruct(entt::registry& registry, entt::entity entity);
        void OnCircleCollider2DComponentDestroy(entt::registry& registry, entt::entity entity);

        void OnAudioSourceComponentomponentConstruct(entt::registry& registry, entt::entity entity);
        void OnAudioSourceComponentComponentDestroy(entt::registry& registry, entt::entity entity);

    private:
        friend class ComponentEditor;
        friend class SceneRenderer;
        friend class SceneSerializer;
        friend class Entity;

        bool m_IsEditorScene = false;

        String m_Name;
        Path m_Filepath;
        entt::registry m_Registry;
        uint32_t m_ViewportWidth = 0, m_ViewportHeight = 0;

        Entity* m_RootEntity = nullptr;
    };
} // namespace Crowny
