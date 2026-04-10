#include "cwpch.h"

#include "Crowny/Scene/Scene.h"

#include "Crowny/Ecs/Components.h"
#include "Crowny/Ecs/Entity.h"

#include "Crowny/Physics/Physics2D.h"
#include "Crowny/Audio/AudioManager.h"

#include "Crowny/Scripting/ScriptInfoManager.h"

#include <box2d/box2d.h>
#include <entt/entt.hpp>

namespace Crowny
{

    template <typename... Component>
    static void CopyComponent(entt::registry& dst, entt::registry& src, const UnorderedMap<UUID, entt::entity>& entityMap)
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

    template <typename... Component> static void CopyComponentIfExists(ComponentGroup<Component...>, Entity dst, Entity src)
    {
        CopyComponentIfExists<Component...>(dst, src);
    }

    static void CopyAllComponents(entt::registry& dstRegistry, entt::registry& srcRegistry, const UnorderedMap<UUID, entt::entity>& entityMap)
    {
        CopyComponent(AllComponents{}, dstRegistry, srcRegistry, entityMap);
    }

    static void CopyAllExistingComponents(Entity dst, Entity src) { CopyComponentIfExists(AllComponents{}, dst, src); }

    Scene::Scene(bool createRoot) : m_RootEntity(nullptr)
    {
        if (createRoot)
            CreateRootEntity();
    }

    Scene::Scene(const String& name, bool createRoot) : m_Name(name), m_RootEntity(nullptr)
    {
        if (createRoot)
            CreateRootEntity();
        RegisterEntityCallbacks();
    }

    Scene::Scene(Scene& other)
    {
        m_ViewportWidth = other.m_ViewportWidth;
        m_ViewportHeight = other.m_ViewportHeight;
        m_Filepath = other.m_Filepath;
        m_Name = other.m_Name;
        m_RootEntity = nullptr;

        UnorderedMap<UUID, entt::entity> copyEntityMap;

        auto idView = other.m_Registry.view<IDComponent>();
        for (auto e : idView)
        {
            const UUID& uuid = other.m_Registry.get<IDComponent>(e).Uuid;
            const String& name = other.m_Registry.get<TagComponent>(e).Tag;
            Entity newEntity = CreateEntityWithUuid(uuid, name);
            copyEntityMap[uuid] = newEntity.GetHandle();
        }

        CopyAllComponents(m_Registry, other.m_Registry, copyEntityMap);

        if (other.m_RootEntity)
            m_RootEntity = new Entity(m_EntityMap.at(other.m_RootEntity->GetUuid()), this);

        RegisterEntityCallbacks();
    }

    Scene& Scene::operator=(Scene& other)
    {
        if (this == &other)
            return *this;

        m_Registry.clear();
        m_EntityMap.clear();
        delete m_RootEntity;
        m_RootEntity = nullptr;

        m_ViewportWidth = other.m_ViewportWidth;
        m_ViewportHeight = other.m_ViewportHeight;
        m_Filepath = other.m_Filepath;
        m_Name = other.m_Name;

        UnorderedMap<UUID, entt::entity> copyEntityMap;

        auto idView = other.m_Registry.view<IDComponent>();
        for (auto e : idView)
        {
            const UUID& uuid = other.m_Registry.get<IDComponent>(e).Uuid;
            const String& name = other.m_Registry.get<TagComponent>(e).Tag;
            Entity newEntity = CreateEntityWithUuid(uuid, name);
            copyEntityMap[uuid] = newEntity.GetHandle();
        }

        CopyAllComponents(m_Registry, other.m_Registry, copyEntityMap);

        if (other.m_RootEntity)
            m_RootEntity = new Entity(m_EntityMap.at(other.m_RootEntity->GetUuid()), this);

        return *this;
    }

    Scene::~Scene()
    {
        if (m_RootEntity)
            m_RootEntity->Destroy(true);
        delete m_RootEntity;
    }

    void Scene::CreateRootEntity()
    {
        m_RootEntity = new Entity(m_Registry.create(), this);

        m_RootEntity->AddComponent<TransformComponent>();
        m_RootEntity->AddComponent<IDComponent>(UuidGenerator::Generate());
        m_RootEntity->AddComponent<TagComponent>(m_Name);
        m_RootEntity->AddComponent<RelationshipComponent>();
    }

