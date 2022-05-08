#pragma once

#include "Crowny/Ecs/Entity.h"

#include <entt/entt.hpp>

namespace Crowny
{

    template <class Component> void ComponentEditorWidget(Entity entity) {}

    template <class Component> void ComponentAddAction(Entity entity) { entity.AddComponent<Component>(); }

    template <class Component> void ComponentRemoveAction(Entity entity) { entity.RemoveComponent<Component>(); }

    class ComponentEditor
    {
    public:
        using ComponentTypeID = ENTT_ID_TYPE;

        struct ComponentInfo
        {
            using Callback = std::function<void(Entity)>;
            String name;
            Callback widget, create, destroy;
        };

    public:
        template <class Component> ComponentInfo& RegisterComponent(const ComponentInfo& componentInfo)
        {
            auto index = entt::type_info<Component>::id();
            auto [it, res] = m_ComponentInfos[m_CurrentComponentGroup].insert_or_assign(index, componentInfo);
            CW_ENGINE_ASSERT(res);
            m_OrderedComponentInfos.push_back(std::make_pair(index, componentInfo));
            return std::get<ComponentInfo>(*it);
        }

        template <class Component>
        ComponentInfo& RegisterComponent(const String& name, typename ComponentInfo::Callback widget)
        {
            return RegisterComponent<Component>(ComponentInfo{
              name,
              widget,
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
        bool EntityHasComponent(const entt::registry& registry, Entity entity, ComponentTypeID tid)
        {
            ComponentTypeID type[] = { tid };
            return registry.runtime_view(std::cbegin(type), std::cend(type)).contains(entity.GetHandle());
        }

    private:
        Vector<std::pair<ComponentTypeID, ComponentInfo>> m_OrderedComponentInfos;
        Map<String, Map<ComponentTypeID, ComponentInfo>> m_ComponentInfos;
        String m_CurrentComponentGroup;
    };

} // namespace Crowny
