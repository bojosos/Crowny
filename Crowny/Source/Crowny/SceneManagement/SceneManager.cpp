#include "cwpch.h"

#include "Crowny/SceneManagement/SceneManager.h"

namespace Crowny
{
	uint32_t SceneManager::s_ActiveIndex;
	std::vector<Scene*> SceneManager::s_Scenes;

	Scene* SceneManager::GetActiveScene()
	{
		return s_Scenes[s_ActiveIndex];
	}

	void SceneManager::Shutdown()
	{
		for (auto* scene : s_Scenes)
			delete scene;
	}

	void SceneManager::AddScene(Scene* scene)
	{
		s_Scenes.push_back(scene);
	}
}