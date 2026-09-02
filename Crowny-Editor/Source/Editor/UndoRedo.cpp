#include "cwepch.h"

#include "Editor/UndoRedo.h"

#include "Crowny/Serialization/SceneSerializer.h"
#include "Crowny/Scene/ScriptRuntime.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <yaml-cpp/yaml.h>

namespace Crowny
{
    namespace
    {
        const String EmptyActionName;

        void SerializeSubtree(SceneSerializer& serializer, YAML::Emitter& output, Entity entity)
        {
            if (!entity)
                return;
            serializer.SerializeEntity(output, entity);
            const Vector<Entity> children = entity.GetChildren();
            for (Entity child : children)
                SerializeSubtree(serializer, output, child);
        }

        Vector<Entity> NormalizeHierarchySelection(const Vector<Entity>& entities, Scene* scene)
        {
            Vector<Entity> candidates;
            if (scene == nullptr)
                return candidates;

            const Entity sceneRoot = scene->GetRootEntity();
            UnorderedSet<entt::entity> selectedHandles;
            selectedHandles.reserve(entities.size());
            candidates.reserve(entities.size());
            for (Entity entity : entities)
            {
                if (!entity || entity.GetScene() != scene || (sceneRoot && entity == sceneRoot) ||
                    !selectedHandles.insert(entity.GetHandle()).second)
                    continue;
                candidates.push_back(entity);
            }

            Vector<Entity> roots;
            roots.reserve(candidates.size());
            UnorderedSet<entt::entity> ancestorPath;
            for (Entity entity : candidates)
            {
                bool selectedAncestor = false;
                ancestorPath.clear();
                for (Entity parent = entity.GetParent(); parent; parent = parent.GetParent())
                {
                    if (!ancestorPath.insert(parent.GetHandle()).second)
                        return {};
                    if (selectedHandles.find(parent.GetHandle()) != selectedHandles.end())
                    {
                        selectedAncestor = true;
                        break;
                    }
                }
                if (!selectedAncestor)
                    roots.push_back(entity);
            }
            return roots;
        }
    } // namespace

    bool UndoTransaction::Begin(Id id, ActionFactory factory)
    {
        if (m_Active || id == 0u || !factory)
            return false;

        m_Id = id;
        m_Factory = std::move(factory);
        m_RetainedFactory = nullptr;
        m_Active = true;
        m_Changed = false;
        return true;
    }

    bool UndoTransaction::Begin(Id id, const Ref<RetainedUndoActionFactory>& factory)
    {
        if (m_Active || id == 0u || factory == nullptr)
            return false;

        m_Id = id;
        m_Factory = nullptr;
        m_RetainedFactory = factory;
        m_Active = true;
        m_Changed = false;
        return true;
    }

    void UndoTransaction::Update(Id id, bool changed)
    {
        if (Owns(id))
            m_Changed |= changed;
    }

    Ref<UndoAction> UndoTransaction::Commit(Id id)
    {
        if (!Owns(id))
            return {};

        Ref<UndoAction> action = m_Changed ? BuildAction() : Ref<UndoAction>{};
        Cancel();
        return action;
    }

    void UndoTransaction::Cancel(Id id)
    {
        if (Owns(id))
            Cancel();
    }

    void UndoTransaction::Cancel()
    {
        m_Factory = nullptr;
        m_RetainedFactory = nullptr;
        m_Id = 0u;
        m_Active = false;
        m_Changed = false;
    }

    Ref<UndoAction> UndoTransaction::BuildAction() const
    {
        if (m_RetainedFactory != nullptr)
            return m_RetainedFactory->Build();
        return m_Factory ? m_Factory() : Ref<UndoAction>{};
    }

    void UndoRedo::RegisterAction(const Ref<UndoAction>& action)
    {
        if (!m_RecordingEnabled || !action)
            return;

        m_RedoStack.clear();
        m_UndoStack.push_back(action);
        if (m_UndoStack.size() > MaxHistorySize)
            m_UndoStack.erase(m_UndoStack.begin(), m_UndoStack.begin() + (m_UndoStack.size() - MaxHistorySize));
    }

    Ref<UndoAction> UndoRedo::Undo()
    {
        CancelInteraction();
        if (!m_RecordingEnabled || m_UndoStack.empty())
            return {};

        Ref<UndoAction> action = m_UndoStack.back();
        m_UndoStack.pop_back();
        action->Revert();
        m_RedoStack.push_back(action);
        ++m_ActionAppliedVersion;
        if (m_ActionApplied)
            m_ActionApplied(action);
        return action;
    }

