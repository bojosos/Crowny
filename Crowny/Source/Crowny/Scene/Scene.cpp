#include "cwpch.h"

#include "Crowny/Scene/Scene.h"

#include "Crowny/Ecs/Components.h"
#include "Crowny/Ecs/Entity.h"

#include "Crowny/Physics/Physics2D.h"

#include "Crowny/Scripting/ScriptInfoManager.h"

#include <box2d/box2d.h>
#include <entt/entt.hpp>

namespace Crowny
{

    template <typename... Component>
    static void CopyComponent(entt::registry& dst, entt::registry& src,
                              const UnorderedMap<UUID42, entt::entity>& entityMap)
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
                              const UnorderedMap<UUID42, entt::entity>& entityMap)
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
                                  const UnorderedMap<UUID42, entt::entity>& entityMap)
    {
        CopyComponent(AllComponents{}, dstRegistry, srcRegistry, entityMap);
    }

    static void CopyAllExistingComponents(Entity dst, Entity src) { CopyComponentIfExists(AllComponents{}, dst, src); }

    Scene::Scene() {}

    Scene::Scene(const String& name) : m_Name(name)
    {
        m_RootEntity = new Entity(m_Registry.create(), this);

        m_RootEntity->AddComponent<TransformComponent>();
        m_RootEntity->AddComponent<IDComponent>(UuidGenerator::Generate());
        m_RootEntity->AddComponent<TagComponent>(m_Name);
        m_RootEntity->AddComponent<RelationshipComponent>();

        RegisterEntityCallbacks();
    }

    Scene::Scene(Scene& other)
    {
        m_ViewportWidth = other.m_ViewportWidth;
        m_ViewportHeight = other.m_ViewportHeight;
        m_Filepath = other.m_Filepath;
        m_Name = other.m_Name;

        m_RootEntity = new Entity(m_Registry.create(), this);

        UnorderedMap<UUID42, entt::entity> entityMap;

        auto idView = m_Registry.view<IDComponent>();
        for (auto e : idView)
        {
            const UUID42& uuid = other.m_Registry.get<IDComponent>(e).Uuid;
            const String& name = other.m_Registry.get<TagComponent>(e).Tag;
            Entity newEntity = CreateEntityWithUuid(uuid, name);
            entityMap[uuid] = e;
        }

        CopyAllComponents(m_Registry, other.m_Registry, entityMap);

        RegisterEntityCallbacks();
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

        UnorderedMap<UUID42, entt::entity> entityMap;

        auto idView = m_Registry.view<IDComponent>();
        for (auto e : idView)
        {
            const UUID42& uuid = other.m_Registry.get<IDComponent>(e).Uuid;
            const String& name = other.m_Registry.get<TagComponent>(e).Tag;
            Entity newEntity = CreateEntityWithUuid(uuid, name);
            entityMap[uuid] = e;
        }

        CopyAllComponents(m_Registry, other.m_Registry, entityMap);
        return *this;
    }

    Scene::~Scene() { delete m_RootEntity; }

    void Scene::RegisterEntityCallbacks()
    {
        m_Registry.on_construct<Rigidbody2DComponent>().connect<&Scene::OnRigidbody2DComponentConstruct>(this);
        m_Registry.on_destroy<Rigidbody2DComponent>().connect<&Scene::OnRigidbody2DComponentDestroy>(this);
        m_Registry.on_construct<BoxCollider2DComponent>().connect<&Scene::OnBoxCollider2DComponentConstruct>(this);
        m_Registry.on_destroy<BoxCollider2DComponent>().connect<&Scene::OnBoxCollider2DComponentDestroy>(this);
        m_Registry.on_construct<CircleCollider2DComponent>().connect<&Scene::OnCircleCollider2DComponentConstruct>(
          this);
        m_Registry.on_destroy<CircleCollider2DComponent>().connect<&Scene::OnCircleCollider2DComponentDestroy>(this);

        m_Registry.on_construct<AudioSourceComponent>().connect<&Scene::OnAudioSourceComponentomponentConstruct>(this);
        m_Registry.on_destroy<AudioSourceComponent>().connect<&Scene::OnAudioSourceComponentComponentDestroy>(this);
    }

    Entity Scene::DuplicateEntity(Entity entity, bool includeChildren)
    {
        Entity newEntity = CreateEntity(entity.GetName());
        CopyAllExistingComponents(newEntity, entity);

        const auto& children = entity.GetChildren();
        for (auto child : children)
        {
            Entity e = DuplicateEntity(child);
            e.SetParent(newEntity);
        }
        return newEntity;
    }

    Entity Scene::GetPrimaryCameraEntity()
    {
        auto view = m_Registry.view<CameraComponent>();
        for (auto entity : view)
        {
            const auto& camera = view.get<CameraComponent>(entity);
            return Entity{ entity, this };
        }
        return {};
    }

    void Scene::OnRigidbody2DComponentConstruct(entt::registry& registry, entt::entity entity)
    {
        if (m_IsEditorScene)
            return;
        Entity e = { entity, this };
        Physics2D::Get().CreateRigidbody(e);
    }

    void Scene::OnRigidbody2DComponentDestroy(entt::registry& registry, entt::entity entity)
    {
        if (m_IsEditorScene)
            return;
        Entity e = { entity, this };
        Physics2D::Get().DestroyRigidbody(e);
    }

    void Scene::OnBoxCollider2DComponentConstruct(entt::registry& registry, entt::entity entity)
    {
        if (m_IsEditorScene)
            return;
        Entity e = { entity, this };
        Physics2D::Get().CreateBoxCollider(e);
    }

    void Scene::OnBoxCollider2DComponentDestroy(entt::registry& registry, entt::entity entity)
    {
        if (m_IsEditorScene)
            return;
        Entity e = { entity, this };
        Physics2D::Get().DestroyFixture(e, e.GetComponent<BoxCollider2DComponent>());
    }

    void Scene::OnCircleCollider2DComponentConstruct(entt::registry& registry, entt::entity entity)
    {
        if (m_IsEditorScene)
            return;
        Entity e = { entity, this };
        Physics2D::Get().CreateCircleCollider(e);
    }

    void Scene::OnCircleCollider2DComponentDestroy(entt::registry& registry, entt::entity entity)
    {
        if (m_IsEditorScene)
            return;
        Entity e = { entity, this };
        Physics2D::Get().DestroyFixture(e, e.GetComponent<CircleCollider2DComponent>());
    }

    void Scene::OnAudioSourceComponentomponentConstruct(entt::registry& registry, entt::entity entity)
    {
        if (m_IsEditorScene)
            return;
        Entity e = { entity, this };
        auto& source = e.GetComponent<AudioSourceComponent>();
        if (source.GetPlayOnAwake())
            source.Play();
    }

    void Scene::OnAudioSourceComponentComponentDestroy(entt::registry& registry, entt::entity entity)
    {
        Entity e = { entity, this };
        auto& source = e.GetComponent<AudioSourceComponent>();
        source.Stop();
    }

    bool Scene::HasScriptComponent(Entity entity, const String& namespaceName, const String& typeName)
    {
        if (entity.HasComponent<MonoScriptComponent>())
        {
            const MonoScriptComponent& monoScriptComponent = entity.GetComponent<MonoScriptComponent>();
            for (const MonoScript& script : monoScriptComponent.Scripts)
            {
                if (script.GetNamespace() == namespaceName && script.GetTypeName() == typeName)
                    return true;
            }
        }
        return false;
    }

    void Scene::AddScriptComponent(Entity entity, const String& namespaceName, const String& typeName, bool initialize)
    {
        MonoClass* monoClass = MonoManager::Get().FindClass(namespaceName, typeName);
        CW_ENGINE_ASSERT(monoClass != nullptr);
        ::MonoClass* rawClass = monoClass->GetInternalPtr();
        MonoReflectionType* runtimeType = MonoUtils::GetType(rawClass);
        if (entity.HasComponent<MonoScriptComponent>())
        {
            MonoScriptComponent& monoScriptComponent = entity.GetComponent<MonoScriptComponent>();

#ifdef CW_DEBUG
            for (const MonoScript& script : monoScriptComponent.Scripts)
            {
                if (script.GetNamespace() == namespaceName && script.GetTypeName() == typeName)
                {
                    CW_ENGINE_ASSERT(false, "Entity already has that managed component");
                    return;
                }
            }
#endif
            monoScriptComponent.Scripts.push_back(MonoScript(runtimeType));
            if (initialize)
            {
                monoScriptComponent.Scripts.back().Create(entity);
                MonoClass* runInEditor = ScriptInfoManager::Get().GetBuiltinClasses().RunInEditorAttribute;
                if (!m_IsEditorScene || monoClass->HasAttribute(runInEditor))
                    monoScriptComponent.Scripts.back().OnStart();
            }
        }
        else
        {
            MonoScriptComponent& monoScriptComponent = entity.AddComponent<MonoScriptComponent>();
#ifdef CW_DEBUG
            for (const MonoScript& script : monoScriptComponent.Scripts)
            {
                if (script.GetNamespace() == namespaceName && script.GetTypeName() == typeName)
                {
                    CW_ENGINE_ASSERT(false, "Entity already has that managed component");
                    return;
                }
            }
#endif
            monoScriptComponent.Scripts.push_back(MonoScript(runtimeType));
            if (initialize)
            {
                monoScriptComponent.Scripts.back().Create(entity);
                monoScriptComponent.Scripts.back().Create(entity);
                MonoClass* runInEditor = ScriptInfoManager::Get().GetBuiltinClasses().RunInEditorAttribute;
                if (!m_IsEditorScene || monoClass->HasAttribute(runInEditor))
                    monoScriptComponent.Scripts.back().OnStart();
            }
        }
    }

    void Scene::RemoveScriptComponent(Entity entity, const String& namespaceName, const String& typeName)
        {}

    void Scene::OnRuntimeStart()
    {
        Physics2D::Get().BeginSimulation(this);
        auto listenerView = m_Registry.view<AudioListenerComponent>();
        if (listenerView.size() == 0)
            CW_ENGINE_WARN("No audio listener in scene");
        else if (listenerView.size() > 1)
        {
            for (auto e : listenerView)
            {
                Entity entity = { e, this };
                entity.GetComponent<AudioListenerComponent>().Initialize();
                break; // Maybe not necessary
            }
        }
        m_Registry.view<AudioSourceComponent>().each(
          [&](entt::entity entity, AudioSourceComponent& sc) { sc.OnInitialize(); });
    }

    void Scene::OnSimulationStart() { Physics2D::Get().BeginSimulation(this); }

    void Scene::OnSimulationUpdate(Timestep ts) { Physics2D::Get().Step(ts, this); }

    void Scene::OnSimulationEnd() { Physics2D::Get().StopSimulation(this); }

    void Scene::OnRuntimePause()
    {
        auto audioSourceView = m_Registry.view<AudioSourceComponent>();
        for (auto e : audioSourceView)
        {
            Entity entity = { e, this };
            entity.GetComponent<AudioSourceComponent>().Pause();
        }
    }

    void Scene::OnRuntimeStop()
    {
        Physics2D::Get().StopSimulation(this);
        auto audioSourceView = m_Registry.view<AudioSourceComponent>();
        for (auto e : audioSourceView)
        {
            Entity entity = { e, this };
            entity.GetComponent<AudioSourceComponent>().Stop();
        }
    }

    void Scene::OnUpdateEditor(Timestep ts) {}

    void Scene::OnUpdateRuntime(Timestep ts) {}

    void Scene::OnFixedUpdate(Timestep ts) { Physics2D::Get().Step(ts, this); }

    Entity Scene::CreateEntity(const String& name)
    {
        Entity entity = { m_Registry.create(), this };
        entity.AddComponent<IDComponent>(UuidGenerator::Generate());
        entity.AddComponent<TagComponent>(name);
        entity.AddComponent<TransformComponent>();
        entity.AddComponent<RelationshipComponent>(*m_RootEntity);

        return entity;
    }

    Entity Scene::CreateEntityWithUuid(const UUID42& uuid, const String& name)
    {
        Entity entity(m_Registry.create(), this);

        entity.AddComponent<IDComponent>(uuid);
        entity.AddComponent<TagComponent>(name);
        entity.AddComponent<RelationshipComponent>();
        entity.AddComponent<TransformComponent>();

        return entity;
    }

    Entity Scene::GetEntityFromUuid(const UUID42& uuid)
    {
        Entity result;
        m_Registry.each([&](auto entityID) {
            Entity e = { entityID, this };
            if (e.GetUuid() == uuid)
                result = e;
        });

        if (!result)
            CW_ENGINE_ERROR("Entity with uuid {0} not found.", uuid);
        return result;
    }

    Entity Scene::GetRootEntity() { return *m_RootEntity; }

    Entity Scene::FindEntityByName(const String& name)
    {
        auto view = m_Registry.view<TagComponent>();
        for (auto entity : view)
        {
            if (view.get<TagComponent>(entity).Tag == name)
                return Entity(entity, this);
        }
        return Entity{};
    }

    void Scene::OnViewportResize(uint32_t width, uint32_t height)
    {
        m_ViewportWidth = width;
        m_ViewportHeight = height;
        m_Registry.view<CameraComponent>().each(
          [&](CameraComponent& cameraComponent) { cameraComponent.Camera.SetViewportSize(width, height); });
    }

} // namespace Crowny
