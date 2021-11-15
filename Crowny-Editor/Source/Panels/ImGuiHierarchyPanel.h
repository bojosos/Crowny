#pragma once

#include "Crowny/Ecs/Entity.h"
#include "ImGuiPanel.h"

namespace Crowny
{

    class ImGuiHierarchyPanel : public ImGuiPanel
    {
    public:
        ImGuiHierarchyPanel(const String& name);
        ~ImGuiHierarchyPanel() = default;

        virtual void Render() override;
        void Update();

        virtual void Show() override;
        virtual void Hide() override;

    public:
        static void SetSelectedEntity(Entity entity) { s_SelectedEntity = entity; }
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
        bool m_Deleted = false;
        Entity m_NewEntityParent = {};
        Entity m_Renaming = {};
        UnorderedSet<Entity> m_SelectedItems;
    };
} // namespace Crowny