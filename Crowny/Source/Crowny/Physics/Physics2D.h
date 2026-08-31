#pragma once

#include "Crowny/Assets/AssetHandle.h"
#include "Crowny/Common/Module.h"
#include "Crowny/Ecs/Entity.h"
#include "Crowny/Physics/Physics2DBackend.h"
#include "Crowny/Physics/PhysicsMaterial.h"

#include <glm/glm.hpp>

namespace Crowny
{
    inline constexpr uint32_t Physics2DLayerCount = 16;

    enum class ForceMode;
    struct Collider2D;
    struct Rigidbody2DComponent;

    class PhysicsSettingsSerializer;
    struct Physics2DSettings : public RefCounted
    {
        AssetHandle<PhysicsMaterial2D> DefaultMaterial;
        Physics2DBackendType Backend = Physics2DBackendType::Box2D;
        glm::vec2 Gravity = { 0.0f, -9.81f };
        uint32_t VelocityIterations = 8;
        uint32_t PositionIterations = 3;

        Array<String, 32> LayerNames = { "Default" };
        Array<uint32_t, 32> MaskBits = {};
    };

    class Physics2D : public Module<Physics2D>
    {
    public:
        Physics2D();
        ~Physics2D();

        void SetBackend(Physics2DBackendType backend);
        Physics2DBackendType GetBackend() const { return m_Settings->Backend; }
        const char* GetBackendName() const;

        void SetDefaultMaterial(const AssetHandle<PhysicsMaterial2D>& material);
        void SetGravity(const glm::vec2& gravity);
        void SetVelocityIterations(uint32_t iterations);
        void SetPositionIterations(uint32_t iterations);

        const AssetHandle<PhysicsMaterial2D>& GetDefaultMaterial() const { return m_Settings->DefaultMaterial; }
        const glm::vec2& GetGravity() const { return m_Settings->Gravity; }
        uint32_t GetVelocityIterations() const { return m_Settings->VelocityIterations; }
        uint32_t GetPositionIterations() const { return m_Settings->PositionIterations; }

        void SetCategoryMask(uint32_t idx, uint32_t mask);
        uint32_t GetCategoryMask(uint32_t idx) const;
        const String& GetLayerName(uint32_t idx) const;
        void SetLayerName(uint32_t idx, const String& name);

        void BeginSimulation(Scene* scene);
        void CreateRigidbody(Entity entity);
        void CreateBoxCollider(Entity entity);
        void CreateCircleCollider(Entity entity);
        void DestroyRigidbody(Entity entity);
        void DestroyFixture(Entity entity, Collider2D& collider);
        void Step(Timestep ts, Scene* scene);
        void StopSimulation(Scene* scene);

        bool IsSimulating() const;
        bool IsBodyAwake(Entity entity) const;
        float GetMass(Entity entity) const;
        float GetInertia(Entity entity) const;
        glm::vec2 GetCenterOfMass(Entity entity) const;
        glm::vec2 GetPosition(Entity entity) const;
        float GetRotation(Entity entity) const;
        glm::vec2 GetLinearVelocity(Entity entity) const;
        float GetAngularVelocity(Entity entity) const;
        void SetLinearVelocity(Entity entity, const glm::vec2& velocity);
        void SetAngularVelocity(Entity entity, float velocity);
        void SetBodyAwake(Entity entity, bool awake);

        void UpdateLayer(Entity entity);
        void UpdateTransform(Entity entity);
        void UpdateBodyType(Rigidbody2DComponent& rigidbody);
        void UpdateMass(Rigidbody2DComponent& rigidbody, float mass);
        void UpdateInertia(Rigidbody2DComponent& rigidbody, float inertia);
        void ResetMass(Entity entity);
        void UpdateGravityScale(Rigidbody2DComponent& rigidbody, float scale);
        void UpdateConstraints(Rigidbody2DComponent& rigidbody);
        void UpdateCollisionDetectionMode(Rigidbody2DComponent& rigidbody);
        void UpdateSleepMode(Rigidbody2DComponent& rigidbody);
        void UpdateLinearDrag(Rigidbody2DComponent& rigidbody, float linearDrag);
        void UpdateAngularDrag(Rigidbody2DComponent& rigidbody, float angularDrag);
        void UpdateCenterOfMass(Rigidbody2DComponent& rigidbody, const glm::vec2& center);
        void UpdateTrigger(Collider2D& collider, bool trigger);
        void UpdateMaterial(Collider2D& collider);

        void AddForce(Entity entity, const glm::vec2& force, ForceMode mode);
        void AddForceAt(Entity entity, const glm::vec2& force, const glm::vec2& worldPosition, ForceMode mode);
        void AddTorque(Entity entity, float torque, ForceMode mode);

        Vector<PhysicsRaycastHit2D> Raycast(const glm::vec2& origin, const glm::vec2& direction, float distance,
                                            uint32_t layerMask = 0xFFFFFFFF) const;

        Ref<Physics2DSettings> GetPhysicsSettings() const { return m_Settings; }
        void SetPhysicsSettings(const Ref<Physics2DSettings>& settings);

        float CalculateMass(Entity entity) const;
        glm::vec2 CalculateCenterOfMass(Entity entity) const;

        void UIStats();

    private:
        void CreateBackend(Physics2DBackendType backend);

        Scope<Physics2DBackend> m_Backend;
        Ref<Physics2DSettings> m_Settings;
        Scene* m_Scene = nullptr;
    };
} // namespace Crowny
