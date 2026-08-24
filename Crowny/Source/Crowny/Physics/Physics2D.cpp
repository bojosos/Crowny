#include "cwpch.h"

#include "Crowny/Assets/AssetManager.h"
#include "Crowny/Common/Math.h"
#include "Crowny/Ecs/Components.h"
#include "Crowny/Physics/Physics2D.h"
#include "Crowny/Scene/SceneManager.h"

#include <imgui.h>

#include <algorithm>
#include <cmath>

namespace Crowny
{
    namespace
    {
        bool DecomposeWorldTransform(Entity entity, glm::vec3& position, glm::quat& rotation, glm::vec3& scale)
        {
            return entity && Math::DecomposeMatrix(entity.GetWorldMatrix(), position, rotation, scale);
        }

        glm::vec2 GetWorldScale2D(Entity entity)
        {
            glm::vec3 position, scale;
            glm::quat rotation;
            return DecomposeWorldTransform(entity, position, rotation, scale) ? glm::abs(glm::vec2(scale))
                                                                              : glm::abs(glm::vec2(entity.GetLocalScale()));
        }
    } // namespace
    Physics2D::Physics2D()
    {
        m_Settings = CreateRef<Physics2DSettings>();
        m_Settings->DefaultMaterial = static_asset_cast<PhysicsMaterial2D>(AssetManager::TryGet()->CreateAssetHandle(CreateRef<PhysicsMaterial2D>()));
        m_Settings->MaskBits.fill(0xFFFFu);
        CreateBackend(m_Settings->Backend);
    }

    Physics2D::~Physics2D()
    {
        if (m_Backend && m_Backend->IsSimulating())
            m_Backend->StopSimulation(m_Scene);
    }

    void Physics2D::CreateBackend(Physics2DBackendType backend)
    {
        switch (backend)
        {
        case Physics2DBackendType::Box2D:
            m_Backend = CreateBox2DBackend();
            break;
        default:
            CW_ENGINE_ERROR("Unknown 2D physics backend; falling back to Box2D");
            m_Backend = CreateBox2DBackend();
            backend = Physics2DBackendType::Box2D;
            break;
        }
        m_Settings->Backend = backend;
    }

    void Physics2D::SetBackend(Physics2DBackendType backend)
    {
        if (m_Backend && backend == m_Backend->GetType())
        {
            m_Settings->Backend = backend;
            return;
        }

        Scene* scene = m_Scene;
        const bool restart = IsSimulating();
        if (restart)
            StopSimulation(scene);
        CreateBackend(backend);
        if (restart)
            BeginSimulation(scene);
    }

    const char* Physics2D::GetBackendName() const { return m_Backend ? m_Backend->GetName() : "Unavailable"; }

    void Physics2D::UIStats()
    {
        ImGui::Begin("Physics2D Stats");
        ImGui::Text("Backend: %s", GetBackendName());
        ImGui::Text("Bodies: %u", m_Backend ? m_Backend->GetBodyCount() : 0);
        ImGui::End();
    }

    void Physics2D::SetGravity(const glm::vec2& gravity)
    {
        m_Settings->Gravity = gravity;
        if (m_Backend)
            m_Backend->SetGravity(gravity);
    }

    void Physics2D::SetDefaultMaterial(const AssetHandle<PhysicsMaterial2D>& material)
    {
        if (!material.HasUUID())
            return;
        const AssetHandle<PhysicsMaterial2D> previous = m_Settings->DefaultMaterial;
        m_Settings->DefaultMaterial = material;
        if (!SceneManager::TryGet() || !SceneManager::TryGet()->GetActiveScene())
            return;

        Scene* const scene = SceneManager::TryGet()->GetActiveScene().get();
        const auto replaceDefault = [&](Collider2D& collider) {
            if (!collider.GetMaterial().HasUUID() || collider.GetMaterial().GetUUID() == previous.GetUUID())
                collider.SetMaterial(material);
            else if (!collider.GetMaterial())
                collider.RefreshMaterial();
        };
        for (const entt::entity handle : scene->GetAllEntitiesWith<BoxCollider2DComponent>())
            replaceDefault(Entity(handle, scene).GetComponent<BoxCollider2DComponent>());
        for (const entt::entity handle : scene->GetAllEntitiesWith<CircleCollider2DComponent>())
            replaceDefault(Entity(handle, scene).GetComponent<CircleCollider2DComponent>());
    }

    void Physics2D::SetVelocityIterations(uint32_t iterations) { m_Settings->VelocityIterations = std::max(iterations, 1u); }

    void Physics2D::SetPositionIterations(uint32_t iterations) { m_Settings->PositionIterations = std::max(iterations, 1u); }