    Ref<UndoAction> UndoRedo::Redo()
    {
        CancelInteraction();
        if (!m_RecordingEnabled || m_RedoStack.empty())
            return {};

        Ref<UndoAction> action = m_RedoStack.back();
        m_RedoStack.pop_back();
        action->Commit();
        m_UndoStack.push_back(action);
        ++m_ActionAppliedVersion;
        if (m_ActionApplied)
            m_ActionApplied(action);
        return action;
    }

    const String& UndoRedo::GetUndoName() const { return m_UndoStack.empty() ? EmptyActionName : m_UndoStack.back()->GetName(); }

    const String& UndoRedo::GetRedoName() const { return m_RedoStack.empty() ? EmptyActionName : m_RedoStack.back()->GetName(); }

    void UndoRedo::Clear()
    {
        CancelInteraction();
        m_UndoStack.clear();
        m_RedoStack.clear();
    }

    void UndoRedo::SetSceneContext(const Ref<Scene>& scene, bool recordingEnabled)
    {
        if (!recordingEnabled)
        {
            CancelInteraction();
            m_RecordingEnabled = false;
            return;
        }

        m_RecordingEnabled = true;
        if (m_HistoryScene != scene)
        {
            Clear();
            m_HistoryScene = scene;
        }
    }

    void UndoRedo::BeginComponentScope(std::function<Ref<UndoAction>()> factory)
    {
        if (!m_RecordingEnabled)
            return;

        m_ComponentScopeOpen = true;
        if (m_Transaction.IsActive())
            return;

        m_Factory = std::move(factory);
        m_RetainedFactory = nullptr;
    }

    bool UndoRedo::BeginComponentScope(const Ref<RetainedUndoActionFactory>& factory)
    {
        if (!m_RecordingEnabled)
            return false;

        m_ComponentScopeOpen = true;
        if (m_Transaction.IsActive())
            return false;

        m_Factory = nullptr;
        m_RetainedFactory = factory;
        return true;
    }

    void UndoRedo::EndComponentScope()
    {
        if (m_CommitPending)
            FinishInteraction();

        if (!m_Transaction.IsActive())
        {
            m_Factory = nullptr;
            m_RetainedFactory = nullptr;
        }
        m_ComponentScopeOpen = false;
    }

    void UndoRedo::OnItemInteract() { OnItemInteract(ImGui::IsItemEdited()); }

    void UndoRedo::OnItemInteract(bool valueChanged)
    {
        if (GImGui == nullptr)
            return;

        OnItemInteract({ GImGui->LastItemData.ID, ImGui::IsItemActive(), ImGui::IsItemActivated(), ImGui::IsItemDeactivatedAfterEdit(),
                         valueChanged || ImGui::IsItemEdited() });
    }

    void UndoRedo::OnItemInteract(const UndoItemInteraction& interaction)
    {
        if ((!HasPendingActionFactory() && !m_Transaction.IsActive()) || interaction.ItemId == 0u)
            return;

        if (m_RetainedFactory != nullptr)
            m_RetainedFactory->BeforeItemInteraction(interaction);

        if (m_Transaction.IsActive())
        {
            if (!m_Transaction.Owns(interaction.ItemId))
                return;

            m_Transaction.Update(interaction.ItemId, interaction.Changed);
            if (interaction.DeactivatedAfterEdit || (!interaction.Active && !interaction.Activated))
            {
                if (m_ComponentScopeOpen)
                    m_CommitPending = true;
                else
                    FinishInteraction();
            }
            return;
        }

        if (interaction.Activated && interaction.Active)
        {
            if (!BeginPendingTransaction(interaction.ItemId))
                return;
            m_Transaction.Update(interaction.ItemId, interaction.Changed);
            if (interaction.DeactivatedAfterEdit)
                m_CommitPending = true;
        }
        else if (interaction.Changed)
        {
            if (!BeginPendingTransaction(interaction.ItemId))
                return;
            m_Transaction.Update(interaction.ItemId);
            m_CommitPending = true;
        }
    }

    bool UndoRedo::HasPendingActionFactory() const { return m_RetainedFactory != nullptr || static_cast<bool>(m_Factory); }

