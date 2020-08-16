#include "cwpch.h"

#include "Crowny/SceneManagement/Scene.h"
#include "Crowny/SceneManagement/SceneManager.h"
#include "Crowny/Ecs/Entity.h"
#include "Crowny/Ecs/Components.h"

namespace Crowny
{

	Scene::Scene(const std::string& name) : m_Name(name)
	{
		SceneManager::s_Scenes.push_back(this);
		m_SceneEntity = new Entity(m_Registry.create(), this);
		m_SceneEntity->AddComponent<TagComponent>(m_Name);
		m_SceneEntity->AddComponent<RelationshipComponent>();
	}

	void Scene::OnUpdate(Timestep ts)
	{
		//m_Registry.view<TransformComponent, CameraComponent>().each([](const auto& entity, auto& tc, auto& cc) { Renderer2D::BeginScene(cc. });
	}

	Entity Scene::CreateEntity(const std::string& name)
	{
		Entity e(m_Registry.create(), this);
		e.AddComponent<TransformComponent>();
		e.AddComponent<RelationshipComponent>(*m_SceneEntity);
		e.AddComponent<TagComponent>(name);
		return e;
	}

	void Scene::OnViewportResize(uint32_t width, uint32_t height)
	{
		m_ViewportWidth = width;
		m_ViewportHeight = height;

		m_Registry.view<CameraComponent>().each([&](auto& e, auto& cc) {
		if(!cc.FixedAspectRatio)
			cc.Camera.OnResize((float)width, (float)height);
		});
	}

}