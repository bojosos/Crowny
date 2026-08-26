#include "cwepch.h"

#include "Editor/UndoRedo.h"

#include "Crowny/Serialization/CerealDataStreamArchive.h"
#include "Crowny/Serialization/SceneSerializer.h"
#include "Crowny/Serialization/ScriptSerializer.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <yaml-cpp/yaml.h>

namespace Crowny
{
    namespace
    {
        const String EmptyActionName;

        PersistedScriptState CloneScriptState(const PersistedScriptState& state, Scene* scene)
        {
            PersistedScriptState clone{ state.Identity, nullptr };
            if (state.Fields == nullptr)
                return clone;

            Ref<MemoryDataStream> output = CreateRef<MemoryDataStream>();
            BinaryDataStreamOutputArchive outputArchive(output);
            Ref<SerializableObject> fields = state.Fields;
            outputArchive(fields);

            ScriptSerializationSceneScope sceneScope(scene);
            Ref<MemoryDataStream> input = CreateRef<MemoryDataStream>(output->Data(), output->Size());
            BinaryDataStreamInputArchive inputArchive(input);
            inputArchive(clone.Fields);
            return clone;
        }

        void SerializeSubtree(SceneSerializer& serializer, YAML::Emitter& output, Entity entity)
        {
            if (!entity)
                return;
            serializer.SerializeEntity(output, entity);
            const Vector<Entity> children = entity.GetChildren();
            for (Entity child : children)
                SerializeSubtree(serializer, output, child);
        }
    } // namespace

    void UndoRedo::RegisterAction(const Ref<UndoAction>& action)
    {
        if (!action)
            return;

        m_RedoStack.clear();
        m_UndoStack.push_back(action);
        if (m_UndoStack.size() > MaxHistorySize)
            m_UndoStack.erase(m_UndoStack.begin(), m_UndoStack.begin() + (m_UndoStack.size() - MaxHistorySize));
    }

    Ref<UndoAction> UndoRedo::Undo()
    {
        CancelInteraction();
        if (m_UndoStack.empty())
            return {};

        Ref<UndoAction> action = m_UndoStack.back();
        m_UndoStack.pop_back();
        action->Revert();
        m_RedoStack.push_back(action);
        if (m_ActionApplied)
            m_ActionApplied(action);
        return action;
    }

    Ref<UndoAction> UndoRedo::Redo()
    {
        CancelInteraction();
        if (m_RedoStack.empty())
            return {};

        Ref<UndoAction> action = m_RedoStack.back();
        m_RedoStack.pop_back();
        action->Commit();
        m_UndoStack.push_back(action);
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

    void UndoRedo::BeginComponentScope(std::function<Ref<UndoAction>()> factory)
    {
        if (!m_InInteraction)
        {
            m_Factory = std::move(factory);
            m_RetainedFactory = nullptr;
        }
    }

    bool UndoRedo::BeginComponentScope(const Ref<RetainedUndoActionFactory>& factory)
    {
        if (m_InInteraction)
            return false;

        m_Factory = nullptr;
        m_RetainedFactory = factory;
        return true;
    }

    void UndoRedo::EndComponentScope()
    {
        if (!m_InInteraction)
        {
            m_Factory = nullptr;
            m_RetainedFactory = nullptr;
        }
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
        if (!HasComponentActionFactory() || interaction.ItemId == 0u)
            return;

        if (m_RetainedFactory != nullptr)
            m_RetainedFactory->BeforeItemInteraction(interaction);

        if (m_InInteraction)
        {
            if (interaction.ItemId != m_InteractionItemId)
                return;

            m_InteractionChanged |= interaction.Changed;
            if (interaction.DeactivatedAfterEdit || (!interaction.Active && !interaction.Activated))
                FinishInteraction();
            return;
        }

        if (interaction.Activated && interaction.Active)
        {
            m_InInteraction = true;
            m_InteractionItemId = interaction.ItemId;
            m_InteractionChanged = interaction.Changed;
            if (interaction.DeactivatedAfterEdit)
                FinishInteraction();
        }
        else if (interaction.Changed)
        {
            RegisterAction(CreateComponentAction());
        }
    }

    bool UndoRedo::HasComponentActionFactory() const { return m_RetainedFactory != nullptr || static_cast<bool>(m_Factory); }

    Ref<UndoAction> UndoRedo::CreateComponentAction() const
    {
        if (m_RetainedFactory != nullptr)
            return m_RetainedFactory->Build();
        return m_Factory ? m_Factory() : Ref<UndoAction>{};
    }

    void UndoRedo::FinishComponentScope(const Ref<RetainedUndoActionFactory>& factory)
    {
        if (m_RetainedFactory == factory)
            FinishInteraction();
    }

    void UndoRedo::CancelComponentScope(const Ref<RetainedUndoActionFactory>& factory)
    {
        if (m_RetainedFactory == factory)
            CancelInteraction();
    }

    void UndoRedo::FinishInteraction()
    {
        if (m_InteractionChanged && HasComponentActionFactory())
            RegisterAction(CreateComponentAction());
        CancelInteraction();
    }

    void UndoRedo::CancelInteraction()
    {
        m_Factory = nullptr;
        m_RetainedFactory = nullptr;
        m_InteractionItemId = 0u;
        m_InInteraction = false;
        m_InteractionChanged = false;
    }

    ChangeScriptComponentAction::ChangeScriptComponentAction(Entity entity, State oldState, String name)
      : UndoAction(std::move(name)), m_Scene(entity.GetScene()), m_Entity(entity.GetUuid()), m_OldState(std::move(oldState)),
        m_NewState(Capture(entity))
    {
    }

    ChangeScriptComponentAction::State ChangeScriptComponentAction::Capture(Entity entity)
    {
        State state;
        if (!entity || !entity.HasComponent<MonoScriptComponent>())
            return state;

        const MonoScriptComponent& component = entity.GetComponent<MonoScriptComponent>();
        state.reserve(component.Scripts.size());
        for (const MonoScript& script : component.Scripts)
            state.push_back(CloneScriptState(script.CapturePersistedState(), entity.GetScene()));
        return state;
    }

    void ChangeScriptComponentAction::CaptureNewState(Entity entity)
    {
        if (entity && entity.GetScene() == m_Scene && entity.GetUuid() == m_Entity)
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
        if (entity.HasComponent<MonoScriptComponent>())
        {
            for (const MonoScript& script : entity.GetComponent<MonoScriptComponent>().Scripts)
                currentTypes.push_back(script.GetTypeIdentity());
        }
        Vector<ScriptTypeIdentity> desiredTypes;
        desiredTypes.reserve(state.size());
        for (const PersistedScriptState& script : state)
            desiredTypes.push_back(script.Identity);

        if (currentTypes != desiredTypes)
        {
            for (const ScriptTypeIdentity& identity : currentTypes)
                m_Scene->RemoveScriptComponent(entity, identity);
            for (const PersistedScriptState& script : state)
                m_Scene->AddScriptComponent(entity, script);
        }

        if (!entity.HasComponent<MonoScriptComponent>())
            return;
        MonoScriptComponent& component = entity.GetComponent<MonoScriptComponent>();
        for (const PersistedScriptState& snapshot : state)
        {
            const auto found = std::find_if(component.Scripts.begin(), component.Scripts.end(),
                                            [&](const MonoScript& script) { return script.GetTypeIdentity() == snapshot.Identity; });
            if (found != component.Scripts.end())
                found->ApplyPersistedState(snapshot);
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
            return {};
        if (Entity existing = ResolveRoot())
            return existing;

        SceneSerializer serializer(m_Scene);
        serializer.DeserializeEntities(YAML::Load(m_Yaml));
        Entity restored = ResolveRoot();
        Entity parent = ResolveParent();
        if (restored && parent)
        {
            restored.SetParent(parent);
            restored.SetPosition(m_LocalPosition);
            restored.SetRotation(m_LocalRotation);
            restored.SetScale(m_LocalScale);
            restored.SetSiblingIndex(m_SiblingIndex);
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
        Entity parent = m_Scene->TryGetEntityFromUuid(targetUuid);
        if (entity && parent && entity.SetParent(parent))
            entity.SetSiblingIndex(siblingIndex);
    }
} // namespace Crowny
