#include "cwpch.h"

#include "Crowny/SceneManagement/Scene.h"
#include "Crowny/SceneManagement/SceneManager.h"

namespace Crowny
{

	Scene::Scene(const std::string& name) : m_Name(name)
	{
		//SceneManager::AddScene(this);
	}

	void Scene::AddEntity(const Ref<Entity>& entity)
	{
		m_Entities.push_back(entity);
	}

}