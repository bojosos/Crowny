#pragma once

#include "Crowny/Ecs/Entity.h"
#include "Editor/ComponentUndoSnapshot.h"

#include <imgui.h>

#include <entt/entt.hpp>

namespace Crowny
{

    template <class Component> void ComponentEditorWidget(Entity entity) {}

    template <class Component> void ComponentSelectionEditorWidget(Entity primary, const Vector<Entity>& entities)
    {
        if (entities.size() == 1u)
        {
            ComponentEditorWidget<Component>(primary);
            return;
        }
        ImGui::Columns(1);
        ImGui::TextDisabled("This component cannot be edited across multiple entities yet.");
        ImGui::Columns(2);
    }

    template <class Component> Ref<UndoAction> ComponentAddAction(Entity entity)
    {
        entity.AddComponent<Component>();
        return CreateRef<Crowny::AddComponentAction<Component>>(entity);
    }

    template <class Component> Ref<UndoAction> ComponentRemoveAction(Entity entity)
    {
        if constexpr (std::is_same_v<Component, MonoScriptComponent>)
        {
            ChangeScriptComponentAction::State snapshot = ChangeScriptComponentAction::Capture(entity);
            entity.RemoveComponent<Component>();
            return CreateRef<ChangeScriptComponentAction>(entity, std::move(snapshot), "Remove scripts");
        }
        else
        {
            auto comp = entity.GetComponent<Component>();
            entity.RemoveComponent<Component>();
            return CreateRef<Crowny::RemoveComponentAction<Component>>(entity, comp);
        }
    }

    class ComponentEditor
    {
    public:
        using ComponentTypeID = entt::id_type;

        struct ComponentInfo
        {
            using Callback = std::function<void(Entity, const Vector<Entity>&)>;
            using SingleCallback = std::function<void(Entity)>;
            using ActionCallback = std::function<Ref<UndoAction>(Entity)>;
            String name;
            Callback widget;
            ActionCallback create, destroy;
            Ref<RetainedUndoActionFactory> undoFactory;
        };

    public:
        template <class Component> ComponentInfo& RegisterComponent(const ComponentInfo& componentInfo)
        {
            auto hash = entt::type_hash<Component>::value();
            auto [it, res] = m_ComponentInfos[m_CurrentComponentGroup].insert_or_assign(hash, componentInfo);
            CW_ENGINE_ASSERT(res);
            m_OrderedComponentInfos.push_back(std::make_pair(hash, componentInfo));
            return std::get<ComponentInfo>(*it);
        }

        template <class Component> ComponentInfo& RegisterComponent(const String& name, typename ComponentInfo::SingleCallback widget)
        {
            if constexpr (std::is_same_v<Component, MonoScriptComponent>)
            {
                auto wrappedWidget = [widget](Entity primary, const Vector<Entity>& entities) {
                    if (entities.size() != 1u)
                    {
                        ImGui::Columns(1);
                        ImGui::TextDisabled("This component cannot be edited across multiple entities yet.");
                        ImGui::Columns(2);
                        return;
                    }
                    ChangeScriptComponentAction::State snapshot = ChangeScriptComponentAction::Capture(primary);
                    UndoRedo::Get().BeginComponentScope([primary, snapshot]() mutable -> Ref<UndoAction> {
                        return CreateRef<ChangeScriptComponentAction>(primary, std::move(snapshot));
                    });
                    widget(primary);
                    UndoRedo::Get().EndComponentScope();
                };
                return RegisterComponent<Component>(ComponentInfo{
                  name,
                  wrappedWidget,
                  ComponentAddAction<Component>,
                  ComponentRemoveAction<Component>,
                });
            }
            else
            {
                Ref<ComponentUndoSnapshot<Component>> snapshots = CreateRef<ComponentUndoSnapshot<Component>>();
                auto wrappedWidget = [widget, snapshots](Entity primary, const Vector<Entity>& entities) {
                    if (entities.size() != 1u)
                    {
                        ImGui::Columns(1);
                        ImGui::TextDisabled("This component cannot be edited across multiple entities yet.");
                        ImGui::Columns(2);
                        return;
                    }
                    UndoRedo& undoRedo = UndoRedo::Get();
                    if (undoRedo.BeginComponentScope(snapshots))
                        snapshots->Capture(entities);
                    widget(primary);
                    undoRedo.EndComponentScope();
                };
                return RegisterComponent<Component>(ComponentInfo{
                  name,
                  wrappedWidget,
                  ComponentAddAction<Component>,
                  ComponentRemoveAction<Component>,
                  snapshots,
                });
            }
        }

