#pragma once

#include "Crowny/Ecs/Components.h"

#include <functional>
#include <optional>
#include <span>
#include <type_traits>
#include <utility>

namespace Crowny
{
    namespace SelectionPropertyDetail
    {
        template <typename T> bool ValuesEqual(const T& lhs, const T& rhs) { return lhs == rhs; }
        inline bool ValuesEqual(const glm::vec2& lhs, const glm::vec2& rhs) { return glm::all(glm::equal(lhs, rhs)); }
        inline bool ValuesEqual(const glm::vec3& lhs, const glm::vec3& rhs) { return glm::all(glm::equal(lhs, rhs)); }
        inline bool ValuesEqual(const glm::vec4& lhs, const glm::vec4& rhs) { return glm::all(glm::equal(lhs, rhs)); }

        template <typename T, bool Weak> bool ValuesEqual(const TAssetHandle<T, Weak>& lhs, const TAssetHandle<T, Weak>& rhs)
        {
            return lhs.GetHandleData() == rhs.GetHandleData() || (lhs.GetUUID() != UUID::EMPTY && lhs.GetUUID() == rhs.GetUUID());
        }

        struct Equal
        {
            template <typename T> bool operator()(const T& lhs, const T& rhs) const { return ValuesEqual(lhs, rhs); }
        };

        struct AllEntities
        {
            bool operator()(Entity) const { return true; }
        };

        template <typename Callable, typename... Args> bool InvokeMutation(const Callable& callable, Args&&... args)
        {
            if constexpr (std::is_void_v<std::invoke_result_t<Callable, Args...>>)
            {
                std::invoke(callable, std::forward<Args>(args)...);
                return true;
            }
            else
            {
                return static_cast<bool>(std::invoke(callable, std::forward<Args>(args)...));
            }
        }

        template <typename Setter, typename Value> bool InvokeSetter(const Setter& setter, Entity entity, const Value& value)
        {
            return InvokeMutation(setter, entity, value);
        }

        template <typename Writer, typename Parent, typename Value> bool InvokeWriter(const Writer& writer, Parent& parent, const Value& value)
        {
            return InvokeMutation(writer, parent, value);
        }
    } // namespace SelectionPropertyDetail

    // A live selection read. Primary is the first accessible value; Mixed describes
    // the whole accessible set, not merely the first two entities.
    template <typename T> struct SelectionPropertyValue
    {
        std::optional<T> Primary;
        bool Mixed = false;
        size_t TargetCount = 0u;

        explicit operator bool() const { return Primary.has_value(); }
    };

    struct SelectionPropertyWrite
    {
        size_t TargetCount = 0u;
        size_t ChangedCount = 0u;

        explicit operator bool() const { return ChangedCount != 0u; }
    };

