#include "cwpch.h"

#include "Crowny/Ecs/Entity.h"
#include "Crowny/Ecs/Components.h"

namespace Crowny
{
	Entity::Entity(entt::entity entity, Scene* scene) : m_EntityHandle(entity), m_Scene(scene)
	{

	}

	Entity Entity::AddChild(const std::string& name)
	{
		auto& rc = GetComponent<RelationshipComponent>();
		auto entity = rc.Children.emplace_back(m_Scene->m_Registry.create(), m_Scene);

		entity.AddComponent<TagComponent>(name);
		entity.AddComponent<TransformComponent>();
		entity.AddComponent<RelationshipComponent>(*this);

		return entity;
	}

	Entity Entity::GetChild(uint32_t index)
	{
		return GetComponent<RelationshipComponent>().Children[index];
	}

	uint32_t Entity::GetChildCount()
	{
		return (uint32_t)GetComponent<RelationshipComponent>().Children.size();
	}

	Entity Entity::GetParent()
	{
		return GetComponent<RelationshipComponent>().Parent;
	}

	void Entity::Destroy()
	{
		m_Scene->m_Registry.destroy(m_EntityHandle);
		m_EntityHandle = entt::null;
	}

}