        template <class Component> ComponentInfo& RegisterComponent(const String& name)
        {
            if constexpr (std::is_same_v<Component, MonoScriptComponent>)
            {
                auto widget = [](Entity primary, const Vector<Entity>& entities) {
                    if (entities.size() != 1u)
                    {
                        ComponentSelectionEditorWidget<Component>(primary, entities);
                        return;
                    }
                    ChangeScriptComponentAction::State snapshot = ChangeScriptComponentAction::Capture(primary);
                    UndoRedo::Get().BeginComponentScope([primary, snapshot]() mutable -> Ref<UndoAction> {
                        return CreateRef<ChangeScriptComponentAction>(primary, std::move(snapshot));
                    });
                    ComponentSelectionEditorWidget<Component>(primary, entities);
                    UndoRedo::Get().EndComponentScope();
                };
                return RegisterComponent<Component>(ComponentInfo{
                  name,
                  widget,
                  ComponentAddAction<Component>,
                  ComponentRemoveAction<Component>,
                });
            }
            else
            {
                Ref<ComponentUndoSnapshot<Component>> snapshots = CreateRef<ComponentUndoSnapshot<Component>>();
                auto widget = [snapshots](Entity primary, const Vector<Entity>& entities) {
                    UndoRedo& undoRedo = UndoRedo::Get();
                    if (undoRedo.BeginComponentScope(snapshots))
                        snapshots->Capture(entities);
                    ComponentSelectionEditorWidget<Component>(primary, entities);
                    undoRedo.EndComponentScope();
                };
                return RegisterComponent<Component>(ComponentInfo{
                  name,
                  widget,
                  ComponentAddAction<Component>,
                  ComponentRemoveAction<Component>,
                  snapshots,
                });
            }
        }

        void PushComponentGroup(const String& name) { m_CurrentComponentGroup = name; }

        void PopComponentGroup() { m_CurrentComponentGroup.clear(); }

        void Render(Entity primary, const Vector<Entity>& entities);

    private:
        void ResetUndoSnapshots(bool finishInteraction);

        bool EntityHasComponent(const entt::registry& registry, const Entity& entity, const ComponentTypeID tid) const
        {
            for (auto [id, storage] : registry.storage())
            {
                if (id == tid)
                    return storage.contains(entity.GetHandle());
            }
            return false;
        }

    private:
        Vector<Pair<ComponentTypeID, ComponentInfo>> m_OrderedComponentInfos;
        Map<String, Map<ComponentTypeID, ComponentInfo>> m_ComponentInfos;
        String m_CurrentComponentGroup;
        Vector<Entity> m_SelectionScratch;
        Vector<UUID> m_UndoSelection;
        Scene* m_UndoScene = nullptr;
        Ref<ComponentUndoSnapshot<TagComponent>> m_TagSnapshots = CreateRef<ComponentUndoSnapshot<TagComponent>>();
    };

    template <> void ComponentEditorWidget<MonoScriptComponent>(Entity entity);
    template <> void ComponentSelectionEditorWidget<TransformComponent>(Entity primary, const Vector<Entity>& entities);
    template <> void ComponentSelectionEditorWidget<CameraComponent>(Entity primary, const Vector<Entity>& entities);
    template <> void ComponentSelectionEditorWidget<LightComponent>(Entity primary, const Vector<Entity>& entities);
    template <> void ComponentSelectionEditorWidget<TextComponent>(Entity primary, const Vector<Entity>& entities);
    template <> void ComponentSelectionEditorWidget<SpriteRendererComponent>(Entity primary, const Vector<Entity>& entities);
    template <> void ComponentSelectionEditorWidget<MeshRendererComponent>(Entity primary, const Vector<Entity>& entities);
    template <> void ComponentSelectionEditorWidget<AnimationComponent>(Entity primary, const Vector<Entity>& entities);
    template <> void ComponentSelectionEditorWidget<Rigidbody2DComponent>(Entity primary, const Vector<Entity>& entities);
    template <> void ComponentSelectionEditorWidget<BoxCollider2DComponent>(Entity primary, const Vector<Entity>& entities);
    template <> void ComponentSelectionEditorWidget<CircleCollider2DComponent>(Entity primary, const Vector<Entity>& entities);
    template <> void ComponentSelectionEditorWidget<Rigidbody3DComponent>(Entity primary, const Vector<Entity>& entities);
    template <> void ComponentSelectionEditorWidget<BoxCollider3DComponent>(Entity primary, const Vector<Entity>& entities);
    template <> void ComponentSelectionEditorWidget<SphereCollider3DComponent>(Entity primary, const Vector<Entity>& entities);
    template <> void ComponentSelectionEditorWidget<CapsuleCollider3DComponent>(Entity primary, const Vector<Entity>& entities);
    template <> void ComponentSelectionEditorWidget<AudioListenerComponent>(Entity primary, const Vector<Entity>& entities);
    template <> void ComponentSelectionEditorWidget<AudioSourceComponent>(Entity primary, const Vector<Entity>& entities);

} // namespace Crowny
