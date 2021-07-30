#include "cwpch.h"

#include "Crowny/Scene/SceneManager.h"
#include "Crowny/Scene/ScriptRuntime.h"

namespace Crowny
{
	uint32_t SceneManager::s_ActiveIndex;
	std::vector<Ref<Scene>> SceneManager::s_Scenes;

	Ref<Scene> SceneManager::GetActiveScene()
	{
		return s_Scenes[s_ActiveIndex];
	}

	void SceneManager::SetActiveScene(const Ref<Scene>& scene)
	{
		s_Scenes.push_back(scene);
		s_ActiveIndex = s_Scenes.size() - 1;
		ScriptRuntime::Init();
	}

	void SceneManager::Shutdown()
	{
		
	}

	void SceneManager::AddScene(const Ref<Scene>& scene)
	{
		s_Scenes.push_back(scene);
	}
}