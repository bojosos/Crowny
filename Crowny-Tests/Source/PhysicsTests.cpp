#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "Crowny/Application/Application.h"
#include "Crowny/Ecs/Components.h"
#include "Crowny/Physics/Physics2D.h"
#include "Crowny/Physics/Physics2DBackend.h"
#include "Crowny/Physics/Physics3D.h"
#include "Crowny/Scene/Scene.h"
#include "Crowny/Scene/ScriptRuntime.h"

#include <array>
#include <limits>
#include <string_view>
#include <tuple>

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

    struct Physics3DStateRestore
    {
        explicit Physics3DStateRestore(Physics3D& physics)
            : Physics(physics), OriginalBackend(physics.GetBackend()), OriginalSettings(physics.GetSettings())
        {
        }

        ~Physics3DStateRestore()
        {
            if (Physics.IsSimulating())
                Physics.StopSimulation();
            if (Physics.GetBackend() != OriginalBackend)
                Physics.SetBackend(OriginalBackend);
            Physics.SetSettings(OriginalSettings);
        }

        Physics3D& Physics;
        Physics3DBackendType OriginalBackend;
        Physics3DSettings OriginalSettings;
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

TEST_CASE("Box2D advances exactly once by the supplied fixed tick", "[Physics][Physics2D][FixedUpdate]")
{
    EnsureHeadlessRuntime();
    Scope<Physics2DBackend> backend = CreateBox2DBackend();
    REQUIRE(backend != nullptr);

    Ref<Scene> scene = CreateRef<Scene>(false);
    Entity body = scene->CreateEntity("Exact-step 2D body");
    auto& rigidbody = body.AddComponent<Rigidbody2DComponent>();
    rigidbody.SetBodyType(RigidbodyBodyType::Dynamic);
    rigidbody.SetInterpolationMode(RigidbodyInterpolation::Interpolate);
    body.AddComponent<BoxCollider2DComponent>();

    Physics2DSettings settings;
    settings.Gravity = glm::vec2(0.0f);
    settings.MaskBits.fill(0xFFFFu);
    backend->BeginSimulation(scene.get(), settings);
    backend->SetLinearVelocity(body, { 1.0f, 0.0f });

    backend->Step(0.01f, scene.get(), settings);
    CHECK(backend->GetPosition(body).x == Catch::Approx(0.01f).margin(0.0001f));
    backend->SynchronizeTransforms(scene.get(), 0.5f, 0.005f);
    CHECK(body.GetWorldTransform().GetPosition().x == Catch::Approx(0.005f).margin(0.0001f));
    backend->SynchronizeTransforms(scene.get(), 1.0f, 0.0f);

    backend->Step(0.01f, scene.get(), settings);
    CHECK(backend->GetPosition(body).x == Catch::Approx(0.02f).margin(0.0001f));

    backend->StopSimulation(scene.get());
}

TEST_CASE("Physics3D advances exactly once by the supplied fixed tick", "[Physics][Physics3D][FixedUpdate]")
{
    EnsureHeadlessRuntime();
    if (!Physics3D::IsBackendCompiled(Physics3DBackendType::Box3D))
        SKIP("Box3D is not compiled into this build");

    Physics3D& physics = Physics3D::Get();
    [[maybe_unused]] Physics3DStateRestore restorePhysics(physics);
    REQUIRE(physics.SetBackend(Physics3DBackendType::Box3D));

    Physics3DSettings settings = physics.GetSettings();
    settings.Gravity = glm::vec3(0.0f);
    settings.Substeps = 1;
    physics.SetSettings(settings);
    REQUIRE(physics.StartSimulation());

    PhysicsBody3DDesc bodyDesc;
    bodyDesc.Type = PhysicsBodyType3D::Dynamic;
    bodyDesc.LinearVelocity = { 1.0f, 0.0f, 0.0f };
    bodyDesc.GravityScale = 0.0f;
    bodyDesc.AllowSleep = false;
    const PhysicsBody3DHandle body = physics.CreateBody(bodyDesc);
    REQUIRE(body);
    physics.AddShape(body, PhysicsShape3DDesc{});

    glm::vec3 position;
    glm::quat rotation;
    physics.Step(0.01f);
    physics.GetBodyTransform(body, position, rotation);
    CHECK(position.x == Catch::Approx(0.01f).margin(0.002f));

    physics.Step(0.01f);
    physics.GetBodyTransform(body, position, rotation);
    CHECK(position.x == Catch::Approx(0.02f).margin(0.002f));

    physics.StopSimulation();
}

TEST_CASE("Runtime and physics simulation consume one shared frame plan", "[Physics][FixedUpdate][Integration]")
{
    EnsureHeadlessRuntime();
    Physics2D& physics = Physics2D::Get();
    const glm::vec2 originalGravity = physics.GetGravity();
    physics.SetGravity(glm::vec2(0.0f));

    const auto runFrame = [&](bool runtime) {
        Ref<Scene> scene = CreateRef<Scene>(false);
        Entity body = scene->CreateEntity(runtime ? "Runtime body" : "Simulation body");
        auto& rigidbody = body.AddComponent<Rigidbody2DComponent>();
        rigidbody.SetBodyType(RigidbodyBodyType::Dynamic);
        body.AddComponent<BoxCollider2DComponent>();

        if (runtime)
            scene->OnRuntimeStart();
        else
            scene->OnSimulationStart();
        Physics2D::Get().SetLinearVelocity(body, { 1.0f, 0.0f });

        Time time;
        TimeSettings settings;
        settings.FixedTimestep = 0.02f;
        time.BeginFrame(0.055f);
        const SimulationFrame frame = time.AdvanceSimulation(settings);
        uint32_t variableUpdates = 0;

        scene->SynchronizePhysicsTransforms(1.0f, 0.0f);
        time.ExecuteSimulationFrame(
          frame,
          [&](Timestep fixedDelta) {
              if (runtime)
              {
                  ScriptRuntime::OnFixedUpdate(scene, fixedDelta);
                  scene->OnFixedUpdate(fixedDelta);
              }
              else
                  scene->OnSimulationFixedUpdate(fixedDelta);
          },
          [&](Timestep frameDelta) {
              ++variableUpdates;
              if (runtime)
              {
                  scene->OnUpdateRuntime(frameDelta);
                  ScriptRuntime::OnUpdate(scene, frameDelta);
              }
          },
          [&](float interpolationAlpha, Timestep extrapolationTime) {
              scene->SynchronizePhysicsTransforms(interpolationAlpha, extrapolationTime);
          });

        CHECK(variableUpdates == 1);
        CHECK(Physics2D::Get().GetPosition(body).x == Catch::Approx(0.04f).margin(0.0001f));
        if (runtime)
            scene->OnRuntimeStop();
        else
            scene->OnSimulationEnd();
    };

    SECTION("Play") { runFrame(true); }
    SECTION("Simulate") { runFrame(false); }

    physics.SetGravity(originalGravity);
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
    Physics2D::Get().SetBodyAwake(body, true);

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
    CHECK(Physics2D::Get().IsBodyAwake(body));
    CHECK(replacedBody.GetConfiguredMass() == 4.0f);
    CHECK(replacedBody.GetGravityScale() == 0.5f);

    Physics2D::Get().SetBodyAwake(body, false);
    const glm::vec2 sleepingLinearVelocity = Physics2D::Get().GetLinearVelocity(body);
    const float sleepingAngularVelocity = Physics2D::Get().GetAngularVelocity(body);
    Rigidbody2DComponent sleepingSettings = replacedBody;
    sleepingSettings.SetLinearDrag(0.3f);
    sleepingSettings.SetAngularDrag(0.4f);
    auto& sleepingBody = body.AddOrReplaceComponent<Rigidbody2DComponent>(sleepingSettings);
    CHECK(&sleepingBody == bodyAddress);
    CHECK(sleepingBody.InstanceId == bodyInstanceId);
    REQUIRE(sleepingBody.RuntimeBody != nullptr);
    CHECK(Physics2D::Get().GetLinearVelocity(body) == sleepingLinearVelocity);
    CHECK(Physics2D::Get().GetAngularVelocity(body) == sleepingAngularVelocity);
    CHECK_FALSE(Physics2D::Get().IsBodyAwake(body));
    CHECK(sleepingBody.GetLinearDrag() == 0.3f);
    CHECK(sleepingBody.GetAngularDrag() == 0.4f);

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

TEST_CASE("Physics material overrides resolve only selected fields", "[Physics][Material][Override]")
{
    PhysicsMaterialData baseMaterial;
    baseMaterial.Density = 2.0f;
    baseMaterial.Friction = 0.25f;
    baseMaterial.Restitution = 0.4f;
    baseMaterial.RestitutionThreshold = 1.25f;
    baseMaterial.FrictionCombine = PhysicsCombineMode::Minimum;
    baseMaterial.RestitutionCombine = PhysicsCombineMode::Average;

    PhysicsMaterialOverride materialOverride;
    materialOverride.Fields = PhysicsMaterialOverrideBits::Friction | PhysicsMaterialOverrideBits::RestitutionCombine;
    materialOverride.Values.Friction = 0.9f;
    materialOverride.Values.RestitutionCombine = PhysicsCombineMode::Multiply;
    materialOverride.Values.Density = -8.0f;

    const PhysicsMaterialData resolved = ResolvePhysicsMaterialData(baseMaterial, materialOverride);
    CHECK(resolved.Density == 2.0f);
    CHECK(resolved.Friction == 0.9f);
    CHECK(resolved.Restitution == 0.4f);
    CHECK(resolved.RestitutionThreshold == 1.25f);
    CHECK(resolved.FrictionCombine == PhysicsCombineMode::Minimum);
    CHECK(resolved.RestitutionCombine == PhysicsCombineMode::Multiply);

    materialOverride.Fields = PhysicsMaterialOverrideFlags(static_cast<uint32_t>(PhysicsMaterialOverrideBits::All) | (1u << 31u));
    const PhysicsMaterialOverride normalized = NormalizePhysicsMaterialOverride(materialOverride);
    CHECK(static_cast<uint32_t>(normalized.Fields) == static_cast<uint32_t>(PhysicsMaterialOverrideBits::All));
    CHECK(normalized.Values.Density == 0.0f);
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

TEST_CASE("3D contact batches normalize independent of backend callback order", "[Physics][Physics3D][Contacts]")
{
    PhysicsContactEvent3D reversedEnter;
    reversedEnter.Type = PhysicsContactEventType3D::Enter;
    reversedEnter.BodyA = { 90 };
    reversedEnter.BodyB = { 30 };
    reversedEnter.ShapeA = { 9 };
    reversedEnter.ShapeB = { 3 };
    reversedEnter.ShapeUserDataA = 900;
    reversedEnter.ShapeUserDataB = 300;
    reversedEnter.MaterialA.Friction = 0.9f;
    reversedEnter.MaterialB.Friction = 0.3f;
    reversedEnter.Points.push_back({ { 2.0f, 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f }, -0.2f, 2.0f });

    PhysicsContactEvent3D duplicateEnter = reversedEnter;
    std::swap(duplicateEnter.BodyA, duplicateEnter.BodyB);
    std::swap(duplicateEnter.ShapeA, duplicateEnter.ShapeB);
    std::swap(duplicateEnter.ShapeUserDataA, duplicateEnter.ShapeUserDataB);
    std::swap(duplicateEnter.MaterialA, duplicateEnter.MaterialB);
    duplicateEnter.Points[0].Normal = -duplicateEnter.Points[0].Normal;
    duplicateEnter.Points.push_back({ { 1.0f, 0.0f, 0.0f }, { -1.0f, 0.0f, 0.0f }, -0.1f, 1.0f });

    PhysicsContactEvent3D sameStepStay = duplicateEnter;
    sameStepStay.Type = PhysicsContactEventType3D::Stay;

    PhysicsContactEvent3D endingStay;
    endingStay.Type = PhysicsContactEventType3D::Stay;
    endingStay.BodyA = { 20 };
    endingStay.BodyB = { 80 };
    endingStay.ShapeA = { 2 };
    endingStay.ShapeB = { 8 };

    PhysicsContactEvent3D endingExit = endingStay;
    endingExit.Type = PhysicsContactEventType3D::Exit;

    Vector<PhysicsContactEvent3D> first = { sameStepStay, endingExit, reversedEnter, endingStay, duplicateEnter };
    Vector<PhysicsContactEvent3D> second(first.rbegin(), first.rend());
    NormalizePhysicsContactEvents3D(first);
    NormalizePhysicsContactEvents3D(second);

    REQUIRE(first.size() == 2);
    REQUIRE(second.size() == first.size());
    CHECK(first[0].ShapeA.Value == 2);
    CHECK(first[0].ShapeB.Value == 8);
    CHECK(first[0].Type == PhysicsContactEventType3D::Exit);
    CHECK(first[1].ShapeA.Value == 3);
    CHECK(first[1].ShapeB.Value == 9);
    CHECK(first[1].BodyA.Value == 30);
    CHECK(first[1].BodyB.Value == 90);
    CHECK(first[1].ShapeUserDataA == 300);
    CHECK(first[1].ShapeUserDataB == 900);
    CHECK(first[1].MaterialA.Friction == 0.3f);
    CHECK(first[1].MaterialB.Friction == 0.9f);
    CHECK(first[1].Type == PhysicsContactEventType3D::Enter);
    REQUIRE(first[1].Points.size() == 2);
    CHECK(first[1].Points[0].Point == glm::vec3(1.0f, 0.0f, 0.0f));
    CHECK(first[1].Points[1].Point == glm::vec3(2.0f, 0.0f, 0.0f));
    CHECK(first[1].Points[0].Normal == glm::vec3(-1.0f, 0.0f, 0.0f));
    CHECK(first[1].Points[1].Normal == glm::vec3(-1.0f, 0.0f, 0.0f));

    for (size_t index = 0; index < first.size(); ++index)
    {
        CHECK(second[index].Type == first[index].Type);
        CHECK(second[index].BodyA == first[index].BodyA);
        CHECK(second[index].BodyB == first[index].BodyB);
        CHECK(second[index].ShapeA == first[index].ShapeA);
        CHECK(second[index].ShapeB == first[index].ShapeB);
        CHECK(second[index].ShapeUserDataA == first[index].ShapeUserDataA);
        CHECK(second[index].ShapeUserDataB == first[index].ShapeUserDataB);
        REQUIRE(second[index].Points.size() == first[index].Points.size());
        for (uint32_t point = 0; point < first[index].Points.size(); ++point)
        {
            CHECK(second[index].Points[point].Point == first[index].Points[point].Point);
            CHECK(second[index].Points[point].Normal == first[index].Points[point].Normal);
            CHECK(second[index].Points[point].Separation == first[index].Points[point].Separation);
            CHECK(second[index].Points[point].NormalImpulse == first[index].Points[point].NormalImpulse);
        }
    }
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
            for (size_t index = 0; index < contacts.size(); ++index)
            {
                const PhysicsContactEvent3D& event = contacts[index];
                CHECK(std::tuple(event.ShapeA.Value, event.BodyA.Value) <= std::tuple(event.ShapeB.Value, event.BodyB.Value));
                if (event.ShapeA == staticShape)
                {
                    CHECK(event.ShapeUserDataA == 11);
                    CHECK(event.MaterialA.Friction == 0.25f);
                }
                else if (event.ShapeA == dynamicShape)
                {
                    CHECK(event.ShapeUserDataA == 21);
                    CHECK(event.MaterialA.Friction == 0.5f);
                }
                if (event.ShapeB == staticShape)
                {
                    CHECK(event.ShapeUserDataB == 11);
                    CHECK(event.MaterialB.Friction == 0.25f);
                }
                else if (event.ShapeB == dynamicShape)
                {
                    CHECK(event.ShapeUserDataB == 21);
                    CHECK(event.MaterialB.Friction == 0.5f);
                }
                if (index != 0)
                {
                    const PhysicsContactEvent3D& previous = contacts[index - 1];
                    CHECK(std::tuple(previous.ShapeA.Value, previous.BodyA.Value, previous.ShapeB.Value, previous.BodyB.Value, previous.Type) <=
                          std::tuple(event.ShapeA.Value, event.BodyA.Value, event.ShapeB.Value, event.BodyB.Value, event.Type));
                }
                if (event.Type != PhysicsContactEventType3D::Stay)
                {
                    for (const PhysicsContactEvent3D& candidate : contacts)
                    {
                        if (candidate.ShapeA == event.ShapeA && candidate.ShapeB == event.ShapeB)
                            CHECK(candidate.Type != PhysicsContactEventType3D::Stay);
                    }
                }
            }

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
            rigidbody.SetAwake(true);

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
            CHECK(replacedBody.IsAwake());

            replacedBody.SetAwake(false);
            const glm::vec3 sleepingLinearVelocity = replacedBody.GetLinearVelocity();
            const glm::vec3 sleepingAngularVelocity = replacedBody.GetAngularVelocity();
            Rigidbody3DComponent sleepingSettings = replacedBody;
            sleepingSettings.SetDamping(0.2f, 0.3f);
            auto& sleepingBody = body.AddOrReplaceComponent<Rigidbody3DComponent>(sleepingSettings);
            CHECK(&sleepingBody == bodyAddress);
            CHECK(sleepingBody.InstanceId == bodyInstanceId);
            REQUIRE(sleepingBody.RuntimeBody);
            CHECK(sleepingBody.GetLinearVelocity() == sleepingLinearVelocity);
            CHECK(sleepingBody.GetAngularVelocity() == sleepingAngularVelocity);
            CHECK_FALSE(sleepingBody.IsAwake());
            CHECK(sleepingBody.GetLinearDamping() == 0.2f);
            CHECK(sleepingBody.GetAngularDamping() == 0.3f);

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
