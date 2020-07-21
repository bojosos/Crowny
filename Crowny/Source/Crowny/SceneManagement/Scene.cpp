#include "cwpch.h"

#include "Crowny/SceneManagement/Scene.h"
#include "Crowny/SceneManagement/SceneManager.h"

namespace Crowny
{

	Scene::Scene(const std::string& name) : m_Name(name)
	{
		
	}

	void Scene::OnUpdate(Timestep ts)
	{

	}

	Entity& Scene::CreateEntity()
	{
		return Entity(m_Registry.create());
	}

}