#pragma once

#include "Crowny/Ecs/Entity.h"
#include "Editor/EntitySelection.h"
#include "Editor/UndoRedo.h"
#include "Panels/ImGuiPanel.h"

namespace Crowny
{

    class HierarchyPanel : public ImGuiPanel
    {
    public:
        using SelectionChangedCallback = std::function<void(Entity, const Vector<Entity>&)>;

        HierarchyPanel(const String& name, SelectionChangedCallback callback);
        ~HierarchyPanel() = default;

        virtual void Render() override;
        void Update();

        template <class T> void CreateEntityWith(Entity parent, const String& entityName)
        {
            m_DeferredActions.push_back([parent, entityName, this]() mutable {
                auto activeScene = SceneManager::TryGet()->GetActiveScene();
                Entity newEntity = activeScene->CreateEntity(entityName);
                newEntity.AddComponent<T>();
                parent.AddChild(newEntity);
                UndoRedo::Get().RegisterAction(CreateRef<EntityCreatedAction>(newEntity, activeScene));
                m_NewOpenEntity = parent;
                SetSelectedEntity(newEntity);
            });
        }
        void CreateEmptyEntity(Entity parent);
        const UnorderedSet<UUID>& GetSerializableHierarchy();
        void SetHierarchy(const UnorderedSet<UUID>& hierarchy) { m_Hierarchy = hierarchy; }

    public:
        void SetSelectedEntity(Entity entity)
        {
            m_Selection.Select(entity, EntitySelectionMode::Replace);
            NotifySelectionChanged();
        }
        void SelectEntity(Entity entity, EntitySelectionMode mode)
        {
            if (m_Selection.Select(entity, mode, m_VisibleEntities))
                NotifySelectionChanged();
        }
        Entity GetSelectedEntity() const { return m_Selection.GetPrimary(); }
        const Vector<Entity>& GetSelectedEntities() const { return m_Selection.GetAll(); }

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
        Vector<Entity> GetTopLevelSelection() const;
        void NotifySelectionChanged();
        void QueueReparent(const Vector<Entity>& entities, Entity newParent);
        void ApplyPendingSelection();

#ifdef CW_DEBUG
        void PrintDebugHierarchy();
#endif

    private:
        bool m_PreserveHierarchy = true;
        Entity m_NewOpenEntity;
        Vector<std::function<void()>> m_DeferredActions;
        SelectionChangedCallback m_SelectionChanged;
        Entity m_Renaming = {};
        String m_RenamingString;
        String m_SearchFilter;
        EntitySelection m_Selection;
        Vector<Entity> m_VisibleEntities;
        Entity m_PendingSelection;
        EntitySelectionMode m_PendingSelectionMode = EntitySelectionMode::Replace;
        UnorderedSet<UUID> m_Hierarchy;
    };
} // namespace Crowny
