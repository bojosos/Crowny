#pragma once

#include "Crowny/SceneManagement/Scene.h"

#include <entt/entt.hpp>

namespace Crowny
{
	class ImGuiComponentEditor;
	class ImGuiHierarchyWindow;

	class Entity
	{
	public:
		Entity() = default;
		Entity(entt::entity entity, Scene* scene);
		Entity(const Entity& other) = default;

		template <typename T, typename... Args>
		void AddComponent(Args... args)
		{
			m_Scene->m_Registry.emplace<T>(m_EntityHandle, std::forward<Args>(args)...);
		}

		template<typename T>
		T& GetComponent() const
		{
			return m_Scene->m_Registry.get<T>(m_EntityHandle);
		}

		template <typename T>
		void RemoveComponent()
		{
			m_Scene->m_Registry.remove<T>(m_EntityHandle);
		}

		template<typename T>
		void HasComponent() const
		{
			return m_Scene->m_Registry.has<T>(m_EntityHandle);
		}

		bool IsValid() { return m_Scene && m_Scene->m_Registry.valid(m_EntityHandle); }

		Entity AddChild(const std::string& name);
		Entity GetChild(uint32_t index);
		uint32_t GetChildCount();
		void Destroy();

		bool operator==(const Entity& other) const 
		{
			return m_EntityHandle == other.m_EntityHandle;//&& m_Scene == other.m_Scene;
		}

		friend class ImGuiComponentEditor;
		friend class ImGuiHierarchyWindow;

		struct EntityHasher
		{
			size_t operator()(const Entity& e) const
			{
				return (size_t)e.m_EntityHandle;
			}
		};

	private:
		entt::entity m_EntityHandle{ entt::null };
		Scene* m_Scene = nullptr;
	};  
}