#pragma once

#include "Crowny/Common/Timestep.h"
#include "Crowny/Common/Uuid.h"

#include "Crowny/Renderer/EditorCamera.h"
#include "Crowny/Renderer/Framebuffer.h"

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
		Scene(const std::string& name = std::string());
		Scene(const Scene& other) = delete;
    Scene& operator=(const Scene& other)
    {
      return *this;
    }
		~Scene();

		void OnViewportResize(uint32_t width, uint32_t height);

		Entity CreateEntity(const std::string& name = "");
		Entity CreateEntity(const Uuid& uuid, const std::string& name);
		Entity GetEntity(const Uuid& uuid);
		Entity FindEntityByName(const std::string& name);
		Entity GetRootEntity();
		Uuid& GetUuid(Entity entity);

		const std::string& GetName() const { return m_Name; }
		const std::string& GetFilepath() const { return m_Filepath; }

		friend class ImGuiComponentEditor;
		friend class SceneRenderer;
		friend class SceneSerializer;
		friend class Entity;
		friend class ScriptRuntime;
	private:
		bool m_HasChanged = true;
		std::string m_Name;
		std::string m_Filepath;
		entt::registry m_Registry;
		uint32_t m_ViewportWidth = 0, m_ViewportHeight = 0;

		std::unordered_map<Uuid, Entity>* m_Entities;
		std::unordered_map<Entity, Uuid>* m_Uuids;
		Entity* m_RootEntity;
	};
}
