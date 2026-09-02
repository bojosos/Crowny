#pragma once

#include "Editor/ScriptInspectorTransaction.h"
#include "Panels/EntityInspector.h"

namespace Crowny
{
    // Inspector integration for ManagedScriptComponent. Scripts differ from built-in
    // components in every customization point: the widget draws one collapsing header
    // per attached script, undo is recorded as a script-list transaction rather than a
    // component snapshot, adding happens per script type from the Add Component browser,
    // and removal captures the whole script list.
    template <> void ComponentEditorWidget<ManagedScriptComponent>(Entity entity);
    template <> SelectionComponentChange ComponentSelectionAddAction<ManagedScriptComponent>(std::span<const Entity> entities);
    template <> Ref<UndoAction> ComponentRemoveAction<ManagedScriptComponent>(Entity entity);

    template <> struct ComponentInspectorTraits<ManagedScriptComponent>
    {
        static constexpr bool ListedInAddMenu = false; // Scripts are listed individually by SynchronizeScriptCatalog.
        static constexpr bool DrawsOwnHeader = true;

        static Ref<RetainedUndoActionFactory> CreateUndoFactory() { return CreateRef<ScriptInspectorTransaction>(); }

        static void RenderWithUndo(const Ref<RetainedUndoActionFactory>& factory, const ComponentWidgetCallback& widget, Entity primary,
                                   const Vector<Entity>& entities)
        {
            if (entities.size() != 1u)
            {
                widget(primary, entities);
                return;
            }
            Ref<ScriptInspectorTransaction> transaction = StaticRefCast<ScriptInspectorTransaction>(factory);
            transaction->SetTarget(primary);
            UndoRedo::Get().BeginComponentScope(transaction);
            widget(primary, entities);
            transaction->CompleteFrame();
            UndoRedo::Get().EndComponentScope();
        }
    };

    namespace ScriptComponentInspector
    {
        // Mirrors the runtime script catalog into the Add Component menu when it changed.
        void SynchronizeScriptCatalog(ComponentMenuModel& menu);

        // Whether `value` can be used as a C# class name for a new script.
        bool IsValidScriptClassName(const String& value);

        // Creates <className>.cs from the default template in the project's asset folder and
        // attaches the (not yet compiled) script to every selected entity as one undo action.
        bool CreateNewScript(const Vector<Entity>& entities, const String& className);
    } // namespace ScriptComponentInspector
} // namespace Crowny
