#pragma once

#include "Crowny/SceneManagement/Scene.h"

namespace Crowny
{
	class SceneManager
	{
		friend class Scene;
	public:
		static Ref<Scene> GetActiveScene() { return s_Scenes[s_ActiveIndex]; }
		static Ref<Scene> CreateScene(const std::string& name);
		/*
		static Scene& LoadScene(uint32_t buildIndex);
		static Scene& LoadScene(const std::string& name);
		*/
	private:
		static uint32_t s_ActiveIndex;
		static std::vector<Ref<Scene>> s_Scenes;
	};
}