    bool UndoRedo::BeginPendingTransaction(uint32_t itemId)
    {
        if (m_RetainedFactory != nullptr)
            return m_Transaction.Begin(itemId, m_RetainedFactory);
        return m_Transaction.Begin(itemId, std::move(m_Factory));
    }

    void UndoRedo::FinishComponentScope(const Ref<RetainedUndoActionFactory>& factory)
    {
        if (m_Transaction.Owns(factory))
            FinishInteraction();
    }

    void UndoRedo::CancelComponentScope(const Ref<RetainedUndoActionFactory>& factory)
    {
        if (m_Transaction.Owns(factory))
            CancelInteraction();
    }

    void UndoRedo::FinishInteraction()
    {
        RegisterAction(m_Transaction.Commit(m_Transaction.IsActive() ? m_Transaction.GetId() : 0u));
        m_Factory = nullptr;
        m_RetainedFactory = nullptr;
        m_ComponentScopeOpen = false;
        m_CommitPending = false;
    }

    void UndoRedo::CancelInteraction()
    {
        m_Transaction.Cancel();
        m_Factory = nullptr;
        m_RetainedFactory = nullptr;
        m_ComponentScopeOpen = false;
        m_CommitPending = false;
    }

    ChangeScriptComponentAction::ChangeScriptComponentAction(Entity entity, State oldState, String name)
      : UndoAction(std::move(name)), m_Scene(entity.GetScene()), m_Entity(entity.GetUuid()), m_OldState(std::move(oldState)),
        m_NewState(Capture(entity))
    {
    }

    ChangeScriptComponentAction::State ChangeScriptComponentAction::Capture(Entity entity)
    {
        State state;
        if (!entity || !entity.HasComponent<ManagedScriptComponent>())
            return state;

        ManagedScriptComponent& component = entity.GetComponent<ManagedScriptComponent>();
        state.reserve(component.Scripts.size());
        for (ManagedScript& script : component.Scripts)
            state.push_back(ScriptRuntime::CaptureState(script));
        return state;
    }

    void ChangeScriptComponentAction::CaptureNewState(Entity entity)
    {
        if (entity && entity.GetScene() == m_Scene.get() && entity.GetUuid() == m_Entity)
            m_NewState = Capture(entity);
    }

    void ChangeScriptComponentAction::Commit() { Apply(m_NewState); }

    void ChangeScriptComponentAction::Revert() { Apply(m_OldState); }

    void ChangeScriptComponentAction::Apply(const State& state)
    {
        Entity entity = m_Scene ? m_Scene->TryGetEntityFromUuid(m_Entity) : Entity{};
        if (!entity)
            return;

        Vector<ScriptTypeIdentity> currentTypes;
        if (entity.HasComponent<ManagedScriptComponent>())
        {
            for (const ManagedScript& script : entity.GetComponent<ManagedScriptComponent>().Scripts)
                currentTypes.push_back(script.GetTypeIdentity());
        }
        Vector<ScriptTypeIdentity> desiredTypes;
        desiredTypes.reserve(state.size());
        for (const ScriptState& script : state)
            desiredTypes.push_back(script.Identity);

        if (currentTypes != desiredTypes)
        {
            for (const ScriptTypeIdentity& identity : currentTypes)
                m_Scene->RemoveScriptComponent(entity, identity);
            for (const ScriptState& script : state)
                m_Scene->AddScriptComponent(entity, script);
            return;
        }

        if (!entity.HasComponent<ManagedScriptComponent>())
            return;
        ManagedScriptComponent& component = entity.GetComponent<ManagedScriptComponent>();
        for (const ScriptState& snapshot : state)
        {
            const auto found = std::find_if(component.Scripts.begin(), component.Scripts.end(),
                                            [&](const ManagedScript& script) { return script.GetTypeIdentity() == snapshot.Identity; });
            if (found != component.Scripts.end())
                ScriptRuntime::ApplyState(*found, snapshot);
        }
    }

    EntitySnapshot::EntitySnapshot(Entity entity, const Ref<Scene>& scene) : m_Scene(scene)
    {
        if (!m_Scene || !entity || entity.GetScene() != m_Scene.get())
            return;

        m_RootUuid = entity.GetUuid();
        Entity parent = entity.GetParent();
        m_ParentUuid = parent ? parent.GetUuid() : UUID::EMPTY;
        m_SiblingIndex = entity.GetSiblingIndex();
        m_LocalPosition = entity.GetLocalPosition();
        m_LocalRotation = entity.GetLocalRotation();
        m_LocalScale = entity.GetLocalScale();

        YAML::Emitter output;
        output << YAML::BeginSeq;
        SceneSerializer serializer(m_Scene);
        SerializeSubtree(serializer, output, entity);
        output << YAML::EndSeq;
        m_Yaml = output.c_str();
    }

