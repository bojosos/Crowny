#pragma once

#include "Crowny/Physics/Physics3DTypes.h"

namespace Crowny
{
    struct Physics3DSettings;

    class Physics3DBackend
    {
    public:
        virtual ~Physics3DBackend() = default;

        virtual Physics3DBackendType GetType() const = 0;
        virtual const char* GetName() const = 0;
        virtual Physics3DCapability GetCapabilities() const = 0;
        virtual bool Initialize(const Physics3DSettings& settings, PhysicsContactCallback3D callback) = 0;
        virtual void Shutdown() = 0;
        virtual void Step(float timestep, uint32_t substeps) = 0;
        virtual void SetGravity(const glm::vec3& gravity) = 0;

        virtual PhysicsBody3DHandle CreateBody(const PhysicsBody3DDesc& desc) = 0;
        virtual void DestroyBody(PhysicsBody3DHandle body) = 0;
        virtual PhysicsShape3DHandle AddShape(PhysicsBody3DHandle body, const PhysicsShape3DDesc& desc) = 0;
        virtual void RemoveShape(PhysicsBody3DHandle body, PhysicsShape3DHandle shape) = 0;

        virtual void SetBodyTransform(PhysicsBody3DHandle body, const glm::vec3& position, const glm::quat& rotation, bool activate) = 0;
        virtual void GetBodyTransform(PhysicsBody3DHandle body, glm::vec3& position, glm::quat& rotation) const = 0;
        virtual void MoveKinematic(PhysicsBody3DHandle body, const glm::vec3& position, const glm::quat& rotation, float timestep) = 0;
        virtual void SetLinearVelocity(PhysicsBody3DHandle body, const glm::vec3& velocity) = 0;
        virtual glm::vec3 GetLinearVelocity(PhysicsBody3DHandle body) const = 0;
        virtual void SetAngularVelocity(PhysicsBody3DHandle body, const glm::vec3& velocity) = 0;
        virtual glm::vec3 GetAngularVelocity(PhysicsBody3DHandle body) const = 0;
        virtual void AddForce(PhysicsBody3DHandle body, const glm::vec3& force, const glm::vec3* point, PhysicsForceMode3D mode) = 0;
        virtual void AddTorque(PhysicsBody3DHandle body, const glm::vec3& torque, PhysicsForceMode3D mode) = 0;
        virtual void SetGravityScale(PhysicsBody3DHandle body, float scale) = 0;
        virtual void SetDamping(PhysicsBody3DHandle body, float linear, float angular) = 0;
        virtual void SetAwake(PhysicsBody3DHandle body, bool awake) = 0;
        virtual bool IsAwake(PhysicsBody3DHandle body) const = 0;
        virtual void SetFilter(PhysicsBody3DHandle body, const PhysicsFilter3D& filter) = 0;
        virtual void SetShapeMaterial(PhysicsShape3DHandle shape, const PhysicsMaterialData& material) = 0;
        virtual void SetShapeTrigger(PhysicsShape3DHandle shape, bool trigger) = 0;

        virtual PhysicsConstraint3DHandle CreateConstraint(const PhysicsConstraint3DDesc& desc) = 0;
        virtual void DestroyConstraint(PhysicsConstraint3DHandle constraint) = 0;

        virtual Vector<PhysicsQueryHit3D> Raycast(const glm::vec3& origin, const glm::vec3& direction, float distance,
                                                  const PhysicsQueryFilter3D& filter) const = 0;
        virtual Vector<PhysicsQueryHit3D> Sweep(const PhysicsShape3DDesc& shape, const glm::vec3& position, const glm::quat& rotation,
                                                const glm::vec3& direction, float distance, const PhysicsQueryFilter3D& filter) const = 0;
        virtual Vector<PhysicsQueryHit3D> Overlap(const PhysicsShape3DDesc& shape, const glm::vec3& position, const glm::quat& rotation,
                                                  const PhysicsQueryFilter3D& filter) const = 0;
    };

    Scope<Physics3DBackend> CreateBox3DBackend();
    Scope<Physics3DBackend> CreateJoltPhysicsBackend();
    Scope<Physics3DBackend> CreateBulletPhysicsBackend();
} // namespace Crowny
