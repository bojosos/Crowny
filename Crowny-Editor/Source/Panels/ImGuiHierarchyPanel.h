#pragma once

#include "ImGuiPanel.h"
#include "Crowny/Ecs/Entity.h"

namespace Crowny
{
		
	class ImGuiHierarchyPanel : public ImGuiPanel
	{
	public:
		ImGuiHierarchyPanel(const std::string& name);
		~ImGuiHierarchyPanel() = default;

		virtual void Render() override;
		void Update();

		virtual void Show() override;
		virtual void Hide() override;

	public:
		static Entity GetSelectedEntity() { return s_SelectedEntity; }

	private:
		static Entity s_SelectedEntity;

	private:
		void DisplayTree(Entity& e, uint32_t i = 0);
		Entity m_NewEntityParent = {};

		std::unordered_set<Entity, Entity::EntityHasher> m_SelectedItems;
	};
}