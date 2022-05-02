#pragma once

#include "Crowny/Assets/AssetHandle.h"
#include "Crowny/Common/Module.h"
#include "Crowny/Ecs/Components.h"
#include "Crowny/Ecs/Entity.h"
#include "Crowny/Physics/PhysicsMaterial.h"

#include <glm/glm.hpp>

class b2World;

namespace Crowny
{
    class ContactListener;
    // These are the ones exposed from Box2D by default. It looks like it wouldn't be hard to expose some more of their
    // #define-s
    struct Physics2DSettings
    {
        AssetHandle<PhysicsMaterial2D> DefaultMaterial;
        glm::vec2 Gravity = { 0.0f, -9.81f };
        uint32_t VelocityIterations = 8;
        uint32_t PositionIterations = 3;
    };

    class Physics2D : public Module<Physics2D>
    {
    public:
        Physics2D();
        ~Physics2D();

        void SetDefaultMaterial(const AssetHandle<PhysicsMaterial2D>& material)
        {
            m_Settings->DefaultMaterial = material;
        }
        void SetGravity(const glm::vec2& gravity) { m_Settings->Gravity = gravity; }
        void SetVelocityIterations(uint32_t iterations) { m_Settings->VelocityIterations = iterations; }
        void SetPositionIterations(uint32_t iterations) { m_Settings->PositionIterations = iterations; }

        const AssetHandle<PhysicsMaterial2D>& GetDefaultMaterial() const { return m_Settings->DefaultMaterial; }
        const glm::vec2& GetGravity() const { return m_Settings->Gravity; }
        uint32_t GetVelocityIterations(uint32_t iterations) const { return m_Settings->VelocityIterations; }
        uint32_t GetPositionIterations(uint32_t iterations) const { return m_Settings->PositionIterations; }

        void SetCategoryMask(uint32_t idx, uint32_t mask) { m_MaskBits[idx] = mask; }
        uint32_t GetCategoryMask(uint32_t idx) { return m_MaskBits[idx]; }

        void BeginSimulation(Scene* scene);
        void CreateRigidbody(Entity e);
        void CreateBoxCollider(Entity e);
        void CreateCircleCollider(Entity e);
        void DestroyRigidbody(Entity e);
        void DestroyFixture(Entity entity, const Collider2D& collider);
        void Step(Timestep ts, Scene* scene);
        void StopSimulation(Scene* scene);

    private:
        b2World* m_PhysicsWorld2D = nullptr;
        ContactListener* m_ContactListener2D = nullptr;
        Ref<Physics2DSettings> m_Settings;
        Array<uint32_t, 32> m_MaskBits;
    };
} // namespace Crowny