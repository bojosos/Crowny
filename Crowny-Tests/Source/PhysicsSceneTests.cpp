#include <catch2/catch_test_macros.hpp>

#include "Crowny/Ecs/Components.h"
#include "Crowny/Ecs/Entity.h"
#include "Crowny/Scene/Scene.h"

using namespace Crowny;

TEST_CASE("2D physics components reset runtime pointers when copied", "[Physics][Physics2D][Ecs]")
{
    Ref<Scene> scene = CreateRef<Scene>(false);
    Entity source = scene->CreateEntity("Physics source");

    auto& rigidbody = source.AddComponent<Rigidbody2DComponent>();
    rigidbody.SetBodyType(RigidbodyBodyType::Dynamic);
    rigidbody.SetMass(3.0f);
    rigidbody.RuntimeBody = reinterpret_cast<void*>(0x1);

    auto& collider = source.AddComponent<BoxCollider2DComponent>();
    collider.SetSize({ 2.0f, 4.0f }, source);
    collider.SetIsTrigger(true);
    collider.RuntimeFixture = reinterpret_cast<void*>(0x2);

    auto& circle = source.AddComponent<CircleCollider2DComponent>();
    circle.SetRadius(1.25f, source);
    circle.SetOffset({ 0.25f, 0.5f }, source);
    circle.RuntimeFixture = reinterpret_cast<void*>(0x3);

    Entity duplicate = scene->DuplicateEntity(source, false);
    REQUIRE(duplicate);
    const auto& copiedBody = duplicate.GetComponent<Rigidbody2DComponent>();
    const auto& copiedCollider = duplicate.GetComponent<BoxCollider2DComponent>();
    const auto& copiedCircle = duplicate.GetComponent<CircleCollider2DComponent>();
    CHECK(copiedBody.RuntimeBody == nullptr);
    CHECK(copiedCollider.RuntimeFixture == nullptr);
    CHECK(copiedCircle.RuntimeFixture == nullptr);
    CHECK(copiedBody.GetBodyType() == RigidbodyBodyType::Dynamic);
    CHECK(copiedBody.GetConfiguredMass() == 3.0f);
    CHECK(copiedCollider.GetSize() == glm::vec2(2.0f, 4.0f));
    CHECK(copiedCollider.IsTrigger());
    CHECK(copiedCircle.GetRadius() == 1.25f);
    CHECK(copiedCircle.GetOffset() == glm::vec2(0.25f, 0.5f));
}

TEST_CASE("2D physics assignment preserves live runtime state", "[Physics][Physics2D][Ecs][Lifecycle]")
{
    Rigidbody2DComponent sourceBody;
    sourceBody.SetBodyType(RigidbodyBodyType::Dynamic);
    sourceBody.SetMass(6.0f);
    sourceBody.SetGravityScale(0.25f);

    Rigidbody2DComponent destinationBody;
    destinationBody.RuntimeBody = reinterpret_cast<void*>(0x11);
    destinationBody.RuntimePreviousPosition = { 2.0f, 3.0f };
    destinationBody.RuntimePreviousRotation = 0.75f;
    destinationBody.RuntimeHasPreviousState = true;
    const uint64_t bodyInstanceId = destinationBody.InstanceId;
    destinationBody = sourceBody;

    CHECK(destinationBody.InstanceId == bodyInstanceId);
    CHECK(destinationBody.RuntimeBody == reinterpret_cast<void*>(0x11));
    CHECK(destinationBody.RuntimePreviousPosition == glm::vec2(2.0f, 3.0f));
    CHECK(destinationBody.RuntimePreviousRotation == 0.75f);
    CHECK(destinationBody.RuntimeHasPreviousState);
    CHECK(destinationBody.GetBodyType() == RigidbodyBodyType::Dynamic);
    CHECK(destinationBody.GetConfiguredMass() == 6.0f);
    CHECK(destinationBody.GetGravityScale() == 0.25f);

    BoxCollider2DComponent sourceCollider;
    sourceCollider.SetSize({ 3.0f, 5.0f }, {});
    sourceCollider.SetOffset({ 1.0f, 2.0f }, {});
    sourceCollider.SetIsTrigger(true);

    BoxCollider2DComponent destinationCollider;
    destinationCollider.RuntimeFixture = reinterpret_cast<void*>(0x22);
    const uint64_t colliderInstanceId = destinationCollider.InstanceId;
    destinationCollider = sourceCollider;

    CHECK(destinationCollider.InstanceId == colliderInstanceId);
    CHECK(destinationCollider.RuntimeFixture == reinterpret_cast<void*>(0x22));
    CHECK(destinationCollider.GetSize() == glm::vec2(3.0f, 5.0f));
    CHECK(destinationCollider.GetOffset() == glm::vec2(1.0f, 2.0f));
    CHECK(destinationCollider.IsTrigger());
}

