#include "cwpch.h"

#include "Crowny/Scene/Scene.h"
#include "Crowny/Scene/SceneManager.h"

#include "Crowny/Ecs/Components.h"
#include "Crowny/Ecs/Entity.h"

#include "Crowny/Audio/AudioManager.h"
#include "Crowny/Physics/Physics2D.h"
#include "Crowny/Physics/Physics3D.h"

#include "Crowny/Scripting/ScriptInfoManager.h"
#include "Crowny/Scripting/ScriptSceneObjectManager.h"

#include <entt/entt.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

namespace Crowny
{

    template <typename... Component>
    static void CopyComponent(entt::registry& dst, const entt::registry& src, const UnorderedMap<UUID, entt::entity>& entityMap)
    {
        (
          [&]() {
              if constexpr (std::is_same_v<Component, RelationshipComponent>)
                  return;

              const auto view = src.view<Component>();
              for (auto srcEntity : view)
              {
                  const entt::entity dstEntity = entityMap.at(src.get<IDComponent>(srcEntity).Uuid);
                  const auto& srcComponent = src.get<Component>(srcEntity);
                  dst.emplace_or_replace<Component>(dstEntity, srcComponent);
              }
          }(),
          ...);
    }

    template <typename... Component>
    static void CopyComponent(ComponentGroup<Component...>, entt::registry& dst, const entt::registry& src,
                              const UnorderedMap<UUID, entt::entity>& entityMap)
    {
        CopyComponent<Component...>(dst, src, entityMap);
    }

    template <typename... Component> static void CopyComponentIfExists(Entity dst, Entity src)
    {
        (
          [&]() {
              if constexpr (std::is_same_v<Component, RelationshipComponent>)
                  return;

              if (src.HasComponent<Component>())
                  dst.AddOrReplaceComponent<Component>(src.GetComponent<Component>());
          }(),
          ...);
    }

    template <typename... Component> static void CopyComponentIfExists(ComponentGroup<Component...>, Entity dst, Entity src)
    {
        CopyComponentIfExists<Component...>(dst, src);
    }

    static void CopyAllComponents(entt::registry& dstRegistry, const entt::registry& srcRegistry, const UnorderedMap<UUID, entt::entity>& entityMap)
    {
        CopyComponent(AllComponents{}, dstRegistry, srcRegistry, entityMap);
    }

    static void CopyAllExistingComponents(Entity dst, Entity src) { CopyComponentIfExists(AllComponents{}, dst, src); }

    Scene::Scene(bool createRoot) : m_RootEntity(nullptr)
    {
        RegisterEntityCallbacks();
        if (createRoot)
            CreateRootEntity();
    }

    Scene::Scene(const String& name, bool createRoot) : m_Name(name), m_RootEntity(nullptr)
    {
        RegisterEntityCallbacks();
        if (createRoot)
            CreateRootEntity();
    }

    Scene::Scene(const Scene& other)
    {
        m_ViewportWidth = other.m_ViewportWidth;
        m_ViewportHeight = other.m_ViewportHeight;
        m_Filepath = other.m_Filepath;
        m_Name = other.m_Name;
        m_ImGuiLayout = other.m_ImGuiLayout;
        m_Environment = other.m_Environment;
        m_IsEditorScene = other.m_IsEditorScene;
        m_RootEntity = nullptr;

        UnorderedMap<UUID, entt::entity> copyEntityMap;

        const auto idView = other.m_Registry.view<IDComponent>();
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
        RebuildCopiedRelationships(other, copyEntityMap);

        RegisterEntityCallbacks();
    }

    Scene& Scene::operator=(const Scene& other)
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
        m_ImGuiLayout = other.m_ImGuiLayout;
        m_Environment = other.m_Environment;
        m_IsEditorScene = other.m_IsEditorScene;

        UnorderedMap<UUID, entt::entity> copyEntityMap;

        const auto idView = other.m_Registry.view<IDComponent>();
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
        RebuildCopiedRelationships(other, copyEntityMap);

