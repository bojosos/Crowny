#pragma once

#include "Crowny-CLI.h"
#include "Transform.h"
#include "../../../Crowny/Source/Crowny/Ecs/Entity.h"
#include "../../../Crowny/Source/Crowny/Ecs/Components.h"

#include <glm/glm.hpp>

namespace Crowny
{

	public ref class SpriteRenderer : public ManagedClass<Crowny::SpriteRendererComponent> 
	{
	public:
		property Sprite sprite
		{
			Sprite get()
			{
				return m_Instance;
			}

			void set(Sprite value)
			{

			}
		}
	};

	public ref class MeshRenderer : public ManagedClass<Crowny::MeshRendererComponent> {};
	public ref class Text : public ManagedClass<Crowny::TextComponent> {};
	public ref class Transform : public ManagedClass<Crowny::TransformComponent> {};
	public ref class Camera : public ManagedClass<Crowny::CameraComponent> {};

	public ref class EntityBehaviour : public ManagedClass<Entity>
	{
	public:
		property glm::mat4 transform
		{
			glm::mat4 get()
			{
				return m_Instance->GetComponent<TransformComponent>().Transform;
			}

			void set(glm::mat4 value)
			{
				m_Instance->GetComponent<TransformComponent>().Transform = value;
			}
		}

	public:
		template<typename T>
		T GetComponent()
		{
			
		}
		
		template<typename T>
		bool HasComponent()
		{

		}

	};
}