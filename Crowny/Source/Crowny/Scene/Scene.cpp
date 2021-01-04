#include "cwpch.h"

#include "Crowny/Ecs/Entity.h"
#include "Crowny/Scene/Scene.h"
#include "Crowny/Scene/SceneManager.h"
#include "Crowny/Ecs/Components.h"
#include "Crowny/Renderer/Renderer.h"
#include "Crowny/Renderer/Renderer2D.h"
#include "Crowny/Renderer/RenderCommand.h"
#include "Crowny/Renderer/ForwardRenderer.h"
#include "Crowny/Renderer/ForwardPlusRenderer.h"
#include "Crowny/Renderer/IDBufferRenderer.h"

#include "Crowny/Scripting/Bindings/Scene/ScriptComponent.h"

#include <entt/entt.hpp>

namespace Crowny
{

	Scene::Scene(const std::string& name) : m_Name(name), m_HasChanged(true)
	{
		m_RootEntity = new Entity(m_Registry.create(), this);
		
		m_RootEntity->AddComponent<TagComponent>(m_Name);
		m_RootEntity->AddComponent<RelationshipComponent>();
		m_Entities = new std::unordered_map<Uuid, Entity>();
		m_Uuids = new std::unordered_map<Entity, Uuid>();
	}

	Scene::Scene(const Scene& other)
	{
		m_Name = other.m_Name;
		m_RootEntity = other.m_RootEntity;
		//m_Registry = other.m_Registry.clone();
		m_Entities = new std::unordered_map<Uuid, Entity>();
		m_Uuids = new std::unordered_map<Entity, Uuid>();
	}

	Scene::~Scene()
	{
		delete m_RootEntity;
	}
	
	Entity Scene::CreateEntity(const std::string& name)
	{
		Entity entity = { m_Registry.create(), this };
		Uuid tmp = UuidGenerator::Generate();
		(*m_Entities)[tmp] = entity;
		(*m_Uuids)[entity] = tmp;
		
		entity.AddComponent<TagComponent>(name);
		entity.AddComponent<TransformComponent>();
		entity.AddComponent<RelationshipComponent>(*m_RootEntity);
		m_HasChanged = true;

		return entity;
	}

	Entity Scene::CreateEntity(const Uuid& uuid, const std::string& name)
	{
		Entity entity(m_Registry.create(), this);
		
		(*m_Entities)[uuid] = entity;
		(*m_Uuids)[entity] = uuid;

		entity.AddComponent<TagComponent>(name);
		entity.AddComponent<RelationshipComponent>();
		entity.AddComponent<TransformComponent>();
		m_HasChanged = true;

		return entity;
	}

	Entity Scene::GetEntity(const Uuid& uuid)
	{
		return (*m_Entities)[uuid];
	}

	Uuid& Scene::GetUuid(Entity entity)
	{
		return (*m_Uuids)[entity];
	}

	Entity Scene::GetRootEntity()
	{
		return *m_RootEntity;
	}

	void Scene::OnViewportResize(uint32_t width, uint32_t height)
	{
		m_ViewportWidth = width;
		m_ViewportHeight = height;
		IDBufferRenderer::OnResize(width, height);
		m_Registry.view<CameraComponent>().each([&](auto& e, auto& cc) {
		//if(!cc.FixedAspectRatio)
			cc.Camera.SetViewportSize(width, height);
		});
	}

}