        return *this;
    }

    Scene::~Scene()
    {
        if (m_RuntimeActive)
            OnRuntimeStop();
        else if (m_SimulationActive)
            OnSimulationEnd();
        else
        {
            EndPhysics3D();
            if (m_Physics2DActive && Physics2D::TryGet() != nullptr)
                Physics2D::TryGet()->StopSimulation(this);
            m_Physics2DActive = false;
        }
        if (m_RootEntity)
        {
            Entity root = *m_RootEntity;
            delete m_RootEntity;
            m_RootEntity = nullptr;
            root.Destroy(true);
        }
    }

    void Scene::CreateRootEntity()
    {
        if (m_RootEntity)
            return;
        m_RootEntity = new Entity(m_Registry.create(), this);

        m_RootEntity->AddComponent<TransformComponent>();
        const UUID uuid = UuidGenerator::Generate();
        m_RootEntity->AddComponent<IDComponent>(uuid);
        m_EntityMap[uuid] = m_RootEntity->GetHandle();
        m_RootEntity->AddComponent<TagComponent>(m_Name);
        m_RootEntity->AddComponent<RelationshipComponent>();
    }

    void Scene::RegisterEntityCallbacks()
    {
        m_Registry.on_construct<Rigidbody2DComponent>().connect<&Scene::OnRigidbody2DComponentConstruct>(this);
        m_Registry.on_update<Rigidbody2DComponent>().connect<&Scene::OnRigidbody2DComponentUpdate>(this);
        m_Registry.on_destroy<Rigidbody2DComponent>().connect<&Scene::OnRigidbody2DComponentDestroy>(this);
        m_Registry.on_construct<BoxCollider2DComponent>().connect<&Scene::OnBoxCollider2DComponentConstruct>(this);
        m_Registry.on_update<BoxCollider2DComponent>().connect<&Scene::OnBoxCollider2DComponentUpdate>(this);
        m_Registry.on_destroy<BoxCollider2DComponent>().connect<&Scene::OnBoxCollider2DComponentDestroy>(this);
        m_Registry.on_construct<CircleCollider2DComponent>().connect<&Scene::OnCircleCollider2DComponentConstruct>(this);
        m_Registry.on_update<CircleCollider2DComponent>().connect<&Scene::OnCircleCollider2DComponentUpdate>(this);
        m_Registry.on_destroy<CircleCollider2DComponent>().connect<&Scene::OnCircleCollider2DComponentDestroy>(this);

        m_Registry.on_construct<Rigidbody3DComponent>().connect<&Scene::OnRigidbody3DComponentConstruct>(this);
        m_Registry.on_update<Rigidbody3DComponent>().connect<&Scene::OnRigidbody3DComponentUpdate>(this);
        m_Registry.on_destroy<Rigidbody3DComponent>().connect<&Scene::OnRigidbody3DComponentDestroy>(this);
        m_Registry.on_construct<BoxCollider3DComponent>().connect<&Scene::OnBoxCollider3DComponentConstruct>(this);
        m_Registry.on_update<BoxCollider3DComponent>().connect<&Scene::OnBoxCollider3DComponentUpdate>(this);
        m_Registry.on_destroy<BoxCollider3DComponent>().connect<&Scene::OnBoxCollider3DComponentDestroy>(this);
        m_Registry.on_construct<SphereCollider3DComponent>().connect<&Scene::OnSphereCollider3DComponentConstruct>(this);
        m_Registry.on_update<SphereCollider3DComponent>().connect<&Scene::OnSphereCollider3DComponentUpdate>(this);
        m_Registry.on_destroy<SphereCollider3DComponent>().connect<&Scene::OnSphereCollider3DComponentDestroy>(this);
        m_Registry.on_construct<CapsuleCollider3DComponent>().connect<&Scene::OnCapsuleCollider3DComponentConstruct>(this);
        m_Registry.on_update<CapsuleCollider3DComponent>().connect<&Scene::OnCapsuleCollider3DComponentUpdate>(this);
        m_Registry.on_destroy<CapsuleCollider3DComponent>().connect<&Scene::OnCapsuleCollider3DComponentDestroy>(this);

        m_Registry.on_construct<AudioSourceComponent>().connect<&Scene::OnAudioSourceComponentConstruct>(this);
        m_Registry.on_update<AudioSourceComponent>().connect<&Scene::OnAudioSourceComponentUpdate>(this);
        m_Registry.on_destroy<AudioSourceComponent>().connect<&Scene::OnAudioSourceComponentDestroy>(this);

        m_Registry.on_destroy<TransformComponent>().connect<&Scene::OnTransformComponentDestroy>(this);
        m_Registry.on_destroy<MonoScriptComponent>().connect<&Scene::OnMonoScriptComponentDestroy>(this);
    }

    void Scene::RebuildCopiedRelationships(const Scene& source, const UnorderedMap<UUID, entt::entity>& entityMap)
    {
        const auto relationshipView = source.m_Registry.view<IDComponent, RelationshipComponent>();
        for (const entt::entity sourceHandle : relationshipView)
        {
            const IDComponent& sourceId = source.m_Registry.get<IDComponent>(sourceHandle);
            const RelationshipComponent& sourceRelationship = source.m_Registry.get<RelationshipComponent>(sourceHandle);
            const auto destination = entityMap.find(sourceId.Uuid);
            if (destination == entityMap.end())
                continue;

            auto& destinationRelationship = m_Registry.get<RelationshipComponent>(destination->second);
            destinationRelationship.Parent = {};
            destinationRelationship.Children.clear();
            destinationRelationship.SiblingIndex = 0;
            destinationRelationship.Children.reserve(sourceRelationship.Children.size());
        }

        for (const entt::entity sourceHandle : relationshipView)
        {
            const IDComponent& sourceId = source.m_Registry.get<IDComponent>(sourceHandle);
            const RelationshipComponent& sourceRelationship = source.m_Registry.get<RelationshipComponent>(sourceHandle);
            const auto destination = entityMap.find(sourceId.Uuid);
            if (destination == entityMap.end())
                continue;

            Entity destinationParent{ destination->second, this };
            auto& destinationRelationship = m_Registry.get<RelationshipComponent>(destination->second);

            for (const Entity sourceChild : sourceRelationship.Children)
            {
                if (!sourceChild || sourceChild.GetScene() != &source)
                    continue;
                const auto child = entityMap.find(sourceChild.GetUuid());
                if (child == entityMap.end())
                    continue;
                Entity destinationChild{ child->second, this };
                auto& childRelationship = m_Registry.get<RelationshipComponent>(child->second);
                childRelationship.Parent = destinationParent;
                childRelationship.SiblingIndex = static_cast<uint32_t>(destinationRelationship.Children.size());
                destinationRelationship.Children.push_back(destinationChild);
            }
        }
    }

    Entity Scene::DuplicateEntity(Entity entity, bool includeChildren)
    {
        if (!entity || entity.GetScene() != this || (m_RootEntity && entity == *m_RootEntity))
            return {};

        const Entity sourceParent = entity.GetParent();
        const uint32_t sourceSiblingIndex = entity.GetSiblingIndex();
        Entity newEntity = DuplicateEntityInternal(entity, includeChildren, sourceParent);
        if (newEntity && sourceParent)
            newEntity.SetSiblingIndex(sourceSiblingIndex + 1);
        return newEntity;
    }

    Entity Scene::DuplicateEntityInternal(Entity entity, bool includeChildren, Entity cloneParent)
    {
        Entity newEntity = CreateEntityInternal(UuidGenerator::Generate(), entity.GetName(), cloneParent);
        CopyAllExistingComponents(newEntity, entity);
        newEntity.NotifyTransformChanged();

        if (includeChildren)
        {
            for (Entity child : entity.GetChildren())
                DuplicateEntityInternal(child, true, newEntity);
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
        if (!m_Physics2DActive || !Physics2D::IsStartedUp() || !Physics2D::TryGet()->IsSimulating())
            return;
        Entity e = { entity, this };
        Physics2D::TryGet()->CreateRigidbody(e);
    }

    void Scene::OnRigidbody2DComponentUpdate(entt::registry& registry, entt::entity entity)
    {
        if (!m_Physics2DActive || !Physics2D::IsStartedUp() || !Physics2D::TryGet()->IsSimulating())
            return;

        Entity e = { entity, this };
        auto& rigidbody = registry.get<Rigidbody2DComponent>(entity);
        if (rigidbody.RuntimeBody == nullptr)
            return;

        Physics2D& physics = *Physics2D::TryGet();
        const glm::vec2 linearVelocity = physics.GetLinearVelocity(e);
        const float angularVelocity = physics.GetAngularVelocity(e);
        const bool awake = physics.IsBodyAwake(e);
        physics.DestroyRigidbody(e);
        physics.CreateRigidbody(e);
        if (rigidbody.RuntimeBody != nullptr)
        {
            physics.SetLinearVelocity(e, linearVelocity);
            physics.SetAngularVelocity(e, angularVelocity);
            physics.SetBodyAwake(e, awake);
        }
    }

    void Scene::OnRigidbody2DComponentDestroy(entt::registry& registry, entt::entity entity)
    {
        if (!m_Physics2DActive || !Physics2D::IsStartedUp() || !Physics2D::TryGet()->IsSimulating())
            return;
        Entity e = { entity, this };
        Physics2D::TryGet()->DestroyRigidbody(e);
    }

    void Scene::OnBoxCollider2DComponentConstruct(entt::registry& registry, entt::entity entity)
    {
        if (!m_Physics2DActive || !Physics2D::IsStartedUp() || !Physics2D::TryGet()->IsSimulating())
            return;
        Entity e = { entity, this };
        Physics2D::TryGet()->CreateBoxCollider(e);
    }

    void Scene::OnBoxCollider2DComponentUpdate(entt::registry& registry, entt::entity entity)
    {
        if (!m_Physics2DActive || !Physics2D::IsStartedUp() || !Physics2D::TryGet()->IsSimulating())
            return;

        Entity e = { entity, this };
        auto& collider = registry.get<BoxCollider2DComponent>(entity);
        if (collider.RuntimeFixture == nullptr)
            return;
        Physics2D::TryGet()->DestroyFixture(e, collider);
        Physics2D::TryGet()->CreateBoxCollider(e);
    }

    void Scene::OnBoxCollider2DComponentDestroy(entt::registry& registry, entt::entity entity)
    {
        if (!m_Physics2DActive || !Physics2D::IsStartedUp() || !Physics2D::TryGet()->IsSimulating())
            return;
        Entity e = { entity, this };
        Physics2D::TryGet()->DestroyFixture(e, e.GetComponent<BoxCollider2DComponent>());
    }

    void Scene::OnCircleCollider2DComponentConstruct(entt::registry& registry, entt::entity entity)
    {
        if (!m_Physics2DActive || !Physics2D::IsStartedUp() || !Physics2D::TryGet()->IsSimulating())
            return;
        Entity e = { entity, this };
        Physics2D::TryGet()->CreateCircleCollider(e);
    }

    void Scene::OnCircleCollider2DComponentUpdate(entt::registry& registry, entt::entity entity)
    {
        if (!m_Physics2DActive || !Physics2D::IsStartedUp() || !Physics2D::TryGet()->IsSimulating())
            return;

        Entity e = { entity, this };
        auto& collider = registry.get<CircleCollider2DComponent>(entity);
        if (collider.RuntimeFixture == nullptr)
            return;
        Physics2D::TryGet()->DestroyFixture(e, collider);
        Physics2D::TryGet()->CreateCircleCollider(e);
    }

    void Scene::OnCircleCollider2DComponentDestroy(entt::registry& registry, entt::entity entity)
    {
        if (!m_Physics2DActive || !Physics2D::IsStartedUp() || !Physics2D::TryGet()->IsSimulating())
            return;
        Entity e = { entity, this };
        Physics2D::TryGet()->DestroyFixture(e, e.GetComponent<CircleCollider2DComponent>());
    }

    static bool HasPhysics3DComponents(Entity entity)
    {
        return entity.HasComponent<Rigidbody3DComponent>() || entity.HasComponent<BoxCollider3DComponent>() ||
               entity.HasComponent<SphereCollider3DComponent>() || entity.HasComponent<CapsuleCollider3DComponent>();
    }

    bool Scene::BeginPhysics3D()
    {
        EndPhysics3D();
        if (!Physics3D::IsStartedUp())
            return false;

        m_PendingPhysics3DRebuilds.clear();
        m_PendingPhysics3DContacts.clear();
        m_DispatchPhysics3DContacts.clear();
        m_Physics3DActive = Physics3D::Get().StartSimulation([this](const PhysicsContactEvent3D& event) {
            if (m_Physics3DActive)
                m_PendingPhysics3DContacts.push_back(event);
        });
        if (!m_Physics3DActive)
            return false;

        m_Registry.each([&](entt::entity handle) {
            Entity entity{ handle, this };
            if (HasPhysics3DComponents(entity))
                CreatePhysics3DBody(entity);
        });
        return true;
    }

    void Scene::EndPhysics3D()
    {
        if (!m_Physics3DActive)
            return;

        for (auto& [handle, body] : m_Physics3DBodies)
        {
            if (!m_Registry.valid(handle))
                continue;
            Entity entity{ handle, this };
            if (entity.HasComponent<Rigidbody3DComponent>())
                entity.GetComponent<Rigidbody3DComponent>().RuntimeBody = {};
            if (entity.HasComponent<BoxCollider3DComponent>())
                entity.GetComponent<BoxCollider3DComponent>().RuntimeShape = {};
            if (entity.HasComponent<SphereCollider3DComponent>())
                entity.GetComponent<SphereCollider3DComponent>().RuntimeShape = {};
            if (entity.HasComponent<CapsuleCollider3DComponent>())
                entity.GetComponent<CapsuleCollider3DComponent>().RuntimeShape = {};
        }
        m_Physics3DBodies.clear();
        m_Physics3DEntities.clear();
        m_Physics3DScales.clear();
        m_PendingPhysics3DRebuilds.clear();
        m_PendingPhysics3DContacts.clear();
        m_DispatchPhysics3DContacts.clear();
        m_Physics3DActive = false;
        if (Physics3D::IsStartedUp())
            Physics3D::Get().StopSimulation();
    }

    PhysicsBody3DHandle Scene::CreatePhysics3DBody(Entity entity)
    {
        if (!m_Physics3DActive || !entity || !HasPhysics3DComponents(entity))
            return {};
        const auto existing = m_Physics3DBodies.find(entity.GetHandle());
        if (existing != m_Physics3DBodies.end())
            return existing->second;

        const Transform& world = entity.GetWorldTransform();
        PhysicsBody3DDesc desc;
        desc.Position = world.GetPosition();
        desc.Rotation = world.GetRotation();
        desc.UserData = static_cast<uint32_t>(entity.GetHandle());

        if (entity.HasComponent<Rigidbody3DComponent>())
        {
            const auto& rigidbody = entity.GetComponent<Rigidbody3DComponent>();
            desc.Type = rigidbody.GetBodyType();
            desc.Mass = rigidbody.GetMass();
            desc.AutoMass = rigidbody.GetAutoMass();
            desc.GravityScale = rigidbody.GetGravityScale();
            desc.LinearDamping = rigidbody.GetLinearDamping();
            desc.AngularDamping = rigidbody.GetAngularDamping();
            desc.CenterOfMass = rigidbody.GetCenterOfMass();
            desc.AllowSleep = rigidbody.GetAllowSleep();
            desc.StartAwake = rigidbody.GetStartAwake();
            desc.Continuous = rigidbody.GetContinuousCollision();
            desc.LockRotationX = rigidbody.GetLockRotationX();
            desc.LockRotationY = rigidbody.GetLockRotationY();
            desc.LockRotationZ = rigidbody.GetLockRotationZ();
            desc.Filter = rigidbody.GetFilter();
            desc.LinearVelocity = rigidbody.GetLinearVelocity();
            desc.AngularVelocity = rigidbody.GetAngularVelocity();
        }

        const PhysicsBody3DHandle body = Physics3D::Get().CreateBody(desc);
        if (!body)
        {
            CW_ENGINE_ERROR("Failed to create a 3D physics body for entity {0}", entity.GetName());
            return {};
        }

        m_Physics3DBodies[entity.GetHandle()] = body;
        m_Physics3DEntities[body] = entity.GetHandle();
        m_Physics3DScales[entity.GetHandle()] = world.GetScale();
        if (entity.HasComponent<Rigidbody3DComponent>())
            entity.GetComponent<Rigidbody3DComponent>().RuntimeBody = body;
        CreatePhysics3DShapes(entity, body);
        return body;
    }

    void Scene::CreatePhysics3DShapes(Entity entity, PhysicsBody3DHandle body)
    {
        if (!m_Physics3DActive || !entity || !body)
            return;

        const glm::vec3 worldScale = entity.GetWorldScale();
        const glm::vec3 absoluteScale = glm::max(glm::abs(worldScale), glm::vec3(0.001f));
        const auto makeDesc = [&](const Collider3D& collider, PhysicsShapeType3D type) {
            PhysicsShape3DDesc desc;
            desc.Type = type;
            desc.LocalPosition = collider.GetOffset() * worldScale;
            desc.LocalRotation = collider.GetRotation();
            desc.IsTrigger = collider.IsTrigger();
            desc.Material = collider.GetMaterialData();
            desc.Filter = collider.GetFilter();
            desc.UserData = static_cast<uint32_t>(entity.GetHandle());
            return desc;
        };

        if (entity.HasComponent<BoxCollider3DComponent>())
        {
            auto& collider = entity.GetComponent<BoxCollider3DComponent>();
            PhysicsShape3DDesc desc = makeDesc(collider, PhysicsShapeType3D::Box);
            desc.HalfExtents = glm::max(glm::abs(collider.GetSize()) * absoluteScale * 0.5f, glm::vec3(0.0005f));
            collider.RuntimeShape = Physics3D::Get().AddShape(body, desc);
        }
        if (entity.HasComponent<SphereCollider3DComponent>())
        {
            auto& collider = entity.GetComponent<SphereCollider3DComponent>();
            PhysicsShape3DDesc desc = makeDesc(collider, PhysicsShapeType3D::Sphere);
            desc.Radius = collider.GetRadius() * std::max({ absoluteScale.x, absoluteScale.y, absoluteScale.z });
            collider.RuntimeShape = Physics3D::Get().AddShape(body, desc);
        }
        if (entity.HasComponent<CapsuleCollider3DComponent>())
        {
            auto& collider = entity.GetComponent<CapsuleCollider3DComponent>();
            PhysicsShape3DDesc desc = makeDesc(collider, PhysicsShapeType3D::Capsule);
            desc.Radius = collider.GetRadius() * std::max(absoluteScale.x, absoluteScale.z);
            desc.Height = std::max(collider.GetHeight() * absoluteScale.y, desc.Radius * 2.0f);
            collider.RuntimeShape = Physics3D::Get().AddShape(body, desc);
        }
    }

    void Scene::DestroyPhysics3DShapes(Entity entity, PhysicsBody3DHandle body)
    {
        if (!entity || !body || !Physics3D::IsStartedUp())
            return;
        const auto removeShape = [&](Collider3D& collider) {
            if (collider.RuntimeShape)
                Physics3D::Get().RemoveShape(body, collider.RuntimeShape);
            collider.RuntimeShape = {};
        };
        if (entity.HasComponent<BoxCollider3DComponent>())
            removeShape(entity.GetComponent<BoxCollider3DComponent>());
        if (entity.HasComponent<SphereCollider3DComponent>())
            removeShape(entity.GetComponent<SphereCollider3DComponent>());
        if (entity.HasComponent<CapsuleCollider3DComponent>())
            removeShape(entity.GetComponent<CapsuleCollider3DComponent>());
    }

    void Scene::DestroyPhysics3DBody(entt::entity handle)
    {
        const auto found = m_Physics3DBodies.find(handle);
        if (found == m_Physics3DBodies.end())
            return;
        const PhysicsBody3DHandle body = found->second;
        if (m_Registry.valid(handle))
        {
            Entity entity{ handle, this };
            if (entity.HasComponent<Rigidbody3DComponent>())
                entity.GetComponent<Rigidbody3DComponent>().RuntimeBody = {};
            if (entity.HasComponent<BoxCollider3DComponent>())
                entity.GetComponent<BoxCollider3DComponent>().RuntimeShape = {};
            if (entity.HasComponent<SphereCollider3DComponent>())
                entity.GetComponent<SphereCollider3DComponent>().RuntimeShape = {};
            if (entity.HasComponent<CapsuleCollider3DComponent>())
                entity.GetComponent<CapsuleCollider3DComponent>().RuntimeShape = {};
        }
        m_Physics3DEntities.erase(body);
        m_Physics3DScales.erase(handle);
        m_Physics3DBodies.erase(found);
        if (Physics3D::IsStartedUp())
            Physics3D::Get().DestroyBody(body);
    }

    void Scene::QueuePhysics3DRebuild(entt::entity handle)
    {
        if (std::find(m_PendingPhysics3DRebuilds.begin(), m_PendingPhysics3DRebuilds.end(), handle) == m_PendingPhysics3DRebuilds.end())
            m_PendingPhysics3DRebuilds.push_back(handle);
    }

    void Scene::RecreatePhysics3DBody(Entity entity)
    {
        if (!m_Physics3DActive || !entity)
            return;
        glm::vec3 linearVelocity{ 0.0f };
        glm::vec3 angularVelocity{ 0.0f };
        bool awake = false;
        bool preserveAwake = false;
        const bool hasRigidbody = entity.HasComponent<Rigidbody3DComponent>();
        if (hasRigidbody)
        {
            auto& rigidbody = entity.GetComponent<Rigidbody3DComponent>();
            linearVelocity = rigidbody.GetLinearVelocity();
            angularVelocity = rigidbody.GetAngularVelocity();
            preserveAwake = static_cast<bool>(rigidbody.RuntimeBody);
            if (preserveAwake)
                awake = rigidbody.IsAwake();
        }
        DestroyPhysics3DBody(entity.GetHandle());
        CreatePhysics3DBody(entity);
        if (hasRigidbody && entity.HasComponent<Rigidbody3DComponent>())
        {
            auto& rigidbody = entity.GetComponent<Rigidbody3DComponent>();
            rigidbody.SetLinearVelocity(linearVelocity);
            rigidbody.SetAngularVelocity(angularVelocity);
            if (preserveAwake)
                rigidbody.SetAwake(awake);
        }
    }

    void Scene::RecreatePhysics3DShapes(Entity entity)
    {
        if (!m_Physics3DActive || !entity)
            return;
        const auto found = m_Physics3DBodies.find(entity.GetHandle());
        if (found == m_Physics3DBodies.end())
        {
            CreatePhysics3DBody(entity);
            return;
        }
        const PhysicsBody3DHandle body = found->second;
        if (!body)
            return;
        DestroyPhysics3DShapes(entity, body);
        CreatePhysics3DShapes(entity, body);
    }

    void Scene::UpdatePhysics3DTransform(Entity entity)
    {
        if (!m_Physics3DActive || !entity)
            return;
        const auto found = m_Physics3DBodies.find(entity.GetHandle());
        if (found == m_Physics3DBodies.end())
            return;
        const Transform& world = entity.GetWorldTransform();
        Physics3D::Get().SetBodyTransform(found->second, world.GetPosition(), world.GetRotation(), true);
        const auto scale = m_Physics3DScales.find(entity.GetHandle());
        if (scale == m_Physics3DScales.end() || glm::any(glm::greaterThan(glm::abs(scale->second - world.GetScale()), glm::vec3(0.0001f))))
        {
            m_Physics3DScales[entity.GetHandle()] = world.GetScale();
            RecreatePhysics3DShapes(entity);
        }
    }

    void Scene::OnRigidbody3DComponentConstruct(entt::registry&, entt::entity entity)
    {
        if (m_Physics3DActive)
            RecreatePhysics3DBody({ entity, this });
    }

    void Scene::OnRigidbody3DComponentUpdate(entt::registry&, entt::entity entity)
    {
        if (m_Physics3DActive)
            RecreatePhysics3DBody({ entity, this });
    }

    void Scene::OnRigidbody3DComponentDestroy(entt::registry& registry, entt::entity entity)
    {
        if (m_Physics3DActive)
        {
            DestroyPhysics3DBody(entity);
            if (registry.any_of<BoxCollider3DComponent, SphereCollider3DComponent, CapsuleCollider3DComponent>(entity))
                QueuePhysics3DRebuild(entity);
        }
    }

    void Scene::OnBoxCollider3DComponentConstruct(entt::registry&, entt::entity entity)
    {
        if (m_Physics3DActive)
            RecreatePhysics3DShapes({ entity, this });
    }

    void Scene::OnBoxCollider3DComponentUpdate(entt::registry&, entt::entity entity)
    {
        if (m_Physics3DActive)
            RecreatePhysics3DShapes({ entity, this });
    }

    void Scene::OnBoxCollider3DComponentDestroy(entt::registry& registry, entt::entity entity)
    {
        const auto body = m_Physics3DBodies.find(entity);
        auto& collider = registry.get<BoxCollider3DComponent>(entity);
        if (m_Physics3DActive && body != m_Physics3DBodies.end() && collider.RuntimeShape)
            Physics3D::Get().RemoveShape(body->second, collider.RuntimeShape);
        collider.RuntimeShape = {};
        if (m_Physics3DActive && !registry.any_of<Rigidbody3DComponent, SphereCollider3DComponent, CapsuleCollider3DComponent>(entity))
            DestroyPhysics3DBody(entity);
    }

    void Scene::OnSphereCollider3DComponentConstruct(entt::registry&, entt::entity entity)
    {
        if (m_Physics3DActive)
            RecreatePhysics3DShapes({ entity, this });
    }

    void Scene::OnSphereCollider3DComponentUpdate(entt::registry&, entt::entity entity)
    {
        if (m_Physics3DActive)
            RecreatePhysics3DShapes({ entity, this });
    }

    void Scene::OnSphereCollider3DComponentDestroy(entt::registry& registry, entt::entity entity)
    {
        const auto body = m_Physics3DBodies.find(entity);
        auto& collider = registry.get<SphereCollider3DComponent>(entity);
        if (m_Physics3DActive && body != m_Physics3DBodies.end() && collider.RuntimeShape)
            Physics3D::Get().RemoveShape(body->second, collider.RuntimeShape);
        collider.RuntimeShape = {};
        if (m_Physics3DActive && !registry.any_of<Rigidbody3DComponent, BoxCollider3DComponent, CapsuleCollider3DComponent>(entity))
            DestroyPhysics3DBody(entity);
    }

    void Scene::OnCapsuleCollider3DComponentConstruct(entt::registry&, entt::entity entity)
    {
        if (m_Physics3DActive)
            RecreatePhysics3DShapes({ entity, this });
    }

    void Scene::OnCapsuleCollider3DComponentUpdate(entt::registry&, entt::entity entity)
    {
        if (m_Physics3DActive)
            RecreatePhysics3DShapes({ entity, this });
    }

    void Scene::OnCapsuleCollider3DComponentDestroy(entt::registry& registry, entt::entity entity)
    {
        const auto body = m_Physics3DBodies.find(entity);
        auto& collider = registry.get<CapsuleCollider3DComponent>(entity);
        if (m_Physics3DActive && body != m_Physics3DBodies.end() && collider.RuntimeShape)
            Physics3D::Get().RemoveShape(body->second, collider.RuntimeShape);
        collider.RuntimeShape = {};
        if (m_Physics3DActive && !registry.any_of<Rigidbody3DComponent, BoxCollider3DComponent, SphereCollider3DComponent>(entity))
            DestroyPhysics3DBody(entity);
    }

    void Scene::StepPhysics3D(Timestep ts)
    {
        if (!m_Physics3DActive || !Physics3D::IsStartedUp())
            return;

        for (const entt::entity handle : m_PendingPhysics3DRebuilds)
        {
            if (!m_Registry.valid(handle))
                continue;
            Entity entity{ handle, this };
            if (HasPhysics3DComponents(entity) && m_Physics3DBodies.find(handle) == m_Physics3DBodies.end())
                CreatePhysics3DBody(entity);
        }
        m_PendingPhysics3DRebuilds.clear();

        const float timestep = std::max(ts.GetSeconds(), 0.0f);
        auto rigidbodyView = m_Registry.view<Rigidbody3DComponent>();
        rigidbodyView.each([&](entt::entity handle, Rigidbody3DComponent& rigidbody) {
            const PhysicsBody3DHandle body = rigidbody.RuntimeBody;
            if (!body || rigidbody.GetBodyType() != PhysicsBodyType3D::Kinematic)
                return;
            Entity entity{ handle, this };
            const Transform& world = entity.GetWorldTransform();
            if (timestep > 0.0f)
                Physics3D::Get().MoveKinematic(body, world.GetPosition(), world.GetRotation(), timestep);
            else
                Physics3D::Get().SetBodyTransform(body, world.GetPosition(), world.GetRotation(), true);
        });

        Physics3D::Get().Step(ts);

        rigidbodyView.each([&](entt::entity handle, Rigidbody3DComponent& rigidbody) {
            const PhysicsBody3DHandle body = rigidbody.RuntimeBody;
            if (!body || rigidbody.GetBodyType() != PhysicsBodyType3D::Dynamic)
                return;

            Entity entity{ handle, this };
            glm::vec3 position;
            glm::quat rotation;
            Physics3D::Get().GetBodyTransform(body, position, rotation);
            const glm::mat4 worldMatrix =
              glm::translate(glm::mat4(1.0f), position) * glm::mat4_cast(rotation) * glm::scale(glm::mat4(1.0f), entity.GetWorldScale());
            entity.SetWorldTransform(worldMatrix, false);
        });

        m_DispatchPhysics3DContacts.clear();
        m_DispatchPhysics3DContacts.swap(m_PendingPhysics3DContacts);
        for (const PhysicsContactEvent3D& event : m_DispatchPhysics3DContacts)
            HandlePhysics3DContact(event);
        m_DispatchPhysics3DContacts.clear();
    }

    void Scene::HandlePhysics3DContact(const PhysicsContactEvent3D& event)
    {
        const auto firstFound = m_Physics3DEntities.find(event.BodyA);
        const auto secondFound = m_Physics3DEntities.find(event.BodyB);
        if (firstFound == m_Physics3DEntities.end() || secondFound == m_Physics3DEntities.end())
            return;
        const entt::entity firstHandle = firstFound->second;
        const entt::entity secondHandle = secondFound->second;
        if (!m_Registry.valid(firstHandle) || !m_Registry.valid(secondHandle))
            return;

        const auto dispatch = [&](entt::entity selfHandle, entt::entity otherHandle, bool reverseNormal) {
            if (!m_Registry.valid(selfHandle) || !m_Registry.valid(otherHandle))
                return;
            Entity self{ selfHandle, this };
            Entity other{ otherHandle, this };
            if (!self.HasComponent<MonoScriptComponent>())
                return;

            auto& scripts = self.GetComponent<MonoScriptComponent>().Scripts;
            if (event.IsTrigger)
            {
                for (auto& script : scripts)
                {
                    switch (event.Type)
                    {
                    case PhysicsContactEventType3D::Enter:
                        script.OnTriggerEnter3D(other);
                        break;
                    case PhysicsContactEventType3D::Stay:
                        script.OnTriggerStay3D(other);
                        break;
                    case PhysicsContactEventType3D::Exit:
                        script.OnTriggerExit3D(other);
                        break;
                    }
                }
                return;
            }

            Collision3D collision;
            collision.Colliders = { self, other };
            collision.Points = event.Points;
            if (reverseNormal)
                for (auto& point : collision.Points)
                    point.Normal = -point.Normal;
            for (auto& script : scripts)
            {
                switch (event.Type)
                {
                case PhysicsContactEventType3D::Enter:
                    script.OnCollisionEnter3D(collision);
                    break;
                case PhysicsContactEventType3D::Stay:
                    script.OnCollisionStay3D(collision);
                    break;
                case PhysicsContactEventType3D::Exit:
                    script.OnCollisionExit3D(collision);
                    break;
                }
            }
        };

        dispatch(firstHandle, secondHandle, false);
        dispatch(secondHandle, firstHandle, true);
    }

    void Scene::OnAudioSourceComponentConstruct(entt::registry& registry, entt::entity entity)
    {
        if (!AudioManager::IsStartedUp() || !m_RuntimeActive)
            return;
        Entity e = { entity, this };
        AudioSourceComponent& source = e.GetComponent<AudioSourceComponent>();
        source.OnInitialize();
    }

    void Scene::OnAudioSourceComponentUpdate(entt::registry& registry, entt::entity entity)
    {
        if (!AudioManager::IsStartedUp() || !m_RuntimeActive)
            return;
        registry.get<AudioSourceComponent>(entity).ApplyRuntimeSettings();
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

    bool Scene::HasScriptComponent(Entity entity, const ScriptTypeIdentity& identity) const
    {
        if (entity.HasComponent<MonoScriptComponent>())
        {
            const MonoScriptComponent& monoScriptComponent = entity.GetComponent<MonoScriptComponent>();
            for (const MonoScript& script : monoScriptComponent.Scripts)
            {
                if (script.GetTypeIdentity() == identity)
                    return true;
            }
        }
        return false;
    }

    bool Scene::HasScriptComponent(Entity entity, const String& namespaceName, const String& typeName) const
    {
        return HasScriptComponent(entity, { GAME_ASSEMBLY, namespaceName, typeName });
    }

    bool Scene::AddScriptComponent(Entity entity, const ScriptTypeIdentity& identity, bool initialize)
    {
        return AddScriptComponent(entity, PersistedScriptState{ identity, nullptr }, initialize);
    }

    bool Scene::AddScriptComponent(Entity entity, const PersistedScriptState& state, bool initialize)
    {
        if (!entity || entity.GetScene() != this || !state.Identity.IsValid())
        {
            CW_ENGINE_WARN("Cannot attach managed script with invalid identity '{}:{}'.", state.Identity.Assembly, state.Identity.GetFullName());
            return false;
        }

        MonoScriptComponent* monoScriptComponent = nullptr;
        if (entity.HasComponent<MonoScriptComponent>())
            monoScriptComponent = &entity.GetComponent<MonoScriptComponent>();
        if (monoScriptComponent != nullptr)
        {
            for (const MonoScript& script : monoScriptComponent->Scripts)
            {
                if (script.GetTypeIdentity() == state.Identity)
                {
                    CW_ENGINE_WARN("Entity '{}' already has managed script '{}:{}'. The duplicate was ignored.", entity.GetName(),
                                   state.Identity.Assembly, state.Identity.GetFullName());
                    return false;
                }
            }
        }
        else
            monoScriptComponent = &entity.AddComponent<MonoScriptComponent>();

        monoScriptComponent->Scripts.emplace_back(state.Identity);
        MonoScript& script = monoScriptComponent->Scripts.back();
        script.ApplyPersistedState(state);
        if (initialize)
        {
            script.Create(entity);
            MonoClass* monoClass = script.GetManagedClass();
            MonoClass* runInEditor = ScriptInfoManager::IsStartedUp() ? ScriptInfoManager::Get().GetBuiltinClasses().RunInEditorAttribute : nullptr;
            if (m_RuntimeActive || (m_IsEditorScene && monoClass != nullptr && runInEditor != nullptr && monoClass->HasAttribute(runInEditor)))
                script.OnStart();
        }
        return true;
    }

    bool Scene::AddScriptComponent(Entity entity, const String& namespaceName, const String& typeName, bool initialize)
    {
        return AddScriptComponent(entity, ScriptTypeIdentity{ GAME_ASSEMBLY, namespaceName, typeName }, initialize);
    }

    void Scene::RemoveScriptComponent(Entity entity, const ScriptTypeIdentity& identity)
    {
        if (!entity.HasComponent<MonoScriptComponent>())
            return;
        auto& scripts = entity.GetComponent<MonoScriptComponent>().Scripts;
        const auto script =
          std::find_if(scripts.begin(), scripts.end(), [&](const MonoScript& candidate) { return candidate.GetTypeIdentity() == identity; });
        if (script == scripts.end())
            return;
        if (ScriptSceneObjectManager::IsStartedUp())
            ScriptSceneObjectManager::Get().DestroyManagedScriptComponent(entity, &*script);
        scripts.erase(script);
        if (scripts.empty())
            entity.RemoveComponent<MonoScriptComponent>();
    }

    void Scene::RemoveScriptComponent(Entity entity, const String& namespaceName, const String& typeName)
    {
        RemoveScriptComponent(entity, { GAME_ASSEMBLY, namespaceName, typeName });
    }

    void Scene::OnRuntimeStart()
    {
        m_RuntimeActive = true;
        if (Physics2D::TryGet() != nullptr)
        {
            Physics2D::TryGet()->BeginSimulation(this);
            m_Physics2DActive = true;
        }
        BeginPhysics3D();
        if (AudioManager::TryGet() != nullptr)
        {
            auto listenerView = m_Registry.view<AudioListenerComponent>();
            uint32_t listenerCount = 0;
            for ([[maybe_unused]] entt::entity entity : listenerView)
                ++listenerCount;
            if (listenerCount == 0)
                CW_ENGINE_WARN("No audio listener in scene");
            else
            {
                if (listenerCount > 1)
                    CW_ENGINE_WARN("Multiple audio listeners in scene; using the first enabled listener");
                for (auto e : listenerView)
                {
                    Entity entity = { e, this };
                    entity.GetComponent<AudioListenerComponent>().Initialize();
                    break;
                }
            }
            m_Registry.view<AudioSourceComponent>().each([](AudioSourceComponent& source) { source.OnInitialize(); });
        }
        m_Registry.view<AnimationComponent>().each([](AnimationComponent& animation) { animation.ResetRuntime(); });
    }

    void Scene::OnSimulationStart()
    {
        m_SimulationActive = true;
        if (Physics2D::TryGet() != nullptr)
        {
            Physics2D::TryGet()->BeginSimulation(this);
            m_Physics2DActive = true;
        }
        BeginPhysics3D();
    }

    void Scene::OnSimulationUpdate(Timestep ts)
    {
        SceneManager::CallbackScope callbackScope =
          SceneManager::TryGet() != nullptr ? SceneManager::TryGet()->DeferSceneChanges() : SceneManager::CallbackScope();
        if (m_Physics2DActive && Physics2D::TryGet() != nullptr)
            Physics2D::TryGet()->Step(ts, this);
        StepPhysics3D(ts);
    }

    void Scene::OnSimulationEnd()
    {
        EndPhysics3D();
        if (Physics2D::TryGet() != nullptr)
            Physics2D::TryGet()->StopSimulation(this);
        m_Physics2DActive = false;
        m_SimulationActive = false;
    }

    void Scene::OnRuntimePause()
    {
        if (AudioManager::TryGet() != nullptr)
        {
            auto audioSourceView = m_Registry.view<AudioSourceComponent>();
            for (auto e : audioSourceView)
            {
                Entity entity = { e, this };
                entity.GetComponent<AudioSourceComponent>().Pause();
            }
        }
        m_Registry.view<AnimationComponent>().each([](AnimationComponent& animation) {
            if (animation.Player)
                animation.Player->Pause();
        });
    }

    void Scene::OnRuntimeResume()
    {
        if (AudioManager::TryGet() != nullptr)
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
        m_Registry.view<AnimationComponent>().each([](AnimationComponent& animation) {
            if (animation.Player)
                animation.Player->Resume();
        });
    }

    void Scene::OnRuntimeStop()
    {
        EndPhysics3D();
        if (Physics2D::TryGet() != nullptr)
            Physics2D::TryGet()->StopSimulation(this);
        m_Physics2DActive = false;
        if (AudioManager::TryGet() != nullptr)
        {
            auto audioSourceView = m_Registry.view<AudioSourceComponent>();
            for (auto e : audioSourceView)
            {
                Entity entity = { e, this };
                entity.GetComponent<AudioSourceComponent>().Stop();
            }
        }
        m_Registry.view<AnimationComponent>().each([](AnimationComponent& animation) { animation.ResetRuntime(); });
        m_RuntimeActive = false;
    }

    void Scene::OnUpdateEditor(Timestep ts) {}

    void Scene::OnUpdateRuntime(Timestep ts) {}

    void Scene::OnFixedUpdate(Timestep ts)
    {
        SceneManager::CallbackScope callbackScope =
          SceneManager::TryGet() != nullptr ? SceneManager::TryGet()->DeferSceneChanges() : SceneManager::CallbackScope();
        if (m_Physics2DActive && Physics2D::TryGet() != nullptr)
            Physics2D::TryGet()->Step(ts, this);
        StepPhysics3D(ts);
    }

    Entity Scene::CreateEntity(const String& name)
    {
        return CreateEntityInternal(UuidGenerator::Generate(), name, m_RootEntity ? *m_RootEntity : Entity{});
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

    Entity Scene::CreateEntityInternal(const UUID& uuid, const String& name, Entity parent)
    {
        Entity entity(m_Registry.create(), this);

        entity.AddComponent<IDComponent>(uuid);
        m_EntityMap[uuid] = entity.GetHandle();
        entity.AddComponent<TagComponent>(name);
        entity.AddComponent<TransformComponent>();
        entity.AddComponent<RelationshipComponent>();
        if (parent)
            entity.SetParent(parent);

        return entity;
    }

    void Scene::DestroyEntity(Entity entity) { entity.Destroy(); }

    Entity Scene::TryGetEntityFromUuid(const UUID& uuid) const
    {
        const auto entity = m_EntityMap.find(uuid);
        if (entity != m_EntityMap.end() && m_Registry.valid(entity->second))
            return { entity->second, const_cast<Scene*>(this) };

        return {};
    }

    Entity Scene::GetEntityFromUuid(const UUID& uuid) const
    {
        Entity entity = TryGetEntityFromUuid(uuid);
        if (entity)
            return entity;

        CW_ENGINE_ERROR("Entity with uuid {0} not found.", uuid);
        return {};
    }

    Entity Scene::GetRootEntity() const { return m_RootEntity ? *m_RootEntity : Entity{}; }

    Entity Scene::FindEntityByName(const String& name) const
    {
        const auto view = m_Registry.view<TagComponent>();
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
