#pragma once

#include "Crowny/Common/Timestep.h"

#include "Crowny/Renderer/Texture.h"

#include <entt/entt.hpp>

namespace Crowny
{
	class Entity;
	class ImGuiComponentEditor;

	class Scene
	{
	public:
		Scene(const std::string& name = std::string());
		Scene(const Scene& other);
		~Scene();

		void OnUpdate(Timestep ts);
		void OnViewportResize(uint32_t width, uint32_t height);

		void Run();

		Entity CreateEntity(const std::string& name = "");
		Entity& GetRootEntity();

		float GetTime() { return time; }
		float GetDeltaTime() { return deltaTime; }
		float GetFrameCount() { return frameCount; }
		float GetFixedDeltaTime() { return fixedDeltaTime; }
		float GetRealtimeSinceStartup() { return realtimeSinceStarup; /* Not correct! This returns time until last frame*/ };
		float GetSmoothDeltaTime() { return deltaTime + time / (frameCount + 1); }

		friend class ImGuiComponentEditor;
		friend class Entity;
	private:
		float deltaTime = 0.0f, frameCount = 0.0f, fixedDeltaTime = 0.0f, time = 0.0f, realtimeSinceStarup = 0.0f;

		bool m_Running = true;
		uint32_t m_BuildIndex;
		std::string m_Name;
		std::string m_Filepath;
		bool m_IsLoaded;
		entt::registry m_Registry;
		uint32_t m_ViewportWidth = 0, m_ViewportHeight = 0;

		Entity* m_RootEntity;
	};
}