    uint32_t Physics2D::GetCategoryMask(uint32_t idx) const
    {
        CW_ENGINE_ASSERT(idx < Physics2DLayerCount);
        return idx < Physics2DLayerCount ? m_Settings->MaskBits[idx] & 0xFFFFu : 0;
    }

    const String& Physics2D::GetLayerName(uint32_t idx) const
    {
        CW_ENGINE_ASSERT(idx < Physics2DLayerCount);
        static const String empty;
        return idx < Physics2DLayerCount ? m_Settings->LayerNames[idx] : empty;
    }

    void Physics2D::SetLayerName(uint32_t idx, const String& name)
    {
        CW_ENGINE_ASSERT(idx < Physics2DLayerCount);
        if (idx < Physics2DLayerCount)
            m_Settings->LayerNames[idx] = name;
    }

    void Physics2D::SetCategoryMask(uint32_t idx, uint32_t mask)
    {
        CW_ENGINE_ASSERT(idx < Physics2DLayerCount);
        if (idx >= Physics2DLayerCount)
            return;
        m_Settings->MaskBits[idx] = mask & 0xFFFFu;

        if (!IsSimulating() || SceneManager::TryGet() == nullptr || !SceneManager::TryGet()->GetActiveScene())
            return;
        Scene* scene = SceneManager::TryGet()->GetActiveScene().get();
        for (auto handle : scene->GetAllEntitiesWith<Rigidbody2DComponent>())
        {
            Entity entity(handle, scene);
            if (entity.GetComponent<Rigidbody2DComponent>().GetLayerMask() == idx)
                UpdateLayer(entity);
        }
    }

    void Physics2D::BeginSimulation(Scene* scene)
    {
        if (!scene || !m_Backend)
            return;
        if (IsSimulating())
            StopSimulation(m_Scene);
        m_Scene = scene;
        m_Backend->BeginSimulation(scene, *m_Settings);
    }

    void Physics2D::CreateRigidbody(Entity entity)
    {
        if (!IsSimulating() || !entity || !entity.HasComponent<Rigidbody2DComponent>())
            return;
        m_Backend->CreateRigidbody(entity);
        if (entity.HasComponent<BoxCollider2DComponent>())
            m_Backend->CreateBoxCollider(entity);
        if (entity.HasComponent<CircleCollider2DComponent>())
            m_Backend->CreateCircleCollider(entity);
    }

    void Physics2D::CreateBoxCollider(Entity entity)
    {
        if (IsSimulating() && entity && entity.HasComponent<Rigidbody2DComponent>() && entity.HasComponent<BoxCollider2DComponent>())
            m_Backend->CreateBoxCollider(entity);
    }

    void Physics2D::CreateCircleCollider(Entity entity)
    {
        if (IsSimulating() && entity && entity.HasComponent<Rigidbody2DComponent>() && entity.HasComponent<CircleCollider2DComponent>())
            m_Backend->CreateCircleCollider(entity);
    }

    void Physics2D::DestroyRigidbody(Entity entity)
    {
        if (IsSimulating() && entity && entity.HasComponent<Rigidbody2DComponent>())
            m_Backend->DestroyRigidbody(entity);
    }

    void Physics2D::DestroyFixture(Entity entity, Collider2D& collider)
    {
        if (IsSimulating())
            m_Backend->DestroyFixture(entity, collider);
    }

    void Physics2D::Step(Timestep timestep, Scene* scene)
    {
        if (IsSimulating())
            m_Backend->Step(timestep, scene, *m_Settings);
    }

    void Physics2D::StopSimulation(Scene* scene)
    {
        if (!IsSimulating())
            return;
        m_Backend->StopSimulation(scene ? scene : m_Scene);
        m_Scene = nullptr;
    }

    bool Physics2D::IsSimulating() const { return m_Backend && m_Backend->IsSimulating(); }

    bool Physics2D::IsBodyAwake(Entity entity) const { return IsSimulating() && m_Backend->IsBodyAwake(entity); }

    float Physics2D::GetMass(Entity entity) const { return IsSimulating() ? m_Backend->GetMass(entity) : CalculateMass(entity); }

    float Physics2D::GetInertia(Entity entity) const
    {
        if (IsSimulating())
            return m_Backend->GetInertia(entity);
        return entity && entity.HasComponent<Rigidbody2DComponent>() ? entity.GetComponent<Rigidbody2DComponent>().GetConfiguredInertia() : 0.0f;
    }

    glm::vec2 Physics2D::GetCenterOfMass(Entity entity) const
    {
        return IsSimulating() ? m_Backend->GetCenterOfMass(entity) : CalculateCenterOfMass(entity);
    }

