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
		Scene(const Scene& other);
		~Scene();

		void OnViewportResize(uint32_t width, uint32_t height);

		Entity CreateEntity(const std::string& name = "");
		Entity CreateEntity(const Uuid& uuid, const std::string& name);
		Entity GetEntity(const Uuid& uuid);
		Uuid& GetUuid(Entity entity);
		Entity GetRootEntity();

		const std::string& GetName() const { return m_Name; }
		const std::string& GetFilepath() const { return m_Filepath; }

		// These should not be here. A scene is just a bunch of data. Nothing to do with time
		float GetTime() { return 0.0f; }
		float GetDeltaTime() { return 0.0f; }
		float GetFrameCount() { return 0.0f; }
		float GetFixedDeltaTime() { return 0.0f; }
		float GetRealtimeSinceStartup() { return 0.0f; /* Not correct! This returns time until last frame*/ };
		float GetSmoothDeltaTime() { return 0.0f; }

		friend class ImGuiComponentEditor;
		friend class SceneRenderer;
		friend class SceneSerializer;
		friend class Entity;
		friend class ScriptRuntime;
	private:
		bool m_HasChanged = true;
		uint32_t m_BuildIndex;
		std::string m_Name;
		std::string m_Filepath;
		bool m_IsLoaded;
		entt::registry m_Registry;
		uint32_t m_ViewportWidth = 0, m_ViewportHeight = 0;

		std::unordered_map<Uuid, Entity>* m_Entities;
		std::unordered_map<Entity, Uuid>* m_Uuids;
		Entity* m_RootEntity;
	};
}