    // Binds one logical property across an entity selection. Projections retain the
    // parent's read/modify/write behavior, so editing one field preserves every other
    // field independently on each entity.
    template <typename Value, typename Getter, typename Setter, typename Predicate, typename Equal> class SelectionProperty
    {
    public:
        using ValueType = Value;
        using SelectionPropertyMarker = void;

        SelectionProperty(std::span<const Entity> entities, StringView componentName, StringView propertyName, Getter getter, Setter setter,
                          Predicate predicate, Equal equal)
          : m_Entities(entities), m_ComponentName(componentName), m_PropertyName(propertyName), m_Getter(std::move(getter)),
            m_Setter(std::move(setter)), m_Predicate(std::move(predicate)), m_Equal(std::move(equal))
        {
        }

        SelectionPropertyValue<Value> Read() const
        {
            SelectionPropertyValue<Value> result;
            for (Entity entity : m_Entities)
            {
                if (!CanAccess(entity))
                    continue;

                Value value = Get(entity);
                if (!result.Primary)
                    result.Primary = std::move(value);
                else if (!m_Equal(*result.Primary, value))
                    result.Mixed = true;
                ++result.TargetCount;
            }
            return result;
        }

        SelectionPropertyWrite Assign(const Value& value) const
        {
            SelectionPropertyWrite result;
            for (Entity entity : m_Entities)
            {
                if (!CanAccess(entity))
                    continue;

                ++result.TargetCount;
                const Value previous = Get(entity);
                if (m_Equal(previous, value) || !Set(entity, value) || m_Equal(previous, Get(entity)))
                    continue;

                MarkOverridden(entity);
                ++result.ChangedCount;
            }
            return result;
        }

        // Both overloads are constrained on the reader so a string literal first argument
        // cannot be deduced as the Reader of the unnamed overload.
        template <typename Reader, typename Writer, typename ProjectedEqual = SelectionPropertyDetail::Equal>
            requires std::is_invocable_v<const Reader&, const Value&>
        auto Project(StringView propertyName, Reader reader, Writer writer, ProjectedEqual equal = {}) const
        {
            using Projected = std::remove_cvref_t<std::invoke_result_t<Reader, const Value&>>;
            const SelectionProperty parent = *this;
            auto getter = [parent, reader](Entity entity) { return static_cast<Projected>(std::invoke(reader, parent.Get(entity))); };
            auto setter = [parent, writer](Entity entity, const Projected& value) {
                Value parentValue = parent.Get(entity);
                if (!SelectionPropertyDetail::InvokeWriter(writer, parentValue, value))
                    return false;
                return parent.Set(entity, parentValue);
            };
            auto predicate = [parent](Entity entity) { return parent.CanAccess(entity); };
            return SelectionProperty<Projected, decltype(getter), decltype(setter), decltype(predicate), ProjectedEqual>(
              m_Entities, m_ComponentName, propertyName, std::move(getter), std::move(setter), std::move(predicate), std::move(equal));
        }

        template <typename Reader, typename Writer, typename ProjectedEqual = SelectionPropertyDetail::Equal>
            requires std::is_invocable_v<const Reader&, const Value&>
        auto Project(Reader reader, Writer writer, ProjectedEqual equal = {}) const
        {
            return Project(m_PropertyName, std::move(reader), std::move(writer), std::move(equal));
        }

        // Owner is deduced from the pointer so this declaration stays well-formed when Value is
        // a scalar (e.g. a projected float); "MemberType Value::*" would be a hard error there.
        template <typename MemberType, typename Owner>
            requires std::is_same_v<Owner, Value>
        auto Member(StringView propertyName, MemberType Owner::* member) const
        {
            return Project(
              propertyName, [member](const Value& value) { return value.*member; },
              [member](Value& value, const MemberType& memberValue) { value.*member = memberValue; });
        }

        auto Element(size_t index) const
        {
            using ElementType = std::remove_cvref_t<decltype(std::declval<Value>()[0])>;
            const auto element = static_cast<int>(index); // glm indexes with int; avoids a narrowing warning.
            return Project([element](const Value& value) -> ElementType { return value[element]; },
                           [element](Value& value, const ElementType& elementValue) { value[element] = elementValue; });
        }

    private:
        template <typename, typename, typename, typename, typename> friend class SelectionProperty;

        bool CanAccess(Entity entity) const { return entity && std::invoke(m_Predicate, entity); }
        Value Get(Entity entity) const { return static_cast<Value>(std::invoke(m_Getter, entity)); }
        bool Set(Entity entity, const Value& value) const { return SelectionPropertyDetail::InvokeSetter(m_Setter, entity, value); }

        void MarkOverridden(Entity entity) const
        {
            if (!entity.HasComponent<PrefabComponent>() || m_ComponentName.empty() || m_PropertyName.empty())
                return;

            String path;
            path.reserve(m_ComponentName.size() + 1u + m_PropertyName.size());
            path.append(m_ComponentName);
            path.push_back('.');
            path.append(m_PropertyName);
            entity.GetComponent<PrefabComponent>().MarkOverridden(std::move(path));
        }

        std::span<const Entity> m_Entities;
        StringView m_ComponentName;
        StringView m_PropertyName;
        Getter m_Getter;
        Setter m_Setter;
        Predicate m_Predicate;
        Equal m_Equal;
    };

    template <typename T>
    concept SelectionPropertyBinding = requires {
        typename std::remove_cvref_t<T>::SelectionPropertyMarker;
        typename std::remove_cvref_t<T>::ValueType;
    };

