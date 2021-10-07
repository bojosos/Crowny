#include "cwpch.h"

#include "Crowny/Scene/Scene.h"

#include "Crowny/Common/Uuid.h"
#include "Crowny/Ecs/Components.h"
#include "Crowny/Ecs/Entity.h"
#include "Crowny/RenderAPI/RenderCommand.h"
#include "Crowny/Renderer/ForwardPlusRenderer.h"
#include "Crowny/Renderer/ForwardRenderer.h"
#include "Crowny/Renderer/Renderer.h"
#include "Crowny/Renderer/Renderer2D.h"
#include "Crowny/Scene/SceneManager.h"

#include <entt/entt.hpp>

namespace Crowny
{

    template <typename... Component>
    static void CopyComponent(entt::registry& dst, entt::registry& src,
                              const UnorderedMap<UUID, entt::entity>& entityMap)
    {
        (
          [&]() {
              auto view = src.view<Component>();
              for (auto srcEntity : view)
              {
                  entt::entity dstEntity = entityMap.at(src.get<IDComponent>(srcEntity).Uuid);
                  auto& srcComponent = src.get<Component>(srcEntity);
                  dst.emplace_or_replace<Component>(dstEntity, srcComponent);
              }
          }(),
          ...);
    }

    template <typename... Component>
    static void CopyComponent(ComponentGroup<Component...>, entt::registry& dst, entt::registry& src,
                              const UnorderedMap<UUID, entt::entity>& entityMap)
    {
        CopyComponent<Component...>(dst, src, entityMap);
    }

    template <typename... Component> static void CopyComponentIfExists(Entity dst, Entity src)
    {
        (
          [&]() {
              if (src.HasComponent<Component>())
                  dst.AddOrReplaceComponent<Component>(src.GetComponent<Component>());
          }(),
          ...);
    }

    template <typename... Component>
    static void CopyComponentIfExists(ComponentGroup<Component...>, Entity dst, Entity src)
    {
        CopyComponentIfExists<Component...>(dst, src);
    }

    static void CopyAllComponents(entt::registry& dstRegistry, entt::registry& srcRegistry,
                                  const UnorderedMap<UUID, entt::entity>& entityMap)
    {
        CopyComponent(AllComponents{}, dstRegistry, srcRegistry, entityMap);
    }

    static void CopyAllExistingComponents(Entity dst, Entity src) { CopyComponentIfExists(AllComponents{}, dst, src); }

    Scene::Scene(const String& name) : m_Name(name)
    {
        m_RootEntity = new Entity(m_Registry.create(), this);

        m_RootEntity->AddComponent<IDComponent>(UuidGenerator::Generate());
        m_RootEntity->AddComponent<TagComponent>(m_Name);
        m_RootEntity->AddComponent<RelationshipComponent>();
    }

    Scene::Scene(Scene& other)
    {
        m_ViewportWidth = other.m_ViewportWidth;
        m_ViewportHeight = other.m_ViewportHeight;
        m_Filepath = other.m_Filepath;
        m_Name = other.m_Name;

        m_RootEntity = new Entity(m_Registry.create(), this);

        UnorderedMap<UUID, entt::entity> entityMap;

        auto idView = m_Registry.view<IDComponent>();
        for (auto e : idView)
        {
            const UUID& uuid = other.m_Registry.get<IDComponent>(e).Uuid;
            const String& name = other.m_Registry.get<TagComponent>(e).Tag;
            Entity newEntity = CreateEntityWithUuid(uuid, name);
            entityMap[uuid] = e;
        }

        CopyAllComponents(m_Registry, other.m_Registry, entityMap);
    }

    Scene& Scene::operator=(Scene& other)
    {
        if (this == &other)
            return *this;

        m_ViewportWidth = other.m_ViewportWidth;
        m_ViewportHeight = other.m_ViewportHeight;
        m_Filepath = other.m_Filepath;
        m_Name = other.m_Name;

        m_RootEntity = new Entity(m_Registry.create(), this);

        UnorderedMap<UUID, entt::entity> entityMap;

        auto idView = m_Registry.view<IDComponent>();
        for (auto e : idView)
        {
            const UUID& uuid = other.m_Registry.get<IDComponent>(e).Uuid;
            const String& name = other.m_Registry.get<TagComponent>(e).Tag;
            Entity newEntity = CreateEntityWithUuid(uuid, name);
            entityMap[uuid] = e;
        }

        CopyAllComponents(m_Registry, other.m_Registry, entityMap);
        return *this;
    }

    Scene::~Scene() { delete m_RootEntity; }

    Entity Scene::CreateEntity(const String& name)
    {
        Entity entity = { m_Registry.create(), this };
        entity.AddComponent<IDComponent>(UuidGenerator::Generate());
        entity.AddComponent<TagComponent>(name);
        entity.AddComponent<TransformComponent>();
        entity.AddComponent<RelationshipComponent>(*m_RootEntity);
        // m_HasChanged = true;

        return entity;
    }

    Entity Scene::CreateEntityWithUuid(const UUID& uuid, const String& name)
    {
        Entity entity(m_Registry.create(), this);

        entity.AddComponent<IDComponent>(uuid);
        entity.AddComponent<TagComponent>(name);
        entity.AddComponent<RelationshipComponent>();
        entity.AddComponent<TransformComponent>();
        // m_HasChanged = true;

        return entity;
    }

    Entity Scene::GetEntityFromUuid(const UUID& uuid)
    {
        Entity result;
        m_Registry.each([&](auto entityID) {
            Entity e = { entityID, this };
            if (e.GetUuid() == uuid)
                result = e;
        });

        CW_ENGINE_ERROR("Entity with uuid {0} not found.", uuid);
        return result;
    }

    Entity Scene::GetRootEntity() { return *m_RootEntity; }

    Entity Scene::FindEntityByName(const String& name)
    {
        Entity result{ entt::null, this };
        m_Registry.view<TagComponent>().each([&](const auto& entity, auto& tc) { result = { entity, this }; });
        return result;
    }

    void Scene::OnViewportResize(uint32_t width, uint32_t height)
    {
        m_ViewportWidth = width;
        m_ViewportHeight = height;
        m_Registry.view<CameraComponent>().each([&](auto& e, auto& cc) { cc.Camera.SetViewportSize(width, height); });
    }

} // namespace Crowny