    glm::vec2 Physics2D::GetPosition(Entity entity) const
    {
        if (IsSimulating())
            return m_Backend->GetPosition(entity);
        return entity ? glm::vec2(entity.GetWorldMatrix()[3]) : glm::vec2(0.0f);
    }

    float Physics2D::GetRotation(Entity entity) const
    {
        if (IsSimulating())
            return m_Backend->GetRotation(entity);
        if (!entity)
            return 0.0f;
        glm::vec3 position, scale;
        glm::quat rotation;
        return DecomposeWorldTransform(entity, position, rotation, scale) ? glm::eulerAngles(rotation).z
                                                                          : glm::eulerAngles(entity.GetLocalRotation()).z;
    }

    glm::vec2 Physics2D::GetLinearVelocity(Entity entity) const
    {
        return IsSimulating() ? m_Backend->GetLinearVelocity(entity) : glm::vec2(0.0f);
    }

    float Physics2D::GetAngularVelocity(Entity entity) const { return IsSimulating() ? m_Backend->GetAngularVelocity(entity) : 0.0f; }

    void Physics2D::SetLinearVelocity(Entity entity, const glm::vec2& velocity)
    {
        if (IsSimulating())
            m_Backend->SetLinearVelocity(entity, velocity);
    }

    void Physics2D::SetAngularVelocity(Entity entity, float velocity)
    {
        if (IsSimulating())
            m_Backend->SetAngularVelocity(entity, velocity);
    }

    void Physics2D::SetBodyAwake(Entity entity, bool awake)
    {
        if (IsSimulating())
            m_Backend->SetBodyAwake(entity, awake);
    }

    void Physics2D::UpdateLayer(Entity entity)
    {
        if (!IsSimulating() || !entity || !entity.HasComponent<Rigidbody2DComponent>())
            return;
        const uint32_t layer = std::min(entity.GetComponent<Rigidbody2DComponent>().GetLayerMask(), Physics2DLayerCount - 1);
        m_Backend->SetLayer(entity.GetComponent<Rigidbody2DComponent>(), layer, 1u << layer, GetCategoryMask(layer));
    }

    void Physics2D::UpdateTransform(Entity entity)
    {
        if (IsSimulating() && entity && entity.HasComponent<Rigidbody2DComponent>())
            m_Backend->SetTransform(entity);
    }

    void Physics2D::UpdateBodyType(Rigidbody2DComponent& rigidbody)
    {
        if (IsSimulating())
            m_Backend->SetBodyType(rigidbody);
    }

    void Physics2D::UpdateMass(Rigidbody2DComponent& rigidbody, float mass)
    {
        if (IsSimulating())
            m_Backend->SetMass(rigidbody, mass);
    }

    void Physics2D::UpdateInertia(Rigidbody2DComponent& rigidbody, float inertia)
    {
        if (IsSimulating())
            m_Backend->SetInertia(rigidbody, inertia);
    }

    void Physics2D::ResetMass(Entity entity)
    {
        if (IsSimulating())
            m_Backend->ResetMass(entity);
    }

    void Physics2D::UpdateGravityScale(Rigidbody2DComponent& rigidbody, float scale)
    {
        if (IsSimulating())
            m_Backend->SetGravityScale(rigidbody, scale);
    }

    void Physics2D::UpdateConstraints(Rigidbody2DComponent& rigidbody)
    {
        if (IsSimulating())
            m_Backend->SetConstraints(rigidbody);
    }

    void Physics2D::UpdateCollisionDetectionMode(Rigidbody2DComponent& rigidbody)
    {
        if (IsSimulating())
            m_Backend->SetCollisionDetectionMode(rigidbody);
    }

    void Physics2D::UpdateSleepMode(Rigidbody2DComponent& rigidbody)
    {
        if (IsSimulating())
            m_Backend->SetSleepMode(rigidbody);
    }

    void Physics2D::UpdateLinearDrag(Rigidbody2DComponent& rigidbody, float value)
    {
        if (IsSimulating())
            m_Backend->SetLinearDrag(rigidbody, value);
    }

    void Physics2D::UpdateAngularDrag(Rigidbody2DComponent& rigidbody, float value)
    {
        if (IsSimulating())
            m_Backend->SetAngularDrag(rigidbody, value);
    }

    void Physics2D::UpdateCenterOfMass(Rigidbody2DComponent& rigidbody, const glm::vec2& center)
    {
        if (IsSimulating())
            m_Backend->SetCenterOfMass(rigidbody, center);
    }

    void Physics2D::UpdateTrigger(Collider2D& collider, bool trigger)
    {
        if (IsSimulating())
            m_Backend->SetTrigger(collider, trigger);
    }

    void Physics2D::UpdateMaterial(Collider2D& collider)
    {
        if (IsSimulating())
            m_Backend->SetMaterial(collider);
    }

