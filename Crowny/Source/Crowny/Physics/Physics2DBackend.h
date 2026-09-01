#pragma once

#include "Crowny/Common/Timestep.h"
#include "Crowny/Ecs/Entity.h"

#include <glm/glm.hpp>

namespace Crowny
{
    class Scene;
    struct Collider2D;
    struct Physics2DSettings;
    struct Rigidbody2DComponent;

    enum class Physics2DBackendType
    {
        Box2D = 0
    };

    struct PhysicsRaycastHit2D
    {
        glm::vec2 Point{ 0.0f };
        glm::vec2 Normal{ 0.0f };
        float Fraction = 0.0f;
        Entity HitEntity;
    };

    class Physics2DBackend
    {
    public:
        virtual ~Physics2DBackend() = default;

        virtual Physics2DBackendType GetType() const = 0;
        virtual const char* GetName() const = 0;

        virtual void BeginSimulation(Scene* scene, const Physics2DSettings& settings) = 0;
        virtual void StopSimulation(Scene* scene) = 0;
        virtual bool IsSimulating() const = 0;
        virtual void Step(Timestep timestep, Scene* scene, const Physics2DSettings& settings) = 0;
        virtual void SynchronizeTransforms(Scene* scene, float interpolationAlpha, Timestep extrapolationTime) = 0;
        virtual void SetGravity(const glm::vec2& gravity) = 0;
        virtual void SetTransform(Entity entity) = 0;

        virtual void CreateRigidbody(Entity entity) = 0;
        virtual void CreateBoxCollider(Entity entity) = 0;
        virtual void CreateCircleCollider(Entity entity) = 0;
        virtual void DestroyRigidbody(Entity entity) = 0;
        virtual void DestroyFixture(Entity entity, Collider2D& collider) = 0;

        virtual bool IsBodyAwake(Entity entity) const = 0;
        virtual float GetMass(Entity entity) const = 0;
        virtual float GetInertia(Entity entity) const = 0;
        virtual glm::vec2 GetCenterOfMass(Entity entity) const = 0;
        virtual glm::vec2 GetPosition(Entity entity) const = 0;
        virtual float GetRotation(Entity entity) const = 0;
        virtual glm::vec2 GetLinearVelocity(Entity entity) const = 0;
        virtual float GetAngularVelocity(Entity entity) const = 0;
        virtual void SetLinearVelocity(Entity entity, const glm::vec2& velocity) = 0;
        virtual void SetAngularVelocity(Entity entity, float velocity) = 0;
        virtual void SetBodyAwake(Entity entity, bool awake) = 0;

        virtual void SetLayer(Rigidbody2DComponent& rigidbody, uint32_t layer, uint32_t categoryBits, uint32_t maskBits) = 0;
        virtual void SetBodyType(Rigidbody2DComponent& rigidbody) = 0;
        virtual void SetMass(Rigidbody2DComponent& rigidbody, float mass) = 0;
        virtual void SetInertia(Rigidbody2DComponent& rigidbody, float inertia) = 0;
        virtual void ResetMass(Entity entity) = 0;
        virtual void SetGravityScale(Rigidbody2DComponent& rigidbody, float scale) = 0;
        virtual void SetConstraints(Rigidbody2DComponent& rigidbody) = 0;
        virtual void SetCollisionDetectionMode(Rigidbody2DComponent& rigidbody) = 0;
        virtual void SetSleepMode(Rigidbody2DComponent& rigidbody) = 0;
        virtual void SetLinearDrag(Rigidbody2DComponent& rigidbody, float linearDrag) = 0;
        virtual void SetAngularDrag(Rigidbody2DComponent& rigidbody, float angularDrag) = 0;
        virtual void SetCenterOfMass(Rigidbody2DComponent& rigidbody, const glm::vec2& center) = 0;
        virtual void SetTrigger(Collider2D& collider, bool trigger) = 0;
        virtual void SetMaterial(Collider2D& collider) = 0;

        virtual void AddForce(Entity entity, const glm::vec2& force, bool impulse) = 0;
        virtual void AddForceAt(Entity entity, const glm::vec2& force, const glm::vec2& worldPosition, bool impulse) = 0;
        virtual void AddTorque(Entity entity, float torque, bool impulse) = 0;

        virtual Vector<PhysicsRaycastHit2D> Raycast(const glm::vec2& origin, const glm::vec2& direction, float distance,
                                                    uint32_t layerMask) const = 0;

        virtual uint32_t GetBodyCount() const = 0;
    };

    Scope<Physics2DBackend> CreateBox2DBackend();
} // namespace Crowny
