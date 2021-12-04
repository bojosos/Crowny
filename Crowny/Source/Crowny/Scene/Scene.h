#pragma once

#include "Crowny/Common/Timestep.h"
#include "Crowny/Common/Uuid.h"

#include "Crowny/Renderer/EditorCamera.h"

#include <entt/entt.hpp>

class b2World;

namespace Crowny
{
    class Entity;
    class ComponentEditor;
    class SceneSerializer;
    class SceneRenderer;
    class ScriptRuntime;
    class CameraComponent;

    class Scene
    {
    public:
        Scene(const String& name = String());
        Scene(Scene& other);
        Scene& operator=(Scene& other);
        ~Scene();

        void OnViewportResize(uint32_t width, uint32_t height);

        Entity CreateEntity(const String& name = "");
        Entity CreateEntityWithUuid(const UUID& uuid, const String& name);
        Entity FindEntityByName(const String& name);
        Entity GetRootEntity();
        Entity GetEntityFromUuid(const UUID& uuid);

        const String& GetName() const { return m_Name; }
        const String& GetFilepath() const { return m_Filepath; }

        void OnRuntimeStart();
        void OnRuntimePause();
        void OnRuntimeStop();

        void OnUpdateRuntime(Timestep ts);
        void OnUpdateEditor(Timestep ts);

        Entity GetPrimaryCameraEntity();

        template<typename... Components>
        auto GetAllEntitiesWith()
        {
            return m_Registry.view<Components...>();
        }

    private:
        friend class ComponentEditor;
        friend class SceneRenderer;
        friend class SceneSerializer;
        friend class Entity;
        friend class ScriptRuntime;

        String m_Name;
        String m_Filepath;
        entt::registry m_Registry;
        uint32_t m_ViewportWidth = 0, m_ViewportHeight = 0;

        b2World* m_PhysicsWorld2D = nullptr;

        Entity* m_RootEntity;
    };
} // namespace Crowny
