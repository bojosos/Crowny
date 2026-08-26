#pragma once

#include "Crowny/Common/Module.h"
#include "Crowny/Ecs/Components.h"
#include "Crowny/Ecs/Entity.h"
#include "Crowny/Scene/SceneManager.h"
#include "Crowny/Scripting/Serialization/SerializableObject.h"

namespace Crowny
{
    class UndoAction : public RefCounted
    {
    public:
        explicit UndoAction(String name = "Edit") : m_Name(std::move(name)) {}
        virtual ~UndoAction() = default;

        virtual void Commit() {}
        virtual void Revert() {}
        virtual Entity GetFocusEntity() const { return {}; }
        const String& GetName() const { return m_Name; }

    private:
        String m_Name;
    };

    class UndoActionGroup : public UndoAction
    {
    public:
        explicit UndoActionGroup(String name = "Edit") : UndoAction(std::move(name)) {}

        void Add(const Ref<UndoAction>& action)
        {
            if (action)
                m_Actions.push_back(action);
        }
        bool Empty() const { return m_Actions.empty(); }

        void Commit() override
        {
            for (const Ref<UndoAction>& action : m_Actions)
                action->Commit();
        }

        void Revert() override
        {
            for (auto action = m_Actions.rbegin(); action != m_Actions.rend(); ++action)
                (*action)->Revert();
        }

        Entity GetFocusEntity() const override
        {
            for (const Ref<UndoAction>& action : m_Actions)
            {
                if (Entity entity = action->GetFocusEntity())
                    return entity;
            }
            return {};
        }

    private:
        Vector<Ref<UndoAction>> m_Actions;
    };

    struct UndoItemInteraction
    {
        uint32_t ItemId = 0u;
        bool Active = false;
        bool Activated = false;
        bool DeactivatedAfterEdit = false;
        bool Changed = false;
    };

    class RetainedUndoActionFactory : public RefCounted
    {
    public:
        virtual ~RetainedUndoActionFactory() = default;
        virtual void BeforeItemInteraction(const UndoItemInteraction&) {}
        virtual Ref<UndoAction> Build() const = 0;
        virtual void Reset() = 0;
    };

    class UndoRedo : public Module<UndoRedo>
    {
    public:
        using ActionAppliedCallback = std::function<void(const Ref<UndoAction>&)>;

        void RegisterAction(const Ref<UndoAction>& action);
        Ref<UndoAction> Undo();
        Ref<UndoAction> Redo();

        bool CanUndo() const { return !m_UndoStack.empty(); }
        bool CanRedo() const { return !m_RedoStack.empty(); }
        const String& GetUndoName() const;
        const String& GetRedoName() const;
        void Clear();

        void SetActionAppliedCallback(ActionAppliedCallback callback) { m_ActionApplied = std::move(callback); }

        void BeginComponentScope(std::function<Ref<UndoAction>()> factory);
        bool BeginComponentScope(const Ref<RetainedUndoActionFactory>& factory);
        void EndComponentScope();
        void OnItemInteract();
        void OnItemInteract(bool valueChanged);
        void OnItemInteract(const UndoItemInteraction& interaction);
        void FinishComponentScope(const Ref<RetainedUndoActionFactory>& factory);
        void CancelComponentScope(const Ref<RetainedUndoActionFactory>& factory);
        void CancelInteraction();

    private:
        bool HasComponentActionFactory() const;
        Ref<UndoAction> CreateComponentAction() const;
        void FinishInteraction();

        static constexpr size_t MaxHistorySize = 256u;
        Vector<Ref<UndoAction>> m_UndoStack;
        Vector<Ref<UndoAction>> m_RedoStack;
        std::function<Ref<UndoAction>()> m_Factory;
        Ref<RetainedUndoActionFactory> m_RetainedFactory;
        ActionAppliedCallback m_ActionApplied;
        uint32_t m_InteractionItemId = 0u;
        bool m_InInteraction = false;
        bool m_InteractionChanged = false;
    };

    template <typename T> class AddComponentAction : public UndoAction
    {
    public:
        explicit AddComponentAction(Entity entity)
          : UndoAction("Add component"), m_Scene(entity.GetScene()), m_Entity(entity.GetUuid()), m_Component(entity.GetComponent<T>())
        {
        }

        void Commit() override
        {
            Entity entity = Resolve();
            if (entity && !entity.HasComponent<T>())
                entity.AddComponent<T>(m_Component);
        }

        void Revert() override
        {
            Entity entity = Resolve();
            if (entity && entity.HasComponent<T>())
                entity.RemoveComponent<T>();
        }

    private:
        Entity Resolve() const { return m_Scene ? m_Scene->TryGetEntityFromUuid(m_Entity) : Entity{}; }

        Scene* m_Scene = nullptr;
        UUID m_Entity;
        T m_Component;
    };

    template <typename T> class RemoveComponentAction : public UndoAction
    {
    public:
        RemoveComponentAction(Entity entity, const T& component)
          : UndoAction("Remove component"), m_Scene(entity.GetScene()), m_Entity(entity.GetUuid()), m_Component(component)
        {
        }

        void Commit() override
        {
            Entity entity = Resolve();
            if (entity && entity.HasComponent<T>())
                entity.RemoveComponent<T>();
        }

        void Revert() override
        {
            Entity entity = Resolve();
            if (entity && !entity.HasComponent<T>())
                entity.AddComponent<T>(m_Component);
        }

    private:
        Entity Resolve() const { return m_Scene ? m_Scene->TryGetEntityFromUuid(m_Entity) : Entity{}; }

        Scene* m_Scene = nullptr;
        UUID m_Entity;
        T m_Component;
    };