    void Scene::RegisterEntityCallbacks()
    {
        m_Registry.on_construct<Rigidbody2DComponent>().connect<&Scene::OnRigidbody2DComponentConstruct>(this);
        m_Registry.on_destroy<Rigidbody2DComponent>().connect<&Scene::OnRigidbody2DComponentDestroy>(this);
        m_Registry.on_construct<BoxCollider2DComponent>().connect<&Scene::OnBoxCollider2DComponentConstruct>(this);
        m_Registry.on_destroy<BoxCollider2DComponent>().connect<&Scene::OnBoxCollider2DComponentDestroy>(this);
        m_Registry.on_construct<CircleCollider2DComponent>().connect<&Scene::OnCircleCollider2DComponentConstruct>(this);
        m_Registry.on_destroy<CircleCollider2DComponent>().connect<&Scene::OnCircleCollider2DComponentDestroy>(this);

        m_Registry.on_construct<AudioSourceComponent>().connect<&Scene::OnAudioSourceComponentConstruct>(this);
        m_Registry.on_destroy<AudioSourceComponent>().connect<&Scene::OnAudioSourceComponentDestroy>(this);

        m_Registry.on_destroy<TransformComponent>().connect<&Scene::OnTransformComponentDestroy>(this);
        m_Registry.on_destroy<MonoScriptComponent>().connect<&Scene::OnMonoScriptComponentDestroy>(this);
    }

    Entity Scene::DuplicateEntity(Entity entity, bool includeChildren)
    {
        Entity newEntity = CreateEntity(entity.GetName());
        CopyAllExistingComponents(newEntity, entity);

        if (includeChildren)
        {
            const auto& children = entity.GetChildren();
            for (auto child : children)
            {
                Entity e = DuplicateEntity(child, true);
                e.SetParent(newEntity);
            }
        }
        return newEntity;
    }

    Entity Scene::GetPrimaryCameraEntity() const
    {
        auto view = m_Registry.view<CameraComponent>();
        for (auto entity : view)
        {
            const auto& camera = view.get(entity);
            return Entity{ entity, const_cast<Scene*>(this) };
        }
        return {};
    }

    void Scene::OnRigidbody2DComponentConstruct(entt::registry& registry, entt::entity entity)
    {
        if (m_IsEditorScene || !Physics2D::IsStartedUp() || !Physics2D::Get().GetPhysicsWorld())
            return;
        Entity e = { entity, this };
        Physics2D::Get().CreateRigidbody(e);
    }

    void Scene::OnRigidbody2DComponentDestroy(entt::registry& registry, entt::entity entity)
    {
        if (m_IsEditorScene || !Physics2D::IsStartedUp() || !Physics2D::Get().GetPhysicsWorld())
            return;
        Entity e = { entity, this };
        Physics2D::Get().DestroyRigidbody(e);
    }

    void Scene::OnBoxCollider2DComponentConstruct(entt::registry& registry, entt::entity entity)
    {
        if (m_IsEditorScene || !Physics2D::IsStartedUp() || !Physics2D::Get().GetPhysicsWorld())
            return;
        Entity e = { entity, this };
        Physics2D::Get().CreateBoxCollider(e);
    }

    void Scene::OnBoxCollider2DComponentDestroy(entt::registry& registry, entt::entity entity)
    {
        if (m_IsEditorScene || !Physics2D::IsStartedUp() || !Physics2D::Get().GetPhysicsWorld())
            return;
        Entity e = { entity, this };
        Physics2D::Get().DestroyFixture(e, e.GetComponent<BoxCollider2DComponent>());
    }

    void Scene::OnCircleCollider2DComponentConstruct(entt::registry& registry, entt::entity entity)
    {
        if (m_IsEditorScene || !Physics2D::IsStartedUp() || !Physics2D::Get().GetPhysicsWorld())
            return;
        Entity e = { entity, this };
        Physics2D::Get().CreateCircleCollider(e);
    }

    void Scene::OnCircleCollider2DComponentDestroy(entt::registry& registry, entt::entity entity)
    {
        if (m_IsEditorScene || !Physics2D::IsStartedUp() || !Physics2D::Get().GetPhysicsWorld())
            return;
        Entity e = { entity, this };
        Physics2D::Get().DestroyFixture(e, e.GetComponent<CircleCollider2DComponent>());
    }

    void Scene::OnAudioSourceComponentConstruct(entt::registry& registry, entt::entity entity)
    {
        if (!AudioManager::IsStartedUp())
            return;
        Entity e = { entity, this };
        AudioSourceComponent& source = e.GetComponent<AudioSourceComponent>();
        if (source.GetPlayOnAwake())
            source.Play();
    }

    void Scene::OnAudioSourceComponentDestroy(entt::registry& registry, entt::entity entity)
    {
        if (!AudioManager::IsStartedUp())
            return;
        Entity e = { entity, this };
        AudioSourceComponent& source = e.GetComponent<AudioSourceComponent>();
        if (source.GetState() == AudioSourceState::Playing)
            source.Stop();
    }

    bool Scene::HasScriptComponent(Entity entity, const String& namespaceName, const String& typeName) const
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
        if (!monoClass)
            return;
        ::MonoClass* rawClass = monoClass->GetInternalPtr();
        MonoReflectionType* runtimeType = MonoUtils::GetType(rawClass);

