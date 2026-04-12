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
            m_DeferredActions.push_back([parent, entityName, this]() mutable {
                auto activeScene = gSceneManager->GetActiveScene();
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
        const UnorderedSet<UUID>& GetSerializableHierarchy();
        void SetHierarchy(const UnorderedSet<UUID>& hierarchy) { m_Hierarchy = hierarchy; }

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
        void RenderEntityRow(Entity e, bool hasChildren);
        void RenderContextMenu(Entity e);
        void Rename(Entity e);
        void RenderSearchResults();
        bool MatchesSearchFilter(Entity e) const;
        void CollectMatchingEntities(Entity e, Vector<Entity>& results) const;
        String BuildParentPath(Entity e) const;

#ifdef CW_DEBUG
        void PrintDebugHierarchy();
#endif

    private:
        bool m_PreserveHierarchy = true;
        Entity m_NewOpenEntity;
        Vector<std::function<void()>> m_DeferredActions;
        std::function<void(Entity)> m_SelectionChanged;
        Entity m_Renaming = {};
        String m_RenamingString;
        String m_SearchFilter;
        UnorderedSet<Entity> m_SelectedItems;
        UnorderedSet<UUID> m_Hierarchy;
    };
} // namespace Crowny
