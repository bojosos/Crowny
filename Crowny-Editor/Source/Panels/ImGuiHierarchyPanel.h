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
		void DisplayTree(Entity e);
		void Select(Entity e);
		void DisplayLeafNode(Entity e);
		void DisplayTreeNode(Entity e);
		void DisplayPopup(Entity e);
		void Rename(Entity e);

	private:
		Entity m_NewEntityParent = {};
		Entity m_Renaming = { };
		std::unordered_set<Entity> m_SelectedItems;
	};
}