    void Physics2D::AddForce(Entity entity, const glm::vec2& force, ForceMode mode)
    {
        if (IsSimulating())
            m_Backend->AddForce(entity, force, mode == ForceMode::Impulse);
    }

    void Physics2D::AddForceAt(Entity entity, const glm::vec2& force, const glm::vec2& worldPosition, ForceMode mode)
    {
        if (IsSimulating())
            m_Backend->AddForceAt(entity, force, worldPosition, mode == ForceMode::Impulse);
    }

    void Physics2D::AddTorque(Entity entity, float torque, ForceMode mode)
    {
        if (IsSimulating())
            m_Backend->AddTorque(entity, torque, mode == ForceMode::Impulse);
    }

    Vector<PhysicsRaycastHit2D> Physics2D::Raycast(const glm::vec2& origin, const glm::vec2& direction, float distance, uint32_t layerMask) const
    {
        if (!IsSimulating() || distance <= 0.0f || glm::dot(direction, direction) <= 0.0f)
            return {};
        return m_Backend->Raycast(origin, direction, distance, layerMask);
    }

    void Physics2D::SetPhysicsSettings(const Ref<Physics2DSettings>& settings)
    {
        if (!settings)
            return;
        const Physics2DBackendType requestedBackend = settings->Backend;
        const AssetHandle<PhysicsMaterial2D> previousDefaultMaterial = m_Settings ? m_Settings->DefaultMaterial : AssetHandle<PhysicsMaterial2D>();
        const AssetHandle<PhysicsMaterial2D> requestedDefaultMaterial = settings->DefaultMaterial;
        m_Settings = settings;
        m_Settings->DefaultMaterial = previousDefaultMaterial;
        if (requestedDefaultMaterial.HasUUID())
            SetDefaultMaterial(requestedDefaultMaterial);
        for (uint32_t layer = 0; layer < Physics2DLayerCount; ++layer)
            m_Settings->MaskBits[layer] &= 0xFFFFu;
        if (!m_Backend || m_Backend->GetType() != requestedBackend)
            SetBackend(requestedBackend);
        SetGravity(settings->Gravity);
    }

    float Physics2D::CalculateMass(Entity entity) const
    {
        if (!entity || !entity.HasComponent<Rigidbody2DComponent>())
            return 0.0f;

        float mass = 0.0f;
        const glm::vec2 scale = GetWorldScale2D(entity);
        if (entity.HasComponent<BoxCollider2DComponent>())
        {
            const auto& collider = entity.GetComponent<BoxCollider2DComponent>();
            const float density = collider.GetMaterialData().Density;
            mass += 4.0f * std::abs(collider.GetSize().x * scale.x * collider.GetSize().y * scale.y) * density;
        }
        if (entity.HasComponent<CircleCollider2DComponent>())
        {
            const auto& collider = entity.GetComponent<CircleCollider2DComponent>();
            const float radius = std::abs(collider.GetRadius()) * 0.5f * (scale.x + scale.y);
            const float density = collider.GetMaterialData().Density;
            mass += 3.14159265358979323846f * radius * radius * density;
        }
        return mass > 0.0f ? mass : entity.GetComponent<Rigidbody2DComponent>().GetConfiguredMass();
    }

    glm::vec2 Physics2D::CalculateCenterOfMass(Entity entity) const
    {
        if (!entity || !entity.HasComponent<Rigidbody2DComponent>())
            return glm::vec2(0.0f);

        glm::vec2 weightedCenter(0.0f);
        float totalMass = 0.0f;
        const glm::vec2 scale = GetWorldScale2D(entity);
        if (entity.HasComponent<BoxCollider2DComponent>())
        {
            const auto& collider = entity.GetComponent<BoxCollider2DComponent>();
            const float density = collider.GetMaterialData().Density;
            const float mass = 4.0f * std::abs(collider.GetSize().x * scale.x * collider.GetSize().y * scale.y) * density;
            weightedCenter += collider.GetOffset() * mass;
            totalMass += mass;
        }
        if (entity.HasComponent<CircleCollider2DComponent>())
        {
            const auto& collider = entity.GetComponent<CircleCollider2DComponent>();
            const float radius = std::abs(collider.GetRadius()) * 0.5f * (scale.x + scale.y);
            const float density = collider.GetMaterialData().Density;
            const float mass = 3.14159265358979323846f * radius * radius * density;
            weightedCenter += collider.GetOffset() * mass;
            totalMass += mass;
        }
        return totalMass > 0.0f ? weightedCenter / totalMass : entity.GetComponent<Rigidbody2DComponent>().GetConfiguredCenterOfMass();
    }
} // namespace Crowny
