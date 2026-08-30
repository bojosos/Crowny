#pragma once

#include "Editor/UndoRedo.h"

namespace Crowny
{
    // Retains backend-neutral state and stable entity/script identities only.
    // No Mono object, class, field, or inspector callable crosses a frame or domain reload.
    class ScriptInspectorTransaction final : public RetainedUndoActionFactory
    {
    public:
        void SetTarget(Entity entity);
        void CompleteFrame();

        void BeforeItemInteraction(const UndoItemInteraction& interaction) override;
        Ref<UndoAction> Build() const override;
        void Reset() override;

    private:
        Entity ResolveTarget() const;

        Scene* m_Scene = nullptr;
        UUID m_Entity = UUID::EMPTY;
        ChangeScriptComponentAction::State m_Before;
        mutable Ref<ChangeScriptComponentAction> m_PendingAction;
        bool m_HasBefore = false;
    };
} // namespace Crowny
