#pragma once

#include "Crowny/Assets/AssetHandle.h"
#include "Crowny/Common/Module.h"
#include "Crowny/Ecs/Entity.h"
#include "Crowny/Physics/PhysicsMaterial.h"

#include <glm/glm.hpp>

class b2World;

namespace Crowny
{
    class ContactListener;

	class TimeSettingsSerializer;
	struct TimeSettings
	{
		float TimeScale = 1.0f;
		float MaxTimestep = 1.0f / 3.0f;
		float FixedTimestep = 0.02f;

		using Serializer = TimeSettingsSerializer;
	};

	class PhysicsSettingsSerializer;
	// These are the ones exposed from Box2D by default. It looks like it wouldn't be hard to expose some more of their
	// #define-s
	struct Physics2DSettings
	{
		AssetHandle<PhysicsMaterial2D> DefaultMaterial;
		glm::vec2 Gravity = { 0.0f, -9.81f };
		uint32_t VelocityIterations = 8;
		uint32_t PositionIterations = 3;

		Array<String, 32> LayerNames = { "Default" };
		Array<uint32_t, 32> MaskBits;
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
        void SetGravity(const glm::vec2& gravity);
        void SetVelocityIterations(uint32_t iterations);
        void SetPositionIterations(uint32_t iterations);

        const AssetHandle<PhysicsMaterial2D>& GetDefaultMaterial() const { return m_Settings->DefaultMaterial; }
        const glm::vec2& GetGravity() const { return m_Settings->Gravity; }
        uint32_t GetVelocityIterations() const { return m_Settings->VelocityIterations; }
        uint32_t GetPositionIterations() const { return m_Settings->PositionIterations; }

        void SetCategoryMask(uint32_t idx, uint32_t mask);
        uint32_t GetCategoryMask(uint32_t idx) const { return m_Settings->MaskBits[idx]; }
        const String& GetLayerName(uint32_t idx) const { return m_Settings->LayerNames[idx]; }
        void SetLayerName(uint32_t idx, const String& name) { m_Settings->LayerNames[idx] = name; }

        void BeginSimulation(Scene* scene);
        void CreateRigidbody(Entity entity);
        void CreateBoxCollider(Entity entity);
        void CreateCircleCollider(Entity entity);
        void DestroyRigidbody(Entity entity);
        void DestroyFixture(Entity entity, Collider2D& collider);
        void Step(Timestep ts, Scene* scene);
        void StopSimulation(Scene* scene);

        Ref<Physics2DSettings> GetPhysicsSettings() const { return m_Settings; }
        void SetPhysicsSettings(const Ref<Physics2DSettings>& settings) { m_Settings = settings; }

        float CalculateMass(Entity entity);
        glm::vec2 CalculateCenterOfMass(Entity entity);

        void UIStats();

    private:
        b2World* m_PhysicsWorld2D = nullptr;
        b2World* m_TemporaryWorld2D = nullptr; // A world used for calculating mass
        ContactListener* m_ContactListener2D = nullptr;
        Ref<Physics2DSettings> m_Settings;
        float m_TimestepAcc = 0.0f;
    };
} // namespace Crowny