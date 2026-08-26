#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "Crowny/Application/Application.h"
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

    void EnsureHeadlessRuntime()
    {
        if (!Application::IsStartedUp())
        {
            ApplicationDesc description;
            description.Name = "PhysicsTests";
            description.Headless = true;
            description.WorkingDirectory = fs::current_path();
            Application::StartUp(description);
        }
        REQUIRE(Physics2D::IsStartedUp());
        REQUIRE(Physics3D::IsStartedUp());
    }

    struct Physics3DBackendRestore
    {
        ~Physics3DBackendRestore()
        {
            if (Physics3D::IsStartedUp() && Physics.GetBackend() != Original)
                Physics.SetBackend(Original);
        }

        Physics3D& Physics;
        Physics3DBackendType Original;
    };
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

TEST_CASE("Active 2D components survive AddOrReplace and clean up", "[Physics][Physics2D][Ecs][Lifecycle]")
{
    EnsureHeadlessRuntime();

    Ref<Scene> scene = CreateRef<Scene>(false);
    Entity body = scene->CreateEntity("Replaceable 2D body");
    auto& rigidbody = body.AddComponent<Rigidbody2DComponent>();
    rigidbody.SetBodyType(RigidbodyBodyType::Dynamic);
    auto& box = body.AddComponent<BoxCollider2DComponent>();
    box.SetSize({ 1.0f, 2.0f }, body);
    auto& circle = body.AddComponent<CircleCollider2DComponent>();
    circle.SetRadius(0.5f, body);

    scene->OnSimulationStart();
    REQUIRE(rigidbody.RuntimeBody != nullptr);
    REQUIRE(box.RuntimeFixture != nullptr);
    REQUIRE(circle.RuntimeFixture != nullptr);

    Physics2D::Get().SetLinearVelocity(body, { 3.0f, -2.0f });
    Physics2D::Get().SetAngularVelocity(body, 1.25f);
    Physics2D::Get().SetBodyAwake(body, false);

    Rigidbody2DComponent bodySettings = rigidbody;
    bodySettings.SetMass(4.0f);
    bodySettings.SetGravityScale(0.5f);
    const uint64_t bodyInstanceId = rigidbody.InstanceId;
    Rigidbody2DComponent* bodyAddress = &rigidbody;
    auto& replacedBody = body.AddOrReplaceComponent<Rigidbody2DComponent>(bodySettings);
    CHECK(&replacedBody == bodyAddress);
    CHECK(replacedBody.InstanceId == bodyInstanceId);
    REQUIRE(replacedBody.RuntimeBody != nullptr);
    CHECK(Physics2D::Get().GetLinearVelocity(body) == glm::vec2(3.0f, -2.0f));
    CHECK(Physics2D::Get().GetAngularVelocity(body) == 1.25f);
    CHECK_FALSE(Physics2D::Get().IsBodyAwake(body));
    CHECK(replacedBody.GetConfiguredMass() == 4.0f);
    CHECK(replacedBody.GetGravityScale() == 0.5f);

    BoxCollider2DComponent boxSettings = box;
    boxSettings.SetSize({ 4.0f, 6.0f }, {});
    boxSettings.SetIsTrigger(true);
    const uint64_t boxInstanceId = box.InstanceId;
    auto& replacedBox = body.AddOrReplaceComponent<BoxCollider2DComponent>(boxSettings);
    CHECK(replacedBox.InstanceId == boxInstanceId);
    REQUIRE(replacedBox.RuntimeFixture != nullptr);
    CHECK(replacedBox.GetSize() == glm::vec2(4.0f, 6.0f));
    CHECK(replacedBox.IsTrigger());

    CircleCollider2DComponent circleSettings = circle;
    circleSettings.SetRadius(1.5f, {});
    circleSettings.SetIsTrigger(true);
    const uint64_t circleInstanceId = circle.InstanceId;
    auto& replacedCircle = body.AddOrReplaceComponent<CircleCollider2DComponent>(circleSettings);
    CHECK(replacedCircle.InstanceId == circleInstanceId);
    REQUIRE(replacedCircle.RuntimeFixture != nullptr);
    CHECK(replacedCircle.GetRadius() == 1.5f);
    CHECK(replacedCircle.IsTrigger());

    scene->OnSimulationEnd();
    CHECK(replacedBody.RuntimeBody == nullptr);
    CHECK(replacedBox.RuntimeFixture == nullptr);
    CHECK(replacedCircle.RuntimeFixture == nullptr);
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

TEST_CASE("Active 3D components survive AddOrReplace on every backend", "[Physics][Physics3D][Ecs][Lifecycle]")
{
    EnsureHeadlessRuntime();
    Physics3D& physics = Physics3D::Get();
    [[maybe_unused]] Physics3DBackendRestore restoreBackend{ physics, physics.GetBackend() };

    for (const BackendCase& backendCase : BackendCases)
    {
        DYNAMIC_SECTION(backendCase.Name)
        {
            if (!Physics3D::IsBackendCompiled(backendCase.Type))
                SKIP("Selected backend is not compiled into this build");
            REQUIRE(physics.SetBackend(backendCase.Type));

            Ref<Scene> scene = CreateRef<Scene>(false);
            Entity body = scene->CreateEntity("Replaceable 3D body");
            auto& rigidbody = body.AddComponent<Rigidbody3DComponent>();
            rigidbody.SetBodyType(PhysicsBodyType3D::Dynamic, body);
            rigidbody.SetAutoMass(false, body);
            rigidbody.SetMass(2.0f, body);
            auto& box = body.AddComponent<BoxCollider3DComponent>();
            box.SetSize({ 1.0f, 2.0f, 3.0f }, body);
            auto& sphere = body.AddComponent<SphereCollider3DComponent>();
            sphere.SetRadius(0.75f, body);
            auto& capsule = body.AddComponent<CapsuleCollider3DComponent>();
            capsule.SetRadius(0.5f, body);
            capsule.SetHeight(2.5f, body);

            scene->OnSimulationStart();
            REQUIRE(rigidbody.RuntimeBody);
            REQUIRE(box.RuntimeShape);
            REQUIRE(sphere.RuntimeShape);
            REQUIRE(capsule.RuntimeShape);

            rigidbody.SetLinearVelocity({ 2.0f, -1.0f, 3.0f });
            rigidbody.SetAngularVelocity({ 0.25f, 0.5f, 0.75f });
            rigidbody.SetAwake(false);

            Rigidbody3DComponent bodySettings = rigidbody;
            bodySettings.SetMass(7.0f, {});
            bodySettings.SetGravityScale(0.35f);
            const uint64_t bodyInstanceId = rigidbody.InstanceId;
            Rigidbody3DComponent* bodyAddress = &rigidbody;
            auto& replacedBody = body.AddOrReplaceComponent<Rigidbody3DComponent>(bodySettings);
            CHECK(&replacedBody == bodyAddress);
            CHECK(replacedBody.InstanceId == bodyInstanceId);
            REQUIRE(replacedBody.RuntimeBody);
            CHECK(replacedBody.GetMass() == 7.0f);
            CHECK(replacedBody.GetGravityScale() == 0.35f);
            CHECK(replacedBody.GetLinearVelocity() == glm::vec3(2.0f, -1.0f, 3.0f));
            CHECK(replacedBody.GetAngularVelocity() == glm::vec3(0.25f, 0.5f, 0.75f));
            CHECK_FALSE(replacedBody.IsAwake());

            BoxCollider3DComponent boxSettings = box;
            boxSettings.SetSize({ 4.0f, 5.0f, 6.0f }, {});
            boxSettings.SetIsTrigger(true);
            const uint64_t boxInstanceId = box.InstanceId;
            auto& replacedBox = body.AddOrReplaceComponent<BoxCollider3DComponent>(boxSettings);
            CHECK(replacedBox.InstanceId == boxInstanceId);
            REQUIRE(replacedBox.RuntimeShape);
            CHECK(replacedBox.GetSize() == glm::vec3(4.0f, 5.0f, 6.0f));
            CHECK(replacedBox.IsTrigger());

            SphereCollider3DComponent sphereSettings = sphere;
            sphereSettings.SetRadius(1.25f, {});
            const uint64_t sphereInstanceId = sphere.InstanceId;
            auto& replacedSphere = body.AddOrReplaceComponent<SphereCollider3DComponent>(sphereSettings);
            CHECK(replacedSphere.InstanceId == sphereInstanceId);
            REQUIRE(replacedSphere.RuntimeShape);
            CHECK(replacedSphere.GetRadius() == 1.25f);

            CapsuleCollider3DComponent capsuleSettings = capsule;
            capsuleSettings.SetRadius(0.9f, {});
            capsuleSettings.SetHeight(3.5f, {});
            const uint64_t capsuleInstanceId = capsule.InstanceId;
            auto& replacedCapsule = body.AddOrReplaceComponent<CapsuleCollider3DComponent>(capsuleSettings);
            CHECK(replacedCapsule.InstanceId == capsuleInstanceId);
            REQUIRE(replacedCapsule.RuntimeShape);
            CHECK(replacedCapsule.GetRadius() == 0.9f);
            CHECK(replacedCapsule.GetHeight() == 3.5f);

            scene->OnSimulationEnd();
            CHECK_FALSE(static_cast<bool>(replacedBody.RuntimeBody));
            CHECK_FALSE(static_cast<bool>(replacedBox.RuntimeShape));
            CHECK_FALSE(static_cast<bool>(replacedSphere.RuntimeShape));
            CHECK_FALSE(static_cast<bool>(replacedCapsule.RuntimeShape));
        }
    }
}
