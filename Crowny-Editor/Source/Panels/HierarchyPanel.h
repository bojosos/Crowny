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

        template <class T> void CreateEntityWith(Entity parent, const String& entityName)
        {
            m_DeferedActions.push_back([parent, entityName, this]() mutable {
                auto activeScene = SceneManager::GetActiveScene();
                Entity newEntity = activeScene->CreateEntity(entityName);
                newEntity.AddComponent<T>();
                parent.AddChild(newEntity);
                m_NewOpenEntity = parent;
                m_SelectedItems.clear();
                m_SelectedItems.insert(newEntity);
                m_SelectionChanged(newEntity);
            });
        }
        void CreateEmptyEntity(Entity parent);
        void BuildSerializableHierarchy();

    public:
        void SetSelectedEntity(Entity entity)
        {
            s_SelectedEntity = entity;
            m_SelectedItems.clear();
            m_SelectedItems.insert(entity);
        }
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

#ifdef CW_DEBUG
        void PrintDebugHierarchy();
#endif

    private:
        Entity m_NewOpenEntity;
        Vector<std::function<void()>> m_DeferedActions;
        std::function<void(Entity)> m_SelectionChanged;
        bool m_Deleted = false;
        Entity m_Renaming = {};
        String m_RenamingString;
        UnorderedSet<Entity> m_SelectedItems;
    };
} // namespace Crowny