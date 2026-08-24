#pragma once

#include "Crowny/Ecs/Entity.h"
#include "Editor/UndoRedo.h"

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
            auto wrappedWidget = [widget](Entity primary, const Vector<Entity>& entities) {
                if (entities.size() != 1u)
                {
                    ImGui::Columns(1);
                    ImGui::TextDisabled("This component cannot be edited across multiple entities yet.");
                    ImGui::Columns(2);
                    return;
                }
                if constexpr (std::is_same_v<Component, MonoScriptComponent>)
                {
                    ChangeScriptComponentAction::State snapshot = ChangeScriptComponentAction::Capture(primary);
                    UndoRedo::Get().BeginComponentScope([primary, snapshot]() mutable -> Ref<UndoAction> {
                        return CreateRef<ChangeScriptComponentAction>(primary, std::move(snapshot));
                    });
                }
                else
                {
                    Component snapshot = primary.GetComponent<Component>();
                    UndoRedo::Get().BeginComponentScope([primary, snapshot]() -> Ref<UndoAction> {
                        return CreateRef<ChangeComponentAction<Component>>(primary, snapshot, primary.GetComponent<Component>());
                    });
                }
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

        template <class Component> ComponentInfo& RegisterComponent(const String& name)
        {
            auto widget = [](Entity primary, const Vector<Entity>& entities) {
                if constexpr (std::is_same_v<Component, MonoScriptComponent>)
                {
                    if (entities.size() != 1u)
                    {
                        ComponentSelectionEditorWidget<Component>(primary, entities);
                        return;
                    }
                    ChangeScriptComponentAction::State snapshot = ChangeScriptComponentAction::Capture(primary);
                    UndoRedo::Get().BeginComponentScope([primary, snapshot]() mutable -> Ref<UndoAction> {
                        return CreateRef<ChangeScriptComponentAction>(primary, std::move(snapshot));
                    });
                }
                else
                {
                    Vector<Pair<Entity, Component>> snapshots;
                    snapshots.reserve(entities.size());
                    for (Entity entity : entities)
                    {
                        if (entity && entity.HasComponent<Component>())
                            snapshots.emplace_back(entity, entity.GetComponent<Component>());
                    }
                    UndoRedo::Get().BeginComponentScope(
                      [snapshots]() -> Ref<UndoAction> { return CreateRef<ChangeComponentsAction<Component>>(snapshots); });
                }
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

        void PushComponentGroup(const String& name) { m_CurrentComponentGroup = name; }

        void PopComponentGroup() { m_CurrentComponentGroup.clear(); }

        void Render(Entity primary, const Vector<Entity>& entities);

    private:
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