    Entity EntitySnapshot::Restore() const
    {
        if (!m_Scene || m_RootUuid == UUID::EMPTY || m_Yaml.empty())
            return Entity::Invalid;
        if (Entity existing = ResolveRoot())
            return existing;

        SceneSerializer serializer(m_Scene);
        serializer.DeserializeEntities(YAML::Load(m_Yaml));
        Entity restored = ResolveRoot();
        Entity parent = ResolveParent();
        if (restored && parent)
        {
            const Array<Entity, 1> entities{ restored };
            m_Scene->ReparentEntities(entities, parent, m_SiblingIndex);
            TransformComponent& transform = restored.GetTransform();
            transform.SetPosition(m_LocalPosition);
            transform.SetRotation(m_LocalRotation);
            transform.SetScale(m_LocalScale);
            restored.NotifyTransformChanged();
        }
        return restored;
    }

    Entity EntitySnapshot::Destroy() const
    {
        Entity root = ResolveRoot();
        Entity parent = ResolveParent();
        if (root)
            m_Scene->DestroyEntity(root);
        return parent;
    }

    Entity EntitySnapshot::ResolveRoot() const { return m_Scene ? m_Scene->TryGetEntityFromUuid(m_RootUuid) : Entity{}; }

    Entity EntitySnapshot::ResolveParent() const { return m_Scene ? m_Scene->TryGetEntityFromUuid(m_ParentUuid) : Entity{}; }

    EntityCreatedAction::EntityCreatedAction(Entity entity, const Ref<Scene>& scene)
      : UndoAction("Create entity"), m_Snapshot(entity, scene), m_Focus(entity)
    {
    }

    void EntityCreatedAction::Commit() { m_Focus = m_Snapshot.Restore(); }

    void EntityCreatedAction::Revert() { m_Focus = m_Snapshot.Destroy(); }

    EntityDeletedAction::EntityDeletedAction(Entity entity, const Ref<Scene>& scene)
      : UndoAction("Delete entity"), m_Snapshot(entity, scene), m_Focus(m_Snapshot.ResolveParent())
    {
    }

    void EntityDeletedAction::Commit() { m_Focus = m_Snapshot.Destroy(); }

    void EntityDeletedAction::Revert() { m_Focus = m_Snapshot.Restore(); }

    EntitiesDeletedAction::EntitiesDeletedAction(const Vector<Entity>& entities, const Ref<Scene>& scene)
      : UndoAction(entities.size() == 1u ? "Delete entity" : "Delete entities"), m_Scene(scene)
    {
        if (!m_Scene)
            return;
        const Vector<Entity> roots = NormalizeHierarchySelection(entities, m_Scene.get());
        m_Snapshots.reserve(roots.size());
        for (Entity entity : roots)
        {
            EntitySnapshot snapshot(entity, m_Scene);
            if (!m_Focus)
                m_Focus = snapshot.ResolveParent();
            m_Snapshots.push_back(std::move(snapshot));
        }
        std::sort(m_Snapshots.begin(), m_Snapshots.end(), [](const EntitySnapshot& lhs, const EntitySnapshot& rhs) {
            if (lhs.GetParentUuid() != rhs.GetParentUuid())
                return lhs.GetParentUuid() < rhs.GetParentUuid();
            return lhs.GetSiblingIndex() < rhs.GetSiblingIndex();
        });
    }

    void EntitiesDeletedAction::Commit()
    {
        if (!m_Scene)
            return;
        Vector<Entity> entities;
        entities.reserve(m_Snapshots.size());
        for (const EntitySnapshot& snapshot : m_Snapshots)
        {
            if (Entity entity = snapshot.ResolveRoot())
                entities.push_back(entity);
        }
        if (!entities.empty())
            m_Scene->DestroyEntities(entities);
        m_Focus = m_Snapshots.empty() ? Entity{} : m_Snapshots.front().ResolveParent();
    }

    void EntitiesDeletedAction::Revert()
    {
        for (const EntitySnapshot& snapshot : m_Snapshots)
            snapshot.Restore();
        m_Focus = m_Snapshots.empty() ? Entity{} : m_Snapshots.front().ResolveParent();
    }

