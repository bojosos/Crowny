#pragma once

#include "Crowny/SceneManagement/Scene.h"

namespace Crowny
{
	class SceneManager
	{
	public:
		static Scene* GetActiveScene();
		static void AddScene(Scene* scene);
		static void Shutdown();
		/*
		static Scene& LoadScene(uint32_t buildIndex);
		static Scene& LoadScene(const std::string& name);
		*/
	private:
		static uint32_t s_ActiveIndex;
		static std::vector<Scene*> s_Scenes;
		friend class Scene;
	};
}