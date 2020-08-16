#include "cwpch.h"

#include "Crowny/Ecs/Entity.h"
#include "Crowny/Ecs/Components.h"

namespace Crowny
{
	Entity::Entity(entt::entity entity, Scene* scene) : m_EntityHandle(entity), m_Scene(scene)
	{

	}

	void Entity::AddChild(const std::string& name)
	{
		auto& rc = GetComponent<RelationshipComponent>();
		rc.Children.emplace_back(m_Scene->m_Registry.create(), m_Scene);

		rc.Children.back().AddComponent<TagComponent>(name);
		rc.Children.back().AddComponent<TransformComponent>();
		rc.Children.back().AddComponent<RelationshipComponent>(*this);
	}

	Entity& Entity::GetChild(uint32_t index)
	{
		return GetComponent<RelationshipComponent>().Children[index];
	}

	uint32_t Entity::GetChildCount()
	{
		return GetComponent<RelationshipComponent>().Children.size();
	}

}