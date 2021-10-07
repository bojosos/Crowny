#pragma once

#include "Crowny/Common/Timestep.h"
#include "Crowny/Common/Uuid.h"

#include "Crowny/Renderer/EditorCamera.h"

#include <entt/entt.hpp>

namespace Crowny
{
    class Entity;
    class ImGuiComponentEditor;
    class SceneSerializer;
    class SceneRenderer;
    class ScriptRuntime;

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

        friend class ImGuiComponentEditor;
        friend class SceneRenderer;
        friend class SceneSerializer;
        friend class Entity;
        friend class ScriptRuntime;

    private:
        String m_Name;
        String m_Filepath;
        entt::registry m_Registry;
        uint32_t m_ViewportWidth = 0, m_ViewportHeight = 0;

        Entity* m_RootEntity;
    };
} // namespace Crowny