TEST_CASE("3D physics components reset runtime handles when copied", "[Physics][Physics3D][Ecs]")
{
    Ref<Scene> scene = CreateRef<Scene>(false);
    Entity source = scene->CreateEntity("Physics source");

    auto& rigidbody = source.AddComponent<Rigidbody3DComponent>();
    rigidbody.SetBodyType(PhysicsBodyType3D::Dynamic, source);
    rigidbody.SetMass(4.0f, source);
    rigidbody.SetAutoMass(false, source);
    rigidbody.SetGravityScale(0.5f);
    rigidbody.RuntimeBody = { 17 };

    auto& collider = source.AddComponent<CapsuleCollider3DComponent>();
    collider.SetRadius(0.75f, source);
    collider.SetHeight(3.0f, source);
    collider.SetIsTrigger(true);
    collider.RuntimeShape = { 23 };
    auto& box = source.AddComponent<BoxCollider3DComponent>();
    box.SetSize({ 2.0f, 3.0f, 4.0f }, source);
    box.RuntimeShape = { 24 };
    auto& sphere = source.AddComponent<SphereCollider3DComponent>();
    sphere.SetRadius(1.5f, source);
    sphere.RuntimeShape = { 25 };
    const uint64_t bodyInstanceId = rigidbody.InstanceId;
    const uint64_t colliderInstanceId = collider.InstanceId;

    Entity duplicate = scene->DuplicateEntity(source, false);
    REQUIRE(duplicate);
    const auto& copiedBody = duplicate.GetComponent<Rigidbody3DComponent>();
    const auto& copiedCollider = duplicate.GetComponent<CapsuleCollider3DComponent>();
    const auto& copiedBox = duplicate.GetComponent<BoxCollider3DComponent>();
    const auto& copiedSphere = duplicate.GetComponent<SphereCollider3DComponent>();
    CHECK_FALSE(static_cast<bool>(copiedBody.RuntimeBody));
    CHECK_FALSE(static_cast<bool>(copiedCollider.RuntimeShape));
    CHECK_FALSE(static_cast<bool>(copiedBox.RuntimeShape));
    CHECK_FALSE(static_cast<bool>(copiedSphere.RuntimeShape));
    CHECK(copiedBody.InstanceId != bodyInstanceId);
    CHECK(copiedCollider.InstanceId != colliderInstanceId);
    CHECK(copiedBody.GetBodyType() == PhysicsBodyType3D::Dynamic);
    CHECK(copiedBody.GetMass() == 4.0f);
    CHECK(copiedBody.GetGravityScale() == 0.5f);
    CHECK(copiedCollider.GetRadius() == 0.75f);
    CHECK(copiedCollider.GetHeight() == 3.0f);
    CHECK(copiedCollider.IsTrigger());
    CHECK(copiedBox.GetSize() == glm::vec3(2.0f, 3.0f, 4.0f));
    CHECK(copiedSphere.GetRadius() == 1.5f);
}

TEST_CASE("3D physics assignment preserves live runtime state", "[Physics][Physics3D][Ecs][Lifecycle]")
{
    Rigidbody3DComponent sourceBody;
    sourceBody.SetBodyType(PhysicsBodyType3D::Dynamic, {});
    sourceBody.SetMass(8.0f, {});
    sourceBody.SetAutoMass(false, {});
    sourceBody.SetGravityScale(0.4f);
    sourceBody.SetLinearVelocity({ 1.0f, 2.0f, 3.0f });

    Rigidbody3DComponent destinationBody;
    destinationBody.RuntimeBody = { 31 };
    const uint64_t bodyInstanceId = destinationBody.InstanceId;
    destinationBody = sourceBody;

    CHECK(destinationBody.InstanceId == bodyInstanceId);
    CHECK(destinationBody.RuntimeBody == PhysicsBody3DHandle{ 31 });
    CHECK(destinationBody.GetBodyType() == PhysicsBodyType3D::Dynamic);
    CHECK(destinationBody.GetMass() == 8.0f);
    CHECK_FALSE(destinationBody.GetAutoMass());
    CHECK(destinationBody.GetGravityScale() == 0.4f);

    CapsuleCollider3DComponent sourceCollider;
    sourceCollider.SetRadius(0.8f, {});
    sourceCollider.SetHeight(4.0f, {});
    sourceCollider.SetOffset({ 1.0f, 2.0f, 3.0f }, {});
    sourceCollider.SetIsTrigger(true);

    CapsuleCollider3DComponent destinationCollider;
    destinationCollider.RuntimeShape = { 41 };
    const uint64_t colliderInstanceId = destinationCollider.InstanceId;
    destinationCollider = sourceCollider;

    CHECK(destinationCollider.InstanceId == colliderInstanceId);
    CHECK(destinationCollider.RuntimeShape == PhysicsShape3DHandle{ 41 });
    CHECK(destinationCollider.GetRadius() == 0.8f);
    CHECK(destinationCollider.GetHeight() == 4.0f);
    CHECK(destinationCollider.GetOffset() == glm::vec3(1.0f, 2.0f, 3.0f));
    CHECK(destinationCollider.IsTrigger());
}

TEST_CASE("3D collider and rigidbody components can be added and removed without a running world", "[Physics][Physics3D][Ecs]")
{
    Ref<Scene> scene = CreateRef<Scene>(false);
    Entity entity = scene->CreateEntity("Lifecycle");

    entity.AddComponent<Rigidbody3DComponent>();
    entity.AddComponent<BoxCollider3DComponent>();
    entity.AddComponent<SphereCollider3DComponent>();
    entity.AddComponent<CapsuleCollider3DComponent>();
    CHECK(entity.HasComponent<Rigidbody3DComponent>());
    CHECK(entity.HasComponent<BoxCollider3DComponent>());
    CHECK(entity.HasComponent<SphereCollider3DComponent>());
    CHECK(entity.HasComponent<CapsuleCollider3DComponent>());

    entity.RemoveComponent<CapsuleCollider3DComponent>();
    entity.RemoveComponent<SphereCollider3DComponent>();
    entity.RemoveComponent<BoxCollider3DComponent>();
    entity.RemoveComponent<Rigidbody3DComponent>();
    CHECK_FALSE(entity.HasComponent<Rigidbody3DComponent>());
    CHECK_FALSE(entity.HasComponent<BoxCollider3DComponent>());
    CHECK_FALSE(entity.HasComponent<SphereCollider3DComponent>());
    CHECK_FALSE(entity.HasComponent<CapsuleCollider3DComponent>());
}
