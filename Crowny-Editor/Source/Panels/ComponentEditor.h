#pragma once

#include "Crowny/Ecs/Entity.h"
#include "Editor/EditorLayer.h"

#include <entt/entt.hpp>

namespace Crowny
{

    template <class Component> void ComponentEditorWidget(Entity entity) {}

    template <class Component> Ref<UndoAction> ComponentAddAction(Entity entity)
    {
        entity.AddComponent<Component>();
        return CreateRef<Crowny::AddComponentAction<Component>>(entity);
    }

    template <class Component> Ref<UndoAction> ComponentRemoveAction(Entity entity)
    {
        auto comp = entity.GetComponent<Component>();
        entity.RemoveComponent<Component>();
        return CreateRef<Crowny::RemoveComponentAction<Component>>(entity, comp);
    }

    class ComponentEditor
    {
    public:
        using ComponentTypeID = entt::id_type;

        struct ComponentInfo
        {
            using Callback = std::function<void(Entity)>;
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

        template <class Component> ComponentInfo& RegisterComponent(const String& name, typename ComponentInfo::Callback widget)
        {
            auto wrappedWidget = [widget](Entity entity) {
                Component snapshot = entity.GetComponent<Component>();
                UndoRedo::Get().BeginComponentScope([entity, snapshot]() -> Ref<UndoAction> {
                    return CreateRef<ChangeComponentAction<Component>>(entity, snapshot, entity.GetComponent<Component>());
                });
                widget(entity);
                UndoRedo::Get().EndComponentScope();
            };
            return RegisterComponent<Component>(ComponentInfo{
              name,
              wrappedWidget,
              ComponentAddAction<Component>,
              ComponentRemoveAction<Component>,
            });
        }

        void PushComponentGroup(const String& name) { m_CurrentComponentGroup = name; }

        void PopComponentGroup() { m_CurrentComponentGroup.clear(); }

        template <class Component> ComponentInfo& RegisterComponent(const String& name)
        {
            return RegisterComponent<Component>(name, ComponentEditorWidget<Component>);
        }

        void Render();

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
        Vector<std::pair<ComponentTypeID, ComponentInfo>> m_OrderedComponentInfos;
        Map<String, Map<ComponentTypeID, ComponentInfo>> m_ComponentInfos;
        String m_CurrentComponentGroup;
    };

} // namespace Crowny
