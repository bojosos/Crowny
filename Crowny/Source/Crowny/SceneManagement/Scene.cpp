#include "cwpch.h"

#include "Crowny/SceneManagement/Scene.h"

namespace Crowny
{

	Scene::Scene(const std::string& name) : m_Name(name)
	{

	}

	void Scene::AddEntity(const Ref<Entity>& entity)
	{
		m_Entities.push_back(entity);
	}

}