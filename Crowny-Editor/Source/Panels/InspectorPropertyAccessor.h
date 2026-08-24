#pragma once

#include "Crowny/Ecs/Components.h"
#include "UI/Properties.h"

#include <type_traits>

namespace Crowny
{
    namespace InspectorPropertyDetail
    {
        template <typename T> bool ValuesEqual(const T& lhs, const T& rhs) { return lhs == rhs; }
        inline bool ValuesEqual(const glm::vec2& lhs, const glm::vec2& rhs) { return glm::all(glm::equal(lhs, rhs)); }
        inline bool ValuesEqual(const glm::vec3& lhs, const glm::vec3& rhs) { return glm::all(glm::equal(lhs, rhs)); }
        inline bool ValuesEqual(const glm::vec4& lhs, const glm::vec4& rhs) { return glm::all(glm::equal(lhs, rhs)); }

        template <typename T, bool Weak> bool ValuesEqual(const TAssetHandle<T, Weak>& lhs, const TAssetHandle<T, Weak>& rhs)
        {
            return lhs.GetHandleData() == rhs.GetHandleData() || (lhs.GetUUID() != UUID::EMPTY && lhs.GetUUID() == rhs.GetUUID());
        }
    } // namespace InspectorPropertyDetail

    class InspectorPropertyAccessor
    {
    public:
        InspectorPropertyAccessor(const Vector<Entity>& entities, const char* componentName) : m_Entities(entities), m_ComponentName(componentName) {}

        bool IsMultiple() const { return m_Entities.size() > 1u; }

        template <typename Value, typename Getter> bool IsMixed(const Value& primaryValue, Getter&& getter) const
        {
            return std::any_of(m_Entities.begin(), m_Entities.end(),
                               [&](Entity entity) { return entity && !InspectorPropertyDetail::ValuesEqual(primaryValue, getter(entity)); });
        }

        template <typename Getter> bool IsMixed(Getter&& getter) const
        {
            if (m_Entities.empty())
                return false;
            return IsMixed(getter(m_Entities.front()), std::forward<Getter>(getter));
        }

        template <typename Value, typename Getter, typename Setter, typename Drawer>
        bool Edit(const char* propertyName, Getter&& getter, Setter&& setter, Drawer&& drawer) const
        {
            if (m_Entities.empty())
                return false;

            Value value = getter(m_Entities.front());
            const bool mixed = IsMixed(value, getter);
            if constexpr (std::is_same_v<Value, String>)
            {
                if (mixed)
                    value.clear();
            }

            if (mixed)
                ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, true);
            const bool changed = drawer(value);
            if (mixed)
                ImGui::PopItemFlag();

            if (changed)
                Assign(propertyName, [&](Entity entity) { setter(entity, value); });
            return changed;
        }

        template <typename Setter> void Assign(const char* propertyName, Setter&& setter) const
        {
            for (Entity entity : m_Entities)
            {
                if (!entity)
                    continue;
                setter(entity);
                if (entity.HasComponent<PrefabComponent>())
                    entity.GetComponent<PrefabComponent>().MarkOverridden(m_ComponentName + "." + propertyName);
            }
        }

    private:
        const Vector<Entity>& m_Entities;
        String m_ComponentName;
    };
} // namespace Crowny
