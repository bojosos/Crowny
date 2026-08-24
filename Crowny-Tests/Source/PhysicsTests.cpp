#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "Crowny/Ecs/Components.h"
#include "Crowny/Physics/Physics2D.h"
#include "Crowny/Physics/Physics2DBackend.h"
#include "Crowny/Physics/Physics3D.h"
#include "Crowny/Scene/Scene.h"

#include <array>
#include <limits>
#include <string_view>

using namespace Crowny;

namespace
{
    struct BackendCase
    {
        Physics3DBackendType Type;
        const char* Name;
    };

    constexpr std::array<BackendCase, 3> BackendCases = {
        BackendCase{ Physics3DBackendType::Box3D, "Box3D 0.1" },
        BackendCase{ Physics3DBackendType::Jolt, "Jolt 5.6" },
        BackendCase{ Physics3DBackendType::Bullet, "Bullet 3.25" },
    };

    Scope<Physics3DBackend> CreateBackend(Physics3DBackendType type)
    {
        switch (type)
        {
        case Physics3DBackendType::Box3D:
            return CreateBox3DBackend();
        case Physics3DBackendType::Jolt:
            return CreateJoltPhysicsBackend();
        case Physics3DBackendType::Bullet:
            return CreateBulletPhysicsBackend();
        }
        return nullptr;
    }
} // namespace

TEST_CASE("Box2D implements the backend-neutral 2D contract", "[Physics][Physics2D]")
{
    Scope<Physics2DBackend> backend = CreateBox2DBackend();
    REQUIRE(backend != nullptr);

    Ref<Scene> scene = CreateRef<Scene>(false);
    Entity body = scene->CreateEntity("2D body");
    auto& rigidbody = body.AddComponent<Rigidbody2DComponent>();
    rigidbody.SetBodyType(RigidbodyBodyType::Dynamic);
    rigidbody.SetLayerMask(99, body);
    CHECK(rigidbody.GetLayerMask() == Physics2DLayerCount - 1);
    body.AddComponent<BoxCollider2DComponent>();

    Physics2DSettings settings;
    settings.Gravity = glm::vec2(0.0f);
    settings.MaskBits.fill(0xFFFFu);
    backend->BeginSimulation(scene.get(), settings);

    CHECK(backend->IsSimulating());
    CHECK(backend->GetBodyCount() == 1);
    const Vector<PhysicsRaycastHit2D> hits = backend->Raycast({ 0.0f, 2.0f }, { 0.0f, -1.0f }, 4.0f, 0xFFFFFFFF);
    REQUIRE_FALSE(hits.empty());
    CHECK(hits.front().HitEntity == body);

    backend->SetLinearVelocity(body, { 2.0f, -3.0f });
    backend->SetAngularVelocity(body, 1.5f);
    CHECK(backend->GetLinearVelocity(body) == glm::vec2(2.0f, -3.0f));
    CHECK(backend->GetAngularVelocity(body) == 1.5f);
    backend->SetBodyAwake(body, false);
    CHECK_FALSE(backend->IsBodyAwake(body));

    backend->StopSimulation(scene.get());
    CHECK_FALSE(backend->IsSimulating());
    CHECK(body.GetComponent<Rigidbody2DComponent>().RuntimeBody == nullptr);
}

TEST_CASE("2D physics material values enforce backend-neutral ranges", "[Physics][Physics2D][Material]")
{
    PhysicsMaterial2D material;
    material.SetDensity(-1.0f);
    material.SetFriction(-2.0f);
    material.SetRestitution(4.0f);
    material.SetRestitutionThreshold(-3.0f);

    CHECK(material.GetDensity() == 0.0f);
    CHECK(material.GetFriction() == 0.0f);
    CHECK(material.GetRestitution() == 1.0f);
    CHECK(material.GetRestitutionThreshold() == 0.0f);
}

TEST_CASE("Physics materials use deterministic backend-neutral combine rules", "[Physics][Material]")
{
    PhysicsMaterial3D material;
    PhysicsMaterialData data;
    data.Density = std::numeric_limits<float>::quiet_NaN();
    data.Friction = -2.0f;
    data.Restitution = 4.0f;
    data.RestitutionThreshold = std::numeric_limits<float>::infinity();
    data.FrictionCombine = static_cast<PhysicsCombineMode>(255);
    material.SetData(data);

    CHECK(material.GetDensity() == 1.0f);
    CHECK(material.GetFriction() == 0.0f);
    CHECK(material.GetRestitution() == 1.0f);
    CHECK(material.GetRestitutionThreshold() == 0.5f);
    CHECK(material.GetFrictionCombine() == PhysicsCombineMode::GeometricMean);
    CHECK(ResolvePhysicsCombineMode(PhysicsCombineMode::Average, PhysicsCombineMode::Minimum) == PhysicsCombineMode::Minimum);
    CHECK_THAT(CombinePhysicsMaterialValue(0.2f, PhysicsCombineMode::Minimum, 0.8f, PhysicsCombineMode::Average),
               Catch::Matchers::WithinAbs(0.2f, 0.0001f));
    CHECK_THAT(CombinePhysicsMaterialValue(0.2f, PhysicsCombineMode::Multiply, 0.8f, PhysicsCombineMode::GeometricMean),
               Catch::Matchers::WithinAbs(0.16f, 0.0001f));
    CHECK_THAT(CombinePhysicsMaterialValue(0.2f, PhysicsCombineMode::Maximum, 0.8f, PhysicsCombineMode::Minimum),
               Catch::Matchers::WithinAbs(0.8f, 0.0001f));
}

