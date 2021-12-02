#pragma once

#include "Crowny/Ecs/Entity.h"
#include "Panels/ImGuiPanel.h"

namespace Crowny
{

    class HierarchyPanel : public ImGuiPanel
    {
    public:
        HierarchyPanel(const String& name, std::function<void(Entity)> callback);
        ~HierarchyPanel() = default;

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
        std::function<void(Entity)> m_SelectionChanged;
        bool m_Deleted = false;
        Entity m_NewEntityParent = {};
        Entity m_Renaming = {};
        UnorderedSet<Entity> m_SelectedItems;
    };
} // namespace Crowny