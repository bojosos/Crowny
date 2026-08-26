#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "Crowny/Ecs/Components.h"
#include "Crowny/Ecs/Entity.h"
#include "Crowny/Scene/Prefab.h"
#include "Crowny/Scene/Scene.h"

using namespace Crowny;
using namespace Crowny::Literals;

TEST_CASE("Prefab override paths support allocation-free lookup", "[Ecs][Prefab]")
{
    PrefabComponent prefab;
    prefab.MarkOverridden("Transform.Position");
    prefab.MarkOverridden("Audio Source.PlayOnAwake");

    CHECK(prefab.IsPropertyOverridden("Transform.Position"_hstr));
    CHECK(prefab.IsPropertyOverridden("Audio Source", "PlayOnAwake"));
    CHECK_FALSE(prefab.IsPropertyOverridden("Audio Source", "Volume"));

    prefab.ClearOverride("Transform.Position");
    CHECK_FALSE(prefab.IsPropertyOverridden("Transform.Position"_hstr));
}

TEST_CASE("Prefab capture preserves its internal hierarchy", "[Ecs][Prefab]")
{
    Ref<Scene> source = CreateRef<Scene>(false);
    Entity sourceRoot = source->CreateEntity("Prefab root");
    Entity sourceChild = source->CreateEntity("Prefab child");
    sourceChild.SetParent(sourceRoot);

    Prefab prefab;
    prefab.CaptureFromEntity(*source, sourceRoot);

    Entity prefabRoot = prefab.GetRootEntity();
    Entity internalRoot = prefab.GetInternalScene()->GetRootEntity();
    REQUIRE(prefabRoot);
    REQUIRE(internalRoot);
    CHECK(prefabRoot.GetParent() == internalRoot);
    CHECK(internalRoot.GetChildCount() == 1);
    CHECK(internalRoot.GetChild(0) == prefabRoot);
    REQUIRE(prefabRoot.GetChildCount() == 1);
    CHECK(prefabRoot.GetChild(0).GetName() == "Prefab child");
    CHECK(prefabRoot.GetChild(0).GetSiblingIndex() == 0);
}

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

        CHECK_FALSE(a.SetParent(a));
        CHECK_FALSE(a.SetParent(b));
        REQUIRE_FALSE(a.GetParent());
        REQUIRE(b.GetParent() == a);
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
        child.SetScale({ 1.0f, 1.0f, 1.0f });    // Local scale
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

        parent.SetPosition({ 10.0f, 0.0f, 0.0f });
        child.SetPosition({ 5.0f, 0.0f, 0.0f });

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

    SECTION("Component pack queries")
    {
        Entity entity = scene->CreateEntity("Pack query");

        CHECK(entity.HasComponents<IDComponent, TagComponent, TransformComponent>());
        CHECK(entity.HasAnyComponents<CameraComponent, TransformComponent>());
        CHECK_FALSE(entity.HasAnyComponents<CameraComponent, AudioListenerComponent>());
    }

    SECTION("Transform invalidation remains lazy")
    {
        Entity parent = scene->CreateEntity("Parent");
        Entity child = scene->CreateEntity("Child");
        Entity grandChild = scene->CreateEntity("Grand child");
        child.SetParent(parent);
        grandChild.SetParent(child);

        grandChild.GetWorldMatrix();
        REQUIRE(child.GetTransform().IsCachedWorldTransformValid());
        REQUIRE(grandChild.GetTransform().IsCachedWorldTransformValid());

        parent.SetPosition({ 2.0f, 3.0f, 4.0f });
        CHECK_FALSE(child.GetTransform().IsCachedWorldTransformValid());
        CHECK_FALSE(grandChild.GetTransform().IsCachedWorldTransformValid());

        grandChild.GetWorldMatrix();
        CHECK(child.GetTransform().IsCachedWorldTransformValid());
        CHECK(grandChild.GetTransform().IsCachedWorldTransformValid());
    }

    SECTION("Scene copies own their hierarchy")
    {
        Entity parent = scene->CreateEntity("Parent");
        Entity child = scene->CreateEntity("Child");
        Entity sibling = scene->CreateEntity("Sibling");
        child.SetParent(parent);
        sibling.SetParent(parent);
        parent.SetPosition({ 10.0f, 0.0f, 0.0f });
        child.SetPosition({ 5.0f, 0.0f, 0.0f });
        const UUID parentId = parent.GetUuid();
        const UUID childId = child.GetUuid();
        const UUID siblingId = sibling.GetUuid();

        Scene copy(*scene);
        Entity copiedParent = copy.GetEntityFromUuid(parentId);
        Entity copiedChild = copy.GetEntityFromUuid(childId);
        Entity copiedSibling = copy.GetEntityFromUuid(siblingId);

        REQUIRE(copiedParent.GetScene() == &copy);
        REQUIRE(copiedChild.GetScene() == &copy);
        CHECK(copiedChild.GetParent() == copiedParent);
        CHECK(copiedParent.GetChild(0) == copiedChild);
        CHECK(copiedParent.GetChild(1) == copiedSibling);
        CHECK(copiedChild.GetSiblingIndex() == 0);
        CHECK(copiedSibling.GetSiblingIndex() == 1);
        CHECK(copiedChild.GetLocalPosition() == glm::vec3(5.0f, 0.0f, 0.0f));
        CHECK(copiedChild.GetWorldPosition() == glm::vec3(15.0f, 0.0f, 0.0f));
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
        CHECK_FALSE(parent.IsValid());
        CHECK_FALSE(child.IsValid());
        CHECK_FALSE(grandChild.IsValid());
    }

    SECTION("Non-recursive destruction reparents children in sibling order")
    {
        Entity parent = scene->CreateEntity("Parent");
        Entity before = scene->CreateEntity("Before");
        Entity removed = scene->CreateEntity("Removed");
        Entity after = scene->CreateEntity("After");
        Entity firstChild = scene->CreateEntity("First child");
        Entity secondChild = scene->CreateEntity("Second child");
        before.SetParent(parent);
        removed.SetParent(parent);
        after.SetParent(parent);
        firstChild.SetParent(removed);
        secondChild.SetParent(removed);
        firstChild.SetPosition({ 1.0f, 2.0f, 3.0f });
        secondChild.SetPosition({ 4.0f, 5.0f, 6.0f });
        const glm::mat4 firstWorld = firstChild.GetWorldMatrix();
        const glm::mat4 secondWorld = secondChild.GetWorldMatrix();

        removed.Destroy(false);

        REQUIRE(parent.GetChildCount() == 4);
        CHECK(parent.GetChild(0) == before);
        CHECK(parent.GetChild(1) == firstChild);
        CHECK(parent.GetChild(2) == secondChild);
        CHECK(parent.GetChild(3) == after);
        CHECK(before.GetSiblingIndex() == 0);
        CHECK(firstChild.GetSiblingIndex() == 1);
        CHECK(secondChild.GetSiblingIndex() == 2);
        CHECK(after.GetSiblingIndex() == 3);
        ExpectMatrixEqual(firstChild.GetWorldMatrix(), firstWorld);
        ExpectMatrixEqual(secondChild.GetWorldMatrix(), secondWorld);
    }

    SECTION("Sibling indices stay coherent across reorder and reparent")
    {
        Entity firstParent = scene->CreateEntity("First parent");
        Entity secondParent = scene->CreateEntity("Second parent");
        Entity first = scene->CreateEntity("First");
        Entity second = scene->CreateEntity("Second");
        Entity third = scene->CreateEntity("Third");
        first.SetParent(firstParent);
        second.SetParent(firstParent);
        third.SetParent(firstParent);

        REQUIRE(third.SetSiblingIndex(0));
        CHECK(firstParent.GetChild(0) == third);
        CHECK(firstParent.GetChild(1) == first);
        CHECK(firstParent.GetChild(2) == second);
        CHECK(third.GetSiblingIndex() == 0);
        CHECK(first.GetSiblingIndex() == 1);
        CHECK(second.GetSiblingIndex() == 2);

        REQUIRE(first.SetParent(secondParent));
        CHECK(third.GetSiblingIndex() == 0);
        CHECK(second.GetSiblingIndex() == 1);
        CHECK(first.GetSiblingIndex() == 0);

        REQUIRE(first.SetParent(firstParent));
        CHECK(first.GetSiblingIndex() == 2);
        CHECK(firstParent.GetChild(2) == first);
    }

    SECTION("Duplicate is inserted beside the source and owns cloned children")
    {
        Entity parent = scene->CreateEntity("Parent");
        Entity source = scene->CreateEntity("Source");
        Entity sibling = scene->CreateEntity("Sibling");
        Entity childA = scene->CreateEntity("A");
        Entity childB = scene->CreateEntity("B");
        source.SetParent(parent);
        sibling.SetParent(parent);
        childA.SetParent(source);
        childB.SetParent(source);

        Entity clone = scene->DuplicateEntity(source, true);

        REQUIRE(clone);
        CHECK(clone.GetParent() == parent);
        CHECK(parent.GetChild(0) == source);
        CHECK(parent.GetChild(1) == clone);
        CHECK(parent.GetChild(2) == sibling);
        REQUIRE(clone.GetChildCount() == 2);
        CHECK(clone.GetChild(0).GetParent() == clone);
        CHECK(clone.GetChild(1).GetParent() == clone);
        CHECK(clone.GetChild(0).GetName() == "A");
        CHECK(clone.GetChild(1).GetName() == "B");
    }

    SECTION("Duplicate attaches each clone to its final parent once")
    {
        Entity parent = scene->CreateEntity("Parent");
        Entity source = scene->CreateEntity("Source");
        Entity child = scene->CreateEntity("Child");
        source.SetParent(parent);
        child.SetParent(source);
        parent.SetPosition({ 10.0f, 20.0f, 30.0f });
        source.SetPosition({ 1.0f, 2.0f, 3.0f });
        child.SetPosition({ 4.0f, 5.0f, 6.0f });

        auto& sourceChildren = source.GetComponent<RelationshipComponent>().Children;
        sourceChildren.shrink_to_fit();
        const Entity* sourceChildStorage = sourceChildren.data();
        const size_t sourceChildCapacity = sourceChildren.capacity();
        const glm::mat4 sourceWorld = source.GetWorldMatrix();
        const glm::mat4 childWorld = child.GetWorldMatrix();

        Entity clone = scene->DuplicateEntity(source, true);

        REQUIRE(clone);
        REQUIRE(clone.GetChildCount() == 1);
        CHECK(sourceChildren.data() == sourceChildStorage);
        CHECK(sourceChildren.capacity() == sourceChildCapacity);
        CHECK(source.GetChild(0) == child);
        CHECK(clone.GetLocalPosition() == source.GetLocalPosition());
        CHECK(clone.GetChild(0).GetLocalPosition() == child.GetLocalPosition());
        ExpectMatrixEqual(clone.GetWorldMatrix(), sourceWorld);
        ExpectMatrixEqual(clone.GetChild(0).GetWorldMatrix(), childWorld);
    }
}
