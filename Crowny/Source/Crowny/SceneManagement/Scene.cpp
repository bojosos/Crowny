#include "cwpch.h"

#include "Crowny/SceneManagement/Scene.h"
#include "Crowny/SceneManagement/SceneManager.h"
#include "Crowny/Ecs/Entity.h"
#include "Crowny/Ecs/Components.h"
#include "Crowny/Renderer/Renderer2D.h"

#include <entt/entt.hpp>

namespace Crowny
{

	Scene::Scene(const std::string& name) : m_Name(name)
	{
		SceneManager::s_Scenes.push_back(this);
		m_SceneEntity = new Entity(m_Registry.create(), this);
		m_SceneEntity->AddComponent<TagComponent>(m_Name);
		m_SceneEntity->AddComponent<RelationshipComponent>();
	}

	Scene::~Scene()
	{
		delete m_SceneEntity;
	}

	void Scene::OnUpdate(Timestep ts)
	{
		m_Registry.group<TransformComponent>(entt::get<CameraComponent>).each([&](const auto& entity, auto& tc, auto& cc)
			{ 
				Renderer2D::Begin(cc.CameraObject.GetProjectionMatrix(), tc.Transform);
				auto group = m_Registry.group<TransformComponent>(entt::get<SpriteRendererComponent>);
				for (auto ee : group)
				{
					auto& [transform, sprite] = m_Registry.get<TransformComponent, SpriteRendererComponent>(ee);
					Renderer2D::FillRect(transform, sprite.Texture, Color::FromRGBA(sprite.Color));
				}

				Renderer2D::End();
			});
	}

	Entity Scene::CreateEntity(const std::string& name)
	{
		m_SceneEntity->AddChild(name);
		return m_SceneEntity->GetComponent<RelationshipComponent>().Children.back();
	}

	void Scene::OnViewportResize(uint32_t width, uint32_t height)
	{
		m_ViewportWidth = width;
		m_ViewportHeight = height;

		m_Registry.view<CameraComponent>().each([&](auto& e, auto& cc) {
		//if(!cc.FixedAspectRatio)
			//cc.Camera.OnResize((float)width, (float)height);
		});
	}

}