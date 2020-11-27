#pragma once

#include "Crowny/Scene/Scene.h"

namespace Crowny
{
	class SceneManager
	{
	public:
		static Ref<Scene> GetActiveScene();
		static void AddScene(const Ref<Scene>& scene);
		static void SetActiveScene(const Ref<Scene>& scene);
		static void Shutdown();
		/*
		static Scene& LoadScene(uint32_t buildIndex);
		static Scene& LoadScene(const std::string& name);
		*/
	private:
		static uint32_t s_ActiveIndex;
		static std::vector<Ref<Scene>> s_Scenes;
		friend class Scene;
	};
}