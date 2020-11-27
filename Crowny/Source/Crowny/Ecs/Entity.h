#pragma once

#include "Crowny/Scene/Scene.h"

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
		T& AddComponent(Args&&... args) const
		{
			return m_Scene->m_Registry.emplace<T>(m_EntityHandle, std::forward<Args>(args)...);
		}

		template <typename T>
		T& GetComponent() const
		{
			return m_Scene->m_Registry.get<T>(m_EntityHandle);
		}

		template <typename T>
		bool HasComponent() const
		{
			return m_Scene->m_Registry.has<T>(m_EntityHandle);
		}

		template <typename T>
		void RemoveComponent()
		{
			m_Scene->m_Registry.remove<T>(m_EntityHandle);
		}

		entt::entity GetHandle() { return m_EntityHandle; }

		bool IsValid() const;

		void AddChild(Entity entity);
		Entity GetChild(uint32_t index) const;
		const std::vector<Entity>& GetChildren() const;
		uint32_t GetChildCount() const;
		Entity GetParent() const;
		void SetParent(Entity entity);

		void Destroy();

		bool operator==(const Entity& other) const 
		{
			return m_EntityHandle == other.m_EntityHandle;//&& m_Scene == other.m_Scene;
		}

		operator bool() const 
		{
			return IsValid();
		}

		friend class ImGuiComponentEditor;
		friend class ImGuiHierarchyWindow;
		friend struct std::hash<Entity>;

	private:
		entt::entity m_EntityHandle{ entt::null };
		Scene* m_Scene;
	};  
}

namespace std
{
	template<>
	struct hash<Crowny::Entity>
	{
		size_t operator()(const Crowny::Entity& e) const
		{
			return (size_t)e.m_EntityHandle;
		}
	};
}