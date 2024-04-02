#pragma once

#include "Crowny/Common/Uuid.h"
#include "Crowny/Scene/Scene.h"

#include <entt/entt.hpp>
#include <type_traits>

namespace Crowny
{
    class Entity;
    class ComponentEditor;
    class HierarchyPanelchyWindow;
    class ScriptableEntity;

    class TransformComponent;

    class EnttEntity
    {
    public:
        EnttEntity() = default;
        EnttEntity(entt::entity entity, Scene* scene);
        EnttEntity(const EnttEntity& other) = default;

        template <typename T, typename... Args> T& AddComponent(Args&&... args) const
        {
            static_assert(std::is_base_of<ComponentBase, T>::value, "T must be a Component");
            CW_ENGINE_ASSERT(!HasComponent<T>());
            return m_Scene->m_Registry.emplace<T>(m_EntityHandle, std::forward<Args>(args)...);
        }

        template <typename T, typename... Args> T& AddOrReplaceComponent(Args&&... args) const
        {
            static_assert(std::is_base_of<ComponentBase, T>::value, "T must be a Component");
            return m_Scene->m_Registry.emplace_or_replace<T>(m_EntityHandle, std::forward<Args>(args)...);
        }

        template <typename T, typename... Args> T& ReplaceComponent(Args&&... args)
        {
            static_assert(std::is_base_of<ComponentBase, T>::value, "T must be a Component");
            CW_ENGINE_ASSERT(HasComponent<T>());
            return m_Scene->m_Registry.replace<T>(m_EntityHandle, std::forward<Args>(args)...);
        }

        template <typename T> T& GetComponent() const
        {
            static_assert(std::is_base_of<ComponentBase, T>::value, "T must be a Component");
            CW_ENGINE_ASSERT(HasComponent<T>());
            return m_Scene->m_Registry.get<T>(m_EntityHandle);
        }
        template <typename T> T& AddOrGetComponent() const
        {
            static_assert(std::is_base_of<ComponentBase, T>::value, "T must be a Component");
            if (!HasComponent())
                return m_Scene->emplace<T>(m_EntityHandle);
            return m_Scene->m_Registry.get<T>(m_EntityHandle);
        }

        template <typename T> bool HasComponent() const
        {
            static_assert(std::is_base_of<ComponentBase, T>::value, "T must be a Component");
            return m_Scene->m_Registry.all_of<T>(m_EntityHandle);
        }
        template <typename... T> bool HasAnyComponents() const
        {
            static_assert(std::is_base_of<ComponentBase, T>::value, "T must be a Component");
            return m_Scene->m_Registry.any_of<T>(m_EntityHandle);
        }
        template <typename... T> bool HasComponents() const
        {
            static_assert(std::is_base_of<ComponentBase, T...>::value, "T must be a Component");
            return m_Scene->m_Registry.all_of<T>(m_EntityHandle);
        }

        template <typename T, typename... Others> void RemoveComponent()
        {
            static_assert(std::is_base_of<ComponentBase, T>::value, "T must be a Component");
            // static_assert(std::is_base_of<ComponentBase, Others...>::value, "T must be a Component");
            m_Scene->m_Registry.remove<T, Others...>(m_EntityHandle);
        }
        template <typename T> void RemoveComponentIfExists()
        {
            static_assert(std::is_base_of<ComponentBase, T>::value, "T must be a Component");
            m_Scene->m_Registry.remove_if_exists<T>(m_EntityHandle);
        }

        bool HasAnyComponents() const { return !m_Scene->m_Registry.orphan(m_EntityHandle); }

        entt::entity GetHandle() { return m_EntityHandle; }

        void Clear()
        {
            m_EntityHandle = entt::null;
            m_Scene = nullptr;
        }

        bool IsValid() const
        {
            return m_Scene && m_EntityHandle != entt::null && m_Scene->m_Registry.valid(m_EntityHandle);
        }

    protected:
        friend class Entity;
        entt::entity m_EntityHandle = entt::null;
        Scene* m_Scene = nullptr;
    };

    class Entity : public EnttEntity
    {
    public:
        Entity() = default;
        Entity(entt::entity entity, Scene* scene);
        Entity(const Entity& other) = default;

        void SetPosition(const glm::vec3& position);
        void SetRotation(const glm::quat& rotation);
        void SetScale(const glm::vec3& scale);

        void SetWorldPosition(const glm::vec3& position);
        void SetWorldRotation(const glm::quat& rotation);
        void SetWorldScale(const glm::vec3& scale);

        glm::vec3 GetWorldPosition() const;
        glm::quat GetWorldRotation() const;
        glm::vec3 GetWorldScale() const;
        glm::vec3 GetLocalPosition() const;
        glm::quat GetLocalRotation() const;
        glm::vec3 GetLocalScale() const;

        const Transform& GetWorldTransform() const;
        const Transform& GetLocalTransform() const;
        const glm::mat4& GetWorldMatrix() const;

        void AddChild(Entity entity);
        Entity GetChild(uint32_t index) const;
        bool IsChildOf(Entity other, bool directOnly = false);
        const Vector<Entity>& GetChildren() const;
        Vector<Entity>& GetChildren();
        uint32_t GetChildCount() const;
        Entity GetParent() const;

        // TODO: Set parent should take care of the transforms too.
        void SetParent(Entity entity);

        void Destroy(bool destroyChildren = true);

        // Helpers
        const UUID& GetUuid() const;
        const TransformComponent& GetTransform() const;
        TransformComponent& GetTransform();
        const String& GetName() const;
        operator bool() const { return IsValid(); }
        bool operator==(const EnttEntity& other) const
        {
            // CW_ASSERT(IsValid());
            // TODO: Why is the scene compare commented out?
            return m_EntityHandle == other.m_EntityHandle; //&& m_Scene == other.m_Scene;
        }

    private:
        template <typename Component> void NotifyTransformChangedComponent(const Transform& transform)
        {
            static_assert(std::is_base_of<ComponentBase, Component>::value, "T must be a Component");
            if (HasComponent<Component>())
                GetComponent<Component>().OnTransformChanged(transform);
            // Better safe than sorry.
            Vector<Entity> children = GetChildren();
            for (Entity& child : children)
                child.NotifyTransformChanged(transform);
        }

        template <typename... Component>
        void NotifyTransformChangedComponentWrapper(ComponentGroup<Component...> group, const Transform& transform)
        {
            ([&]() { NotifyTransformChangedComponent<Component>(transform); }(), ...);
        }

        void NotifyTransformChanged(const Transform& transform);

    private:
        friend class ComponentEditor;
        friend class HierarchyPanelchyWindow;
        friend class ScriptableEntity;
        friend struct std::hash<Entity>;
    };
} // namespace Crowny

namespace std
{
    template <> struct hash<Crowny::Entity>
    {
        size_t operator()(const Crowny::Entity& e) const { return (size_t)e.m_EntityHandle; }
    };
} // namespace std
