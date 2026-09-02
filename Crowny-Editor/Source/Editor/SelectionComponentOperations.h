#pragma once

#include "Crowny/Ecs/Entity.h"
#include "Crowny/Scene/Scene.h"
#include "Editor/UndoRedo.h"

#include <algorithm>
#include <functional>
#include <span>
#include <type_traits>

namespace Crowny
{
    struct SelectionComponentChange
    {
        size_t TargetCount = 0u;
        size_t ChangedCount = 0u;
        Ref<UndoAction> Action;

        explicit operator bool() const { return ChangedCount != 0u; }
    };

    namespace SelectionComponentOperationsDetail
    {
        inline bool VisitEntity(Entity entity, Vector<Entity>& visited)
        {
            if (!entity || std::find(visited.begin(), visited.end(), entity) != visited.end())
                return false;
            visited.push_back(entity);
            return true;
        }

        inline Ref<UndoAction> MakeAction(Vector<Ref<UndoAction>> actions, StringView pluralName)
        {
            if (actions.empty())
                return {};
            if (actions.size() == 1u)
                return actions.front();

            Ref<UndoActionGroup> group = CreateRef<UndoActionGroup>(String(pluralName));
            for (Ref<UndoAction>& action : actions)
                group->Add(std::move(action));
            return group;
        }
    } // namespace SelectionComponentOperationsDetail

    // Adds and initializes a native component only where it is missing. Existing
    // component state is never used as a template for the rest of the selection.
    template <typename Component, typename Initialize>
    SelectionComponentChange AddComponentToSelection(std::span<const Entity> entities, Initialize&& initialize)
    {
        static_assert(std::is_base_of_v<ComponentBase, Component>, "Component must derive from ComponentBase.");
        static_assert(!std::is_same_v<Component, ManagedScriptComponent>,
                      "Managed scripts carry runtime identity and must use AddManagedScriptToSelection.");

        SelectionComponentChange result;
        Vector<Entity> visited;
        Vector<Ref<UndoAction>> actions;
        visited.reserve(entities.size());
        actions.reserve(entities.size());
        for (Entity entity : entities)
        {
            if (!SelectionComponentOperationsDetail::VisitEntity(entity, visited))
                continue;

            ++result.TargetCount;
            if (entity.HasComponent<Component>())
                continue;

            Component& component = entity.AddComponent<Component>();
            std::invoke(initialize, entity, component);
            actions.push_back(CreateRef<AddComponentAction<Component>>(entity));
            ++result.ChangedCount;
        }

        result.Action = SelectionComponentOperationsDetail::MakeAction(std::move(actions), "Add components");
        return result;
    }

    template <typename Component> SelectionComponentChange AddComponentToSelection(std::span<const Entity> entities)
    {
        return AddComponentToSelection<Component>(entities, [](Entity, Component&) {});
    }

    // Captures the complete script list before and after each attach. This keeps
    // constructor side effects, serialized fields, and other scripts intact through undo.
    inline SelectionComponentChange AddManagedScriptToSelection(std::span<const Entity> entities, const ScriptTypeIdentity& identity,
                                                                bool initialize = true)
    {
        SelectionComponentChange result;
        if (!identity.IsValid())
            return result;

        Vector<Entity> visited;
        Vector<Ref<UndoAction>> actions;
        visited.reserve(entities.size());
        actions.reserve(entities.size());
        for (Entity entity : entities)
        {
            if (!SelectionComponentOperationsDetail::VisitEntity(entity, visited))
                continue;

            ++result.TargetCount;
            Scene* scene = entity.GetScene();
            if (scene == nullptr || scene->HasScriptComponent(entity, identity))
                continue;

            ChangeScriptComponentAction::State before = ChangeScriptComponentAction::Capture(entity);
            scene->AddScriptComponent(entity, identity, initialize);
            const ChangeScriptComponentAction::State after = ChangeScriptComponentAction::Capture(entity);
            if (before == after)
                continue;

            actions.push_back(CreateRef<ChangeScriptComponentAction>(entity, std::move(before), "Add script"));
            ++result.ChangedCount;
        }

        result.Action = SelectionComponentOperationsDetail::MakeAction(std::move(actions), "Add scripts");
        return result;
    }
} // namespace Crowny
