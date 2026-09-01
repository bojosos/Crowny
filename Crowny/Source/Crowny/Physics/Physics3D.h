#pragma once

#include "Crowny/Assets/AssetHandle.h"
#include "Crowny/Common/Module.h"
#include "Crowny/Common/Timestep.h"
#include "Crowny/Physics/Physics3DBackend.h"

namespace Crowny
{
    struct Physics3DSettings
    {
        Physics3DBackendType Backend = Physics3DBackendType::Box3D;
        glm::vec3 Gravity{ 0.0f, -9.81f, 0.0f };
        AssetHandle<PhysicsMaterial3D> DefaultMaterial;
        uint32_t Substeps = 4;
        bool EnableSleeping = true;
        bool EnableContinuousCollision = true;
        bool Deterministic = false;
    };

    class Physics3D : public Module<Physics3D>
    {
    public:
        Physics3D();
        ~Physics3D();

        bool SetBackend(Physics3DBackendType backend);
        Physics3DBackendType GetBackend() const { return m_Settings.Backend; }
        const char* GetBackendName() const;
        Physics3DCapability GetCapabilities() const;
        bool Supports(Physics3DCapability capability) const;
        static bool IsBackendCompiled(Physics3DBackendType backend);

        bool StartSimulation(PhysicsContactCallback3D callback = {});
        void StopSimulation();
        bool IsSimulating() const { return m_Simulating; }
        void Step(Timestep timestep);

        void SetSettings(const Physics3DSettings& settings);
        const Physics3DSettings& GetSettings() const { return m_Settings; }
        void SetGravity(const glm::vec3& gravity);
        void SetDefaultMaterial(const AssetHandle<PhysicsMaterial3D>& material);
        const AssetHandle<PhysicsMaterial3D>& GetDefaultMaterial() const { return m_Settings.DefaultMaterial; }

        PhysicsBody3DHandle CreateBody(const PhysicsBody3DDesc& desc);
        void DestroyBody(PhysicsBody3DHandle body);
        PhysicsShape3DHandle AddShape(PhysicsBody3DHandle body, const PhysicsShape3DDesc& desc);
        void RemoveShape(PhysicsBody3DHandle body, PhysicsShape3DHandle shape);
        void SetBodyTransform(PhysicsBody3DHandle body, const glm::vec3& position, const glm::quat& rotation, bool activate = true);
        void GetBodyTransform(PhysicsBody3DHandle body, glm::vec3& position, glm::quat& rotation) const;
        void MoveKinematic(PhysicsBody3DHandle body, const glm::vec3& position, const glm::quat& rotation, float timestep);
        void SetLinearVelocity(PhysicsBody3DHandle body, const glm::vec3& velocity);
        glm::vec3 GetLinearVelocity(PhysicsBody3DHandle body) const;
        void SetAngularVelocity(PhysicsBody3DHandle body, const glm::vec3& velocity);
        glm::vec3 GetAngularVelocity(PhysicsBody3DHandle body) const;
        void AddForce(PhysicsBody3DHandle body, const glm::vec3& force, PhysicsForceMode3D mode = PhysicsForceMode3D::Force);
        void AddForceAt(PhysicsBody3DHandle body, const glm::vec3& force, const glm::vec3& point,
                        PhysicsForceMode3D mode = PhysicsForceMode3D::Force);
        void AddTorque(PhysicsBody3DHandle body, const glm::vec3& torque, PhysicsForceMode3D mode = PhysicsForceMode3D::Force);
        void SetGravityScale(PhysicsBody3DHandle body, float scale);
        void SetDamping(PhysicsBody3DHandle body, float linear, float angular);
        void SetAwake(PhysicsBody3DHandle body, bool awake);
        bool IsAwake(PhysicsBody3DHandle body) const;
        void SetFilter(PhysicsBody3DHandle body, const PhysicsFilter3D& filter);
        void SetShapeMaterial(PhysicsShape3DHandle shape, const PhysicsMaterialData& material);
        void SetShapeTrigger(PhysicsShape3DHandle shape, bool trigger);

        PhysicsConstraint3DHandle CreateConstraint(const PhysicsConstraint3DDesc& desc);
        void DestroyConstraint(PhysicsConstraint3DHandle constraint);

        Vector<PhysicsQueryHit3D> Raycast(const glm::vec3& origin, const glm::vec3& direction, float distance,
                                          const PhysicsQueryFilter3D& filter = {}) const;
        Vector<PhysicsQueryHit3D> Sweep(const PhysicsShape3DDesc& shape, const glm::vec3& position, const glm::quat& rotation,
                                        const glm::vec3& direction, float distance, const PhysicsQueryFilter3D& filter = {}) const;
        Vector<PhysicsQueryHit3D> Overlap(const PhysicsShape3DDesc& shape, const glm::vec3& position, const glm::quat& rotation,
                                          const PhysicsQueryFilter3D& filter = {}) const;

    private:
        Scope<Physics3DBackend> MakeBackend(Physics3DBackendType backend) const;

        Scope<Physics3DBackend> m_Backend;
        Physics3DSettings m_Settings;
        PhysicsContactCallback3D m_ContactCallback;
        bool m_Simulating = false;
    };
} // namespace Crowny