    template <typename Component> class InspectorComponentSelection
    {
    public:
        InspectorComponentSelection(std::span<const Entity> entities, StringView componentName) : m_Entities(entities), m_ComponentName(componentName)
        {
        }

        template <typename Member> auto Bind(StringView propertyName, Member Component::* member) const
        {
            auto getter = [member](Entity entity) { return std::as_const(entity.GetComponent<Component>()).*member; };
            auto setter = [member](Entity entity, const Member& value) { entity.GetComponent<Component>().*member = value; };
            return MakeProperty(propertyName, std::move(getter), std::move(setter), SelectionPropertyDetail::Equal{});
        }

        // Getters are read-only component functions. Setters may also take the owning
        // Entity when applying the value needs hierarchy, physics, or scene context.
        template <typename Getter, typename Setter, typename Equal = SelectionPropertyDetail::Equal>
        auto Bind(StringView propertyName, Getter getter, Setter setter, Equal equal = {}) const
        {
            using Value = std::remove_cvref_t<std::invoke_result_t<Getter, const Component&>>;
            auto entityGetter = [getter](Entity entity) -> Value { return std::invoke(getter, std::as_const(entity.GetComponent<Component>())); };
            auto entitySetter = [setter](Entity entity, const Value& value) {
                Component& component = entity.GetComponent<Component>();
                if constexpr (std::is_invocable_v<Setter, Component&, const Value&, Entity>)
                    return SelectionPropertyDetail::InvokeMutation(setter, component, value, entity);
                else
                {
                    static_assert(std::is_invocable_v<Setter, Component&, const Value&>,
                                  "Component property setters must accept (component, value) or (component, value, entity).");
                    return SelectionPropertyDetail::InvokeMutation(setter, component, value);
                }
            };
            return MakeProperty(propertyName, std::move(entityGetter), std::move(entitySetter), std::move(equal));
        }

        template <typename Getter, typename Equal = SelectionPropertyDetail::Equal> auto Inspect(Getter getter, Equal equal = {}) const
        {
            using Value = std::remove_cvref_t<std::invoke_result_t<Getter, const Component&>>;
            SelectionPropertyValue<Value> result;
            for (Entity entity : m_Entities)
            {
                if (!entity || !entity.HasComponent<Component>())
                    continue;
                Value value = std::invoke(getter, std::as_const(entity.GetComponent<Component>()));
                if (!result.Primary)
                    result.Primary = std::move(value);
                else if (!std::invoke(equal, *result.Primary, value))
                    result.Mixed = true;
                ++result.TargetCount;
            }
            return result;
        }

    private:
        template <typename Getter, typename Setter, typename Equal>
        auto MakeProperty(StringView propertyName, Getter getter, Setter setter, Equal equal) const
        {
            using Value = std::remove_cvref_t<std::invoke_result_t<Getter, Entity>>;
            auto predicate = [](Entity entity) { return entity.HasComponent<Component>(); };
            return SelectionProperty<Value, Getter, Setter, decltype(predicate), Equal>(m_Entities, m_ComponentName, propertyName, std::move(getter),
                                                                                        std::move(setter), std::move(predicate), std::move(equal));
        }

        std::span<const Entity> m_Entities;
        StringView m_ComponentName;
    };

    class InspectorSelection
    {
    public:
        InspectorSelection(std::span<const Entity> entities, StringView componentName) : m_Entities(entities), m_ComponentName(componentName) {}

        template <typename Component> InspectorComponentSelection<Component> Components() const
        {
            return InspectorComponentSelection<Component>(m_Entities, m_ComponentName);
        }

        template <typename Component, typename Member> auto Bind(StringView propertyName, Member Component::* member) const
        {
            return Components<Component>().Bind(propertyName, member);
        }

        template <typename Getter, typename Setter, typename Equal = SelectionPropertyDetail::Equal>
        auto Bind(StringView propertyName, Getter getter, Setter setter, Equal equal = {}) const
        {
            return BindWhere(propertyName, std::move(getter), std::move(setter), SelectionPropertyDetail::AllEntities{}, std::move(equal));
        }

        template <typename Getter, typename Setter, typename Predicate, typename Equal = SelectionPropertyDetail::Equal>
        auto BindWhere(StringView propertyName, Getter getter, Setter setter, Predicate predicate, Equal equal = {}) const
        {
            using Value = std::remove_cvref_t<std::invoke_result_t<Getter, Entity>>;
            return SelectionProperty<Value, Getter, Setter, Predicate, Equal>(m_Entities, m_ComponentName, propertyName, std::move(getter),
                                                                              std::move(setter), std::move(predicate), std::move(equal));
        }

    private:
        std::span<const Entity> m_Entities;
        StringView m_ComponentName;
    };
} // namespace Crowny