    template <typename T> class ChangeComponentAction : public UndoAction
    {
    public:
        ChangeComponentAction(Entity entity, const T& oldComponent, const T& newComponent)
          : UndoAction("Edit component"), m_Scene(entity.GetScene()), m_Entity(entity.GetUuid()), m_OldComponent(oldComponent),
            m_NewComponent(newComponent)
        {
        }

        void Commit() override { Apply(m_NewComponent); }
        void Revert() override { Apply(m_OldComponent); }

    private:
        Entity Resolve() const { return m_Scene ? m_Scene->TryGetEntityFromUuid(m_Entity) : Entity{}; }

        void Apply(const T& component)
        {
            Entity entity = Resolve();
            if (!entity)
                return;
            entity.AddOrReplaceComponent<T>(component);
            if constexpr (std::is_same_v<T, TransformComponent>)
                entity.NotifyTransformChanged();
        }

        Scene* m_Scene = nullptr;
        UUID m_Entity;
        T m_OldComponent, m_NewComponent;
    };

    template <typename T> class ChangeComponentsAction : public UndoAction
    {
    public:
        struct Snapshot
        {
            Scene* SceneRef = nullptr;
            UUID Target;
            T OldValue;
            T NewValue;
        };

        explicit ChangeComponentsAction(const Vector<Pair<Entity, T>>& oldValues) : UndoAction("Edit components")
        {
            m_Snapshots.reserve(oldValues.size());
            for (const auto& [entity, oldValue] : oldValues)
            {
                if (entity && entity.template HasComponent<T>())
                    m_Snapshots.push_back({ entity.GetScene(), entity.GetUuid(), oldValue, entity.template GetComponent<T>() });
            }
        }

        void FinalizeNewValues()
        {
            for (Snapshot& snapshot : m_Snapshots)
            {
                Entity target = snapshot.SceneRef ? snapshot.SceneRef->TryGetEntityFromUuid(snapshot.Target) : Entity{};
                if (target && target.template HasComponent<T>())
                    snapshot.NewValue = target.template GetComponent<T>();
            }
        }

        void Commit() override { Apply(false); }
        void Revert() override { Apply(true); }

    private:
        void Apply(bool oldValues)
        {
            for (Snapshot& snapshot : m_Snapshots)
            {
                Entity target = snapshot.SceneRef ? snapshot.SceneRef->TryGetEntityFromUuid(snapshot.Target) : Entity{};
                if (!target)
                    continue;
                target.AddOrReplaceComponent<T>(oldValues ? snapshot.OldValue : snapshot.NewValue);
                if constexpr (std::is_same_v<T, TransformComponent>)
                    target.NotifyTransformChanged();
            }
        }

        Vector<Snapshot> m_Snapshots;
    };

    class ChangeScriptComponentAction : public UndoAction
    {
    public:
        using State = Vector<PersistedScriptState>;

        ChangeScriptComponentAction(Entity entity, State oldState, String name = "Edit script");

        static State Capture(Entity entity);
        void CaptureNewState(Entity entity);
        void Commit() override;
        void Revert() override;

    private:
        void Apply(const State& state);

        Scene* m_Scene = nullptr;
        UUID m_Entity = UUID::EMPTY;
        State m_OldState;
        State m_NewState;
    };

    class EntitySnapshot
    {
    public:
        EntitySnapshot() = default;
        EntitySnapshot(Entity entity, const Ref<Scene>& scene);

        Entity Restore() const;
        Entity Destroy() const;
        Entity ResolveRoot() const;
        Entity ResolveParent() const;

    private:
        Ref<Scene> m_Scene;
        UUID m_RootUuid = UUID::EMPTY;
        UUID m_ParentUuid = UUID::EMPTY;
        uint32_t m_SiblingIndex = 0u;
        glm::vec3 m_LocalPosition{ 0.0f };
        glm::quat m_LocalRotation{ 1.0f, 0.0f, 0.0f, 0.0f };
        glm::vec3 m_LocalScale{ 1.0f };
        String m_Yaml;
    };

    class EntityCreatedAction : public UndoAction
    {
    public:
        EntityCreatedAction(Entity entity, const Ref<Scene>& scene);

        void Commit() override;
        void Revert() override;
        Entity GetFocusEntity() const override { return m_Focus; }

    private:
        EntitySnapshot m_Snapshot;
        Entity m_Focus;
    };

    class EntityDeletedAction : public UndoAction
    {
    public:
        EntityDeletedAction(Entity entity, const Ref<Scene>& scene);

        void Commit() override;
        void Revert() override;
        Entity GetFocusEntity() const override { return m_Focus; }

    private:
        EntitySnapshot m_Snapshot;
        Entity m_Focus;
    };

    class EntityReparentAction : public UndoAction
    {
    public:
        EntityReparentAction(Entity entity, Entity oldParent, Entity newParent);

        void Commit() override;
        void Revert() override;

    private:
        void Reparent(UUID targetUuid, uint32_t siblingIndex);

        Scene* m_Scene = nullptr;
        UUID m_Entity = UUID::EMPTY;
        UUID m_OldParent = UUID::EMPTY;
        UUID m_NewParent = UUID::EMPTY;
        uint32_t m_OldSibling = 0u;
        uint32_t m_NewSibling = 0u;
    };
} // namespace Crowny
