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

    Entity duplicate = scene->DuplicateEntity(source, false);
    REQUIRE(duplicate);
    const auto& copiedBody = duplicate.GetComponent<Rigidbody2DComponent>();
    const auto& copiedCollider = duplicate.GetComponent<BoxCollider2DComponent>();
    CHECK(copiedBody.RuntimeBody == nullptr);
    CHECK(copiedCollider.RuntimeFixture == nullptr);
    CHECK(copiedBody.GetBodyType() == RigidbodyBodyType::Dynamic);
    CHECK(copiedBody.GetConfiguredMass() == 3.0f);
    CHECK(copiedCollider.GetSize() == glm::vec2(2.0f, 4.0f));
    CHECK(copiedCollider.IsTrigger());
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
    const uint64_t bodyInstanceId = rigidbody.InstanceId;
    const uint64_t colliderInstanceId = collider.InstanceId;

    Entity duplicate = scene->DuplicateEntity(source, false);
    REQUIRE(duplicate);
    const auto& copiedBody = duplicate.GetComponent<Rigidbody3DComponent>();
    const auto& copiedCollider = duplicate.GetComponent<CapsuleCollider3DComponent>();
    CHECK_FALSE(static_cast<bool>(copiedBody.RuntimeBody));
    CHECK_FALSE(static_cast<bool>(copiedCollider.RuntimeShape));
    CHECK(copiedBody.InstanceId != bodyInstanceId);
    CHECK(copiedCollider.InstanceId != colliderInstanceId);
    CHECK(copiedBody.GetBodyType() == PhysicsBodyType3D::Dynamic);
    CHECK(copiedBody.GetMass() == 4.0f);
    CHECK(copiedBody.GetGravityScale() == 0.5f);
    CHECK(copiedCollider.GetRadius() == 0.75f);
    CHECK(copiedCollider.GetHeight() == 3.0f);
    CHECK(copiedCollider.IsTrigger());
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
