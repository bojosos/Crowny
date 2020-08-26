#pragma once

#include "Crowny/Common/Timestep.h"

#include "Crowny/Renderer/Texture.h"

#include <entt/entt.hpp>

namespace Crowny
{

	class Entity;
	class ImGuiHierarchyWindow;
	class ImGuiInspectorWindow;

	class Scene
	{
	public:
		Scene(const std::string& name = std::string());
		~Scene();

		void OnUpdate(Timestep ts);
		void OnViewportResize(uint32_t width, uint32_t height);

		Entity CreateEntity(const std::string& name = "");

		friend class Entity;
		friend class ImGuiHierarchyWindow;
		friend class ImGuiInspectorWindow;

	private:
		uint32_t m_BuildIndex;
		std::string m_Name;
		std::string m_Filepath;
		bool m_IsLoaded;
		entt::registry m_Registry;
		uint32_t m_ViewportWidth = 0, m_ViewportHeight = 0;

		Entity* m_SceneEntity;
	};
}
