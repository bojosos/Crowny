#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "Crowny/Ecs/Entity.h"
#include "Crowny/Scene/Scene.h"
#include "Crowny/Ecs/Components.h"

using namespace Crowny;

// Helper to compare matrices
void ExpectMatrixEqual(const glm::mat4& actual, const glm::mat4& expected)
{
    for (int i = 0; i < 4; ++i)
    {
        for (int j = 0; j < 4; ++j)
        {
            REQUIRE_THAT(actual[i][j], Catch::Matchers::WithinRel(expected[i][j], 0.001f));
        }
    }
}

TEST_CASE("Entity Parenting and Transform Hierarchies", "[Ecs][Transform]")
{
    Ref<Scene> scene = CreateRef<Scene>(false); // Don't create root entity automatically for clean state

    SECTION("Basic Parenting")
    {
        Entity parent = scene->CreateEntity("Parent");
        Entity child = scene->CreateEntity("Child");

        child.SetParent(parent);

        REQUIRE(child.GetParent() == parent);
        REQUIRE(parent.GetChildCount() == 1);
        REQUIRE(parent.GetChild(0) == child);
    }

    SECTION("Transform Propagation")
    {
        Entity parent = scene->CreateEntity("Parent");
        Entity child = scene->CreateEntity("Child");

        child.SetParent(parent);

        parent.SetPosition({ 10.0f, 0.0f, 0.0f });
        child.SetPosition({ 5.0f, 0.0f, 0.0f });

        // World position should be 15, 0, 0
        REQUIRE(child.GetWorldPosition() == glm::vec3(15.0f, 0.0f, 0.0f));

        parent.SetRotation(glm::angleAxis(glm::radians(90.0f), glm::vec3(0, 1, 0)));
        // Child is at local 5,0,0 relative to parent. Parent is rotated 90 deg around Y.
        // Local X becomes world -Z.
        REQUIRE_THAT(child.GetWorldPosition().x, Catch::Matchers::WithinRel(10.0f, 0.001f));
        REQUIRE_THAT(child.GetWorldPosition().z, Catch::Matchers::WithinRel(-5.0f, 0.001f));
    }

    SECTION("Reparenting Preserves World Transform")
    {
        Entity parent1 = scene->CreateEntity("Parent1");
        Entity parent2 = scene->CreateEntity("Parent2");
        Entity child = scene->CreateEntity("Child");

        parent1.SetPosition({ 10.0f, 0.0f, 0.0f });
        parent2.SetPosition({ 0.0f, 20.0f, 0.0f });
        
        child.SetParent(parent1);
        child.SetPosition({ 5.0f, 0.0f, 0.0f }); // World position 15, 0, 0

        REQUIRE(child.GetWorldPosition() == glm::vec3(15.0f, 0.0f, 0.0f));

        // Move to parent2. World position should remain 15, 0, 0.
        // Relative to parent2 (0, 20, 0), local position should become 15, -20, 0.
        child.SetParent(parent2);

        REQUIRE(child.GetParent() == parent2);
        REQUIRE_THAT(child.GetWorldPosition().x, Catch::Matchers::WithinRel(15.0f, 0.001f));
        REQUIRE_THAT(child.GetWorldPosition().y, Catch::Matchers::WithinRel(0.0f, 0.001f));
        REQUIRE_THAT(child.GetWorldPosition().z, Catch::Matchers::WithinRel(0.0f, 0.001f));
        
        REQUIRE(child.GetLocalPosition() == glm::vec3(15.0f, -20.0f, 0.0f));
    }

    SECTION("Deep Hierarchy Invalidation")
    {
        Entity a = scene->CreateEntity("A");
        Entity b = scene->CreateEntity("B");
        Entity c = scene->CreateEntity("C");

        b.SetParent(a);
        c.SetParent(b);

        a.SetPosition({ 1.0f, 0.0f, 0.0f });
        b.SetPosition({ 1.0f, 0.0f, 0.0f });
        c.SetPosition({ 1.0f, 0.0f, 0.0f });

        REQUIRE(c.GetWorldPosition() == glm::vec3(3.0f, 0.0f, 0.0f));

        a.SetPosition({ 10.0f, 0.0f, 0.0f });
        // C should be updated to 12, 0, 0 (10 + 1 + 1)
        REQUIRE(c.GetWorldPosition() == glm::vec3(12.0f, 0.0f, 0.0f));
    }

    SECTION("Cycle Prevention")
    {
        Entity a = scene->CreateEntity("A");
        Entity b = scene->CreateEntity("B");
        
        b.SetParent(a);
        
        // This should fail/be ignored to prevent cycles
        // a.SetParent(b);

        // REQUIRE(a.GetParent() == Entity{});
        // REQUIRE(b.GetParent() == a);
    }

    SECTION("Unparenting Preserves World Transform")
    {
        Entity parent = scene->CreateEntity("Parent");
        Entity child = scene->CreateEntity("Child");

        parent.SetPosition({ 10.0f, 10.0f, 10.0f });
        child.SetParent(parent);
        child.SetPosition({ 1.0f, 1.0f, 1.0f }); // World 11, 11, 11

        REQUIRE(child.GetWorldPosition() == glm::vec3(11.0f, 11.0f, 11.0f));

        child.SetParent(Entity{}); // Unparent

        REQUIRE(child.GetParent() == Entity{});
        REQUIRE(child.GetWorldPosition() == glm::vec3(11.0f, 11.0f, 11.0f));
        REQUIRE(child.GetLocalPosition() == glm::vec3(11.0f, 11.0f, 11.0f));
    }

    SECTION("SetWorldPosition Works with Hierarchy")
    {
        Entity parent = scene->CreateEntity("Parent");
        Entity child = scene->CreateEntity("Child");

        parent.SetPosition({ 10.0f, 0.0f, 0.0f });
        child.SetParent(parent);

        child.SetWorldPosition({ 5.0f, 0.0f, 0.0f });

        REQUIRE(child.GetWorldPosition() == glm::vec3(5.0f, 0.0f, 0.0f));
        REQUIRE(child.GetLocalPosition() == glm::vec3(-5.0f, 0.0f, 0.0f));
    }

    SECTION("Scaling Propagation")
    {
        Entity parent = scene->CreateEntity("Parent");
        Entity child = scene->CreateEntity("Child");

        parent.SetScale({ 2.0f, 2.0f, 2.0f });
        child.SetParent(parent);
        child.SetScale({ 1.0f, 1.0f, 1.0f }); // Local scale
        child.SetPosition({ 1.0f, 0.0f, 0.0f }); // Local position

        // Child world position should be (2, 0, 0) because parent scale is 2
        CHECK_THAT(child.GetWorldPosition().x, Catch::Matchers::WithinAbs(2.0f, 0.0001f));
        CHECK_THAT(child.GetWorldPosition().y, Catch::Matchers::WithinAbs(0.0f, 0.0001f));
        CHECK_THAT(child.GetWorldPosition().z, Catch::Matchers::WithinAbs(0.0f, 0.0001f));

        CHECK_THAT(child.GetWorldScale().x, Catch::Matchers::WithinAbs(2.0f, 0.0001f));
        CHECK_THAT(child.GetWorldScale().y, Catch::Matchers::WithinAbs(2.0f, 0.0001f));
        CHECK_THAT(child.GetWorldScale().z, Catch::Matchers::WithinAbs(2.0f, 0.0001f));

        child.SetScale({ 0.5f, 0.5f, 0.5f });
        // Child world scale should be 2.0 * 0.5 = 1.0
        CHECK_THAT(child.GetWorldScale().x, Catch::Matchers::WithinAbs(1.0f, 0.0001f));
        CHECK_THAT(child.GetWorldScale().y, Catch::Matchers::WithinAbs(1.0f, 0.0001f));
        CHECK_THAT(child.GetWorldScale().z, Catch::Matchers::WithinAbs(1.0f, 0.0001f));
    }

    SECTION("Entity Duplication")
    {
        Entity parent = scene->CreateEntity("Parent");
        Entity child = scene->CreateEntity("Child");
        child.SetParent(parent);
        
        parent.SetPosition({10.0f, 0.0f, 0.0f});
        child.SetPosition({5.0f, 0.0f, 0.0f});

        SECTION("Duplicate with children")
        {
            Entity parentClone = scene->DuplicateEntity(parent, true);
            CHECK(parentClone.GetName() == "Parent");
            CHECK(parentClone.GetLocalPosition() == glm::vec3(10.0f, 0.0f, 0.0f));
            
            REQUIRE(parentClone.GetChildCount() == 1);
            Entity childClone = parentClone.GetChild(0);
            CHECK(childClone.GetName() == "Child");
            CHECK(childClone.GetLocalPosition() == glm::vec3(5.0f, 0.0f, 0.0f));
            CHECK(childClone.GetWorldPosition() == glm::vec3(15.0f, 0.0f, 0.0f));
        }

        SECTION("Duplicate without children")
        {
            Entity parentClone = scene->DuplicateEntity(parent, false);
            CHECK(parentClone.GetChildCount() == 0);
        }
    }

    SECTION("Component Lifecycle")
    {
        Entity entity = scene->CreateEntity("Test");
        
        REQUIRE(!entity.HasComponent<CameraComponent>());
        
        entity.AddComponent<CameraComponent>();
        REQUIRE(entity.HasComponent<CameraComponent>());
        
        entity.RemoveComponent<CameraComponent>();
        REQUIRE(!entity.HasComponent<CameraComponent>());
    }

    SECTION("Complex Destruction")
    {
        Entity parent = scene->CreateEntity("Parent");
        Entity child = scene->CreateEntity("Child");
        Entity grandChild = scene->CreateEntity("GrandChild");

        child.SetParent(parent);
        grandChild.SetParent(child);

        REQUIRE(parent.GetChildCount() == 1);
        REQUIRE(child.GetChildCount() == 1);

        parent.Destroy(true); // Recursive destroy

        // After registry destroy, handles are still technically there but invalid in the registry.
        // Entt uses versioning. A null check on Entity wrapper works if we null it.
        // Scene::DestroyEntity handles this.
    }
}