        MonoScriptComponent* monoScriptComponent = nullptr;
        if (entity.HasComponent<MonoScriptComponent>())
            monoScriptComponent = &entity.GetComponent<MonoScriptComponent>();
        else
            monoScriptComponent = &entity.AddComponent<MonoScriptComponent>();

#ifdef CW_DEBUG
        for (const MonoScript& script : monoScriptComponent->Scripts)
        {
            if (script.GetNamespace() == namespaceName && script.GetTypeName() == typeName)
            {
                CW_ENGINE_ASSERT(false, "Entity already has that managed component");
                return;
            }
        }
#endif
        monoScriptComponent->Scripts.push_back(MonoScript(runtimeType));
        if (initialize)
        {
            monoScriptComponent->Scripts.back().Create(entity);
            MonoClass* runInEditor = ScriptInfoManager::Get().GetBuiltinClasses().RunInEditorAttribute;
            if (!m_IsEditorScene || monoClass->HasAttribute(runInEditor))
                monoScriptComponent->Scripts.back().OnStart();
        }
    }

    void Scene::RemoveScriptComponent(Entity entity, const String& namespaceName, const String& typeName)
    {
        if (!entity.HasComponent<MonoScriptComponent>())
            return;
        auto& scripts = entity.GetComponent<MonoScriptComponent>().Scripts;
        scripts.erase(std::remove_if(scripts.begin(), scripts.end(),
            [&](const MonoScript& s) { return s.GetNamespace() == namespaceName && s.GetTypeName() == typeName; }),
            scripts.end());
        if (scripts.empty())
            entity.RemoveComponent<MonoScriptComponent>();
    }

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
        m_Registry.view<AudioSourceComponent>().each([&](entt::entity entity, AudioSourceComponent& sc) { sc.OnInitialize(); });
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

    void Scene::OnRuntimeResume()
    {
        auto audioSourceView = m_Registry.view<AudioSourceComponent>();
        for (auto e : audioSourceView)
        {
            Entity entity = { e, this };
            auto& source = entity.GetComponent<AudioSourceComponent>();
            if (source.GetState() == AudioSourceState::Paused)
                source.Play();
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
        const UUID uuid = UuidGenerator::Generate();
        entity.AddComponent<IDComponent>(uuid);
        m_EntityMap[uuid] = entity.GetHandle();
        entity.AddComponent<TagComponent>(name);
        entity.AddComponent<TransformComponent>();
        entity.AddComponent<RelationshipComponent>();
        if (m_RootEntity)
            entity.SetParent(*m_RootEntity);

        return entity;
    }

    Entity Scene::CreateEntityWithUuid(const UUID& uuid, const String& name)
    {
        Entity entity(m_Registry.create(), this);

        entity.AddComponent<IDComponent>(uuid);
        m_EntityMap[uuid] = entity.GetHandle();
        entity.AddComponent<TagComponent>(name);
        entity.AddComponent<RelationshipComponent>();
        entity.AddComponent<TransformComponent>();
        if (m_RootEntity)
            entity.SetParent(*m_RootEntity);

        return entity;
    }

    void Scene::DestroyEntity(Entity entity)
    {
        m_EntityMap.erase(entity.GetUuid());
        entity.Destroy();
    }

    Entity Scene::GetEntityFromUuid(const UUID& uuid) const
    {
        if (m_EntityMap.find(uuid) != m_EntityMap.end())
            return { m_EntityMap.at(uuid), const_cast<Scene*>(this) };

        CW_ENGINE_ERROR("Entity with uuid {0} not found.", uuid);
        return {};
    }

    Entity Scene::GetRootEntity() const { return *m_RootEntity; }

    Entity Scene::FindEntityByName(const String& name) const
    {
        auto view = m_Registry.view<TagComponent>();
        for (auto entity : view)
        {
            auto [tag] = view.get(entity);
            if (tag.Tag == name)
                return Entity(entity, const_cast<Scene*>(this));
        }
        return Entity{};
    }

    void Scene::OnViewportResize(uint32_t width, uint32_t height)
    {
        m_ViewportWidth = width;
        m_ViewportHeight = height;
        m_Registry.view<CameraComponent>().each([&](CameraComponent& cameraComponent) { cameraComponent.Camera.SetViewportSize(width, height); });
    }

    void Scene::OnTransformComponentDestroy(entt::registry& registry, entt::entity entity)
    {
        if (!ScriptSceneObjectManager::IsStartedUp())
            return;
        Entity e = { entity, this };
        ScriptSceneObjectManager::Get().NotifyEntityDestroyed(e);
    }

    void Scene::OnMonoScriptComponentDestroy(entt::registry& registry, entt::entity entity)
    {
        if (!ScriptSceneObjectManager::IsStartedUp())
            return;
        Entity e = { entity, this };
        auto& msc = e.GetComponent<MonoScriptComponent>();
        for (auto& script : msc.Scripts)
            ScriptSceneObjectManager::Get().NotifyComponentDestroyed(script.InstanceId);
    }

} // namespace Crowny