    EntityReparentAction::EntityReparentAction(Entity entity, Entity oldParent, Entity newParent)
      : UndoAction("Reparent entity"), m_Scene(entity.GetScene()), m_Entity(entity.GetUuid()), m_OldParent(oldParent.GetUuid()),
        m_NewParent(newParent.GetUuid()), m_OldSibling(entity.GetSiblingIndex()), m_NewSibling(newParent.GetChildCount())
    {
    }

    void EntityReparentAction::Commit() { Reparent(m_NewParent, m_NewSibling); }

    void EntityReparentAction::Revert() { Reparent(m_OldParent, m_OldSibling); }

    void EntityReparentAction::Reparent(UUID targetUuid, uint32_t siblingIndex)
    {
        if (!m_Scene)
            return;
        Entity entity = m_Scene->TryGetEntityFromUuid(m_Entity);
        Entity parent = targetUuid.Empty() ? Entity{} : m_Scene->TryGetEntityFromUuid(targetUuid);
        if (entity && (targetUuid.Empty() || parent))
        {
            const Array<Entity, 1> entities{ entity };
            m_Scene->ReparentEntities(entities, parent, siblingIndex);
        }
    }

    EntitiesReparentAction::EntitiesReparentAction(const Vector<Entity>& entities, Entity newParent)
      : UndoAction(entities.size() == 1u ? "Reparent entity" : "Reparent entities"),
        m_Scene(newParent ? Ref<Scene>(newParent.GetScene())
                          : (!entities.empty() && entities.front() ? Ref<Scene>(entities.front().GetScene()) : Ref<Scene>{})),
        m_NewParentId(newParent ? newParent.GetUuid() : UUID::EMPTY),
        m_NewSiblingIndex(newParent ? newParent.GetChildCount() : 0u)
    {
        if (!m_Scene)
            return;
        const Vector<Entity> roots = NormalizeHierarchySelection(entities, m_Scene.get());
        m_Records.reserve(roots.size());
        for (Entity entity : roots)
        {
            const Entity oldParent = entity.GetParent();
            m_Records.push_back({ entity.GetUuid(), oldParent ? oldParent.GetUuid() : UUID::EMPTY, entity.GetSiblingIndex() });
        }

        m_RevertOrder.reserve(m_Records.size());
        for (uint32_t index = 0u; index < m_Records.size(); index++)
            m_RevertOrder.push_back(index);
        std::sort(m_RevertOrder.begin(), m_RevertOrder.end(), [&](uint32_t lhs, uint32_t rhs) {
            const Record& first = m_Records[lhs];
            const Record& second = m_Records[rhs];
            if (first.OldParentId != second.OldParentId)
                return first.OldParentId < second.OldParentId;
            return first.OldSiblingIndex < second.OldSiblingIndex;
        });
    }

    void EntitiesReparentAction::Commit()
    {
        if (!m_Scene)
            return;
        const Entity parent = m_NewParentId.Empty() ? Entity{} : m_Scene->TryGetEntityFromUuid(m_NewParentId);
        if (!m_NewParentId.Empty() && !parent)
            return;
        Vector<Entity> entities;
        entities.reserve(m_Records.size());
        for (const Record& record : m_Records)
        {
            if (Entity entity = m_Scene->TryGetEntityFromUuid(record.EntityId))
                entities.push_back(entity);
        }
        if (!entities.empty())
            m_Scene->ReparentEntities(entities, parent, m_NewSiblingIndex);
    }

    void EntitiesReparentAction::Revert()
    {
        if (!m_Scene)
            return;
        for (const uint32_t recordIndex : m_RevertOrder)
        {
            const Record& record = m_Records[recordIndex];
            Entity entity = m_Scene->TryGetEntityFromUuid(record.EntityId);
            Entity parent = record.OldParentId.Empty() ? Entity{} : m_Scene->TryGetEntityFromUuid(record.OldParentId);
            if (!entity || (!record.OldParentId.Empty() && !parent))
                continue;
            const Array<Entity, 1> entities{ entity };
            m_Scene->ReparentEntities(entities, parent, record.OldSiblingIndex);
        }
    }

    Entity EntitiesReparentAction::GetFocusEntity() const
    {
        return m_Scene && !m_Records.empty() ? m_Scene->TryGetEntityFromUuid(m_Records.front().EntityId) : Entity{};
    }
} // namespace Crowny