TEST_CASE("3D backend factories are safe before initialization", "[Physics][Physics3D]")
{
    uint32_t compiledBackends = 0;
    for (const BackendCase& backendCase : BackendCases)
    {
        Scope<Physics3DBackend> backend = CreateBackend(backendCase.Type);
        if (!backend)
            continue;
        ++compiledBackends;
        CHECK(backend->GetType() == backendCase.Type);
        CHECK(std::string_view(backend->GetName()) == backendCase.Name);
    }
    CHECK(compiledBackends > 0);
}

TEST_CASE("3D backends implement the common contract", "[Physics][Physics3D]")
{
    for (const BackendCase& backendCase : BackendCases)
    {
        DYNAMIC_SECTION(backendCase.Name)
        {
            Scope<Physics3DBackend> backend = CreateBackend(backendCase.Type);
            if (!backend)
                SKIP("Selected backend is not compiled into this build");

            CHECK(backend->GetType() == backendCase.Type);
            CHECK(std::string_view(backend->GetName()) == backendCase.Name);
            CHECK(HasCapability(backend->GetCapabilities(), Physics3DCapability::RigidBodies));
            CHECK(HasCapability(backend->GetCapabilities(), Physics3DCapability::RayCasts));
            CHECK(HasCapability(backend->GetCapabilities(), Physics3DCapability::Constraints));

            Vector<PhysicsContactEvent3D> contacts;
            Physics3DSettings settings;
            settings.Backend = backendCase.Type;
            settings.Gravity = glm::vec3(0.0f);
            REQUIRE(backend->Initialize(settings, [&](const PhysicsContactEvent3D& event) { contacts.push_back(event); }));

            PhysicsBody3DDesc staticBodyDesc;
            staticBodyDesc.Type = PhysicsBodyType3D::Static;
            staticBodyDesc.UserData = 10;
            const PhysicsBody3DHandle staticBody = backend->CreateBody(staticBodyDesc);
            REQUIRE(staticBody);

            PhysicsShape3DDesc box;
            box.Type = PhysicsShapeType3D::Box;
            box.HalfExtents = glm::vec3(1.0f);
            box.UserData = 11;
            box.Material.Friction = 0.25f;
            box.Material.FrictionCombine = PhysicsCombineMode::Multiply;
            box.Material.Restitution = 0.4f;
            box.Material.RestitutionCombine = PhysicsCombineMode::Maximum;
            const PhysicsShape3DHandle staticShape = backend->AddShape(staticBody, box);
            REQUIRE(staticShape);

            PhysicsBody3DDesc dynamicBodyDesc;
            dynamicBodyDesc.Type = PhysicsBodyType3D::Dynamic;
            dynamicBodyDesc.Position = { 0.0f, 0.5f, 0.0f };
            dynamicBodyDesc.UserData = 20;
            const PhysicsBody3DHandle dynamicBody = backend->CreateBody(dynamicBodyDesc);
            REQUIRE(dynamicBody);

            PhysicsShape3DDesc sphere;
            sphere.Type = PhysicsShapeType3D::Sphere;
            sphere.Radius = 0.75f;
            sphere.UserData = 21;
            sphere.Material.Friction = 0.8f;
            sphere.Material.FrictionCombine = PhysicsCombineMode::Minimum;
            sphere.Material.Restitution = 0.6f;
            sphere.Material.RestitutionCombine = PhysicsCombineMode::Average;
            const PhysicsShape3DHandle dynamicShape = backend->AddShape(dynamicBody, sphere);
            REQUIRE(dynamicShape);

            PhysicsMaterialData updatedMaterial = sphere.Material;
            updatedMaterial.Friction = 0.5f;
            updatedMaterial.RestitutionThreshold = 0.25f;
            backend->SetShapeMaterial(dynamicShape, updatedMaterial);

            const Vector<PhysicsQueryHit3D> rayHits = backend->Raycast({ 0.0f, 4.0f, 0.0f }, { 0.0f, -1.0f, 0.0f }, 8.0f, {});
            REQUIRE_FALSE(rayHits.empty());

            const Vector<PhysicsQueryHit3D> overlaps = backend->Overlap(sphere, { 0.0f, 0.5f, 0.0f }, glm::quat(1.0f, 0.0f, 0.0f, 0.0f), {});
            REQUIRE_FALSE(overlaps.empty());

            PhysicsConstraint3DDesc constraintDesc;
            constraintDesc.Type = PhysicsConstraintType3D::Distance;
            constraintDesc.BodyA = staticBody;
            constraintDesc.BodyB = dynamicBody;
            constraintDesc.EnableLimits = true;
            constraintDesc.EnableCollision = true;
            constraintDesc.MinimumLimit = 0.0f;
            constraintDesc.MaximumLimit = 2.0f;
            const PhysicsConstraint3DHandle constraint = backend->CreateConstraint(constraintDesc);
            REQUIRE(constraint);

            backend->Step(1.0f / 60.0f, 4);
            CHECK_FALSE(contacts.empty());

            backend->DestroyConstraint(constraint);
            backend->DestroyBody(dynamicBody);
            backend->DestroyBody(staticBody);
            backend->Shutdown();
        }
    }
}
