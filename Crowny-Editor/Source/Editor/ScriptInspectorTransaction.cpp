#include "cwepch.h"

#include "Editor/ScriptInspectorTransaction.h"

namespace Crowny
{
    void ScriptInspectorTransaction::SetTarget(Entity entity)
    {
        Scene* const scene = entity ? entity.GetScene() : nullptr;
        const UUID entityId = entity ? entity.GetUuid() : UUID::EMPTY;
        if (m_Scene == scene && m_Entity == entityId)
            return;

        Reset();
        m_Scene = scene;
        m_Entity = entityId;
    }

    void ScriptInspectorTransaction::CompleteFrame()
    {
        if (m_PendingAction == nullptr)
            return;

        m_PendingAction->CaptureNewState(ResolveTarget());
        m_PendingAction = nullptr;
        m_Before.clear();
        m_HasBefore = false;
    }

    void ScriptInspectorTransaction::BeforeItemInteraction(const UndoItemInteraction& interaction)
    {
        if (!interaction.Changed || m_HasBefore)
            return;

        Entity target = ResolveTarget();
        if (!target)
            return;

        m_Before = ChangeScriptComponentAction::Capture(target);
        m_HasBefore = true;
    }

    Ref<UndoAction> ScriptInspectorTransaction::Build() const
    {
        if (m_PendingAction != nullptr)
            return {};

        Entity target = ResolveTarget();
        if (!target || !m_HasBefore)
            return {};

        m_PendingAction = CreateRef<ChangeScriptComponentAction>(target, m_Before);
        return m_PendingAction;
    }

    void ScriptInspectorTransaction::Reset()
    {
        m_Before.clear();
        m_PendingAction = nullptr;
        m_HasBefore = false;
    }

    Entity ScriptInspectorTransaction::ResolveTarget() const { return m_Scene != nullptr ? m_Scene->TryGetEntityFromUuid(m_Entity) : Entity{}; }
} // namespace Crowny
