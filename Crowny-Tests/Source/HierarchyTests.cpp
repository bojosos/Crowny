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
        const auto& sourceChildrenAfter = source.GetComponent<RelationshipComponent>().Children;
        CHECK(sourceChildrenAfter.data() == sourceChildStorage);
        CHECK(sourceChildrenAfter.capacity() == sourceChildCapacity);
        CHECK(source.GetChild(0) == child);
        CHECK(clone.GetLocalPosition() == source.GetLocalPosition());
        CHECK(clone.GetChild(0).GetLocalPosition() == child.GetLocalPosition());
        ExpectMatrixEqual(clone.GetWorldMatrix(), sourceWorld);
        ExpectMatrixEqual(clone.GetChild(0).GetWorldMatrix(), childWorld);
    }
}

TEST_CASE("Bulk hierarchy reparent preserves input order and world transforms", "[Ecs][Hierarchy][Bulk]")
{
    Ref<Scene> scene = CreateRef<Scene>(false);
    Entity firstParent = scene->CreateEntity("First parent");
    Entity secondParent = scene->CreateEntity("Second parent");
    Entity destination = scene->CreateEntity("Destination");
    Entity first = scene->CreateEntity("First");
    Entity second = scene->CreateEntity("Second");
    Entity third = scene->CreateEntity("Third");
    Entity destinationBefore = scene->CreateEntity("Destination before");
    Entity destinationAfter = scene->CreateEntity("Destination after");

    REQUIRE(first.SetParent(firstParent));
    REQUIRE(second.SetParent(firstParent));
    REQUIRE(third.SetParent(secondParent));
    REQUIRE(destinationBefore.SetParent(destination));
    REQUIRE(destinationAfter.SetParent(destination));
    firstParent.SetPosition({ 10.0f, 0.0f, 0.0f });
    secondParent.SetPosition({ 0.0f, 20.0f, 0.0f });
    destination.SetPosition({ -30.0f, 0.0f, 5.0f });
    first.SetPosition({ 1.0f, 2.0f, 3.0f });
    second.SetPosition({ 4.0f, 5.0f, 6.0f });
    third.SetPosition({ 7.0f, 8.0f, 9.0f });

    const glm::mat4 firstWorld = first.GetWorldMatrix();
    const glm::mat4 secondWorld = second.GetWorldMatrix();
    const glm::mat4 thirdWorld = third.GetWorldMatrix();
    const Array<Entity, 3> moving{ second, third, first };

    const HierarchyMutationResult result = scene->ReparentEntities(moving, destination, 1u);

    REQUIRE(result.Succeeded);
    CHECK(result.Stats.RootEntityCount == 3u);
    CHECK(result.Stats.ReparentedEntityCount == 3u);
    CHECK(result.Stats.ParentVectorRebuildCount == 3u);
    CHECK(result.Stats.TransformInvalidationRootCount == 3u);
    REQUIRE(destination.GetChildCount() == 5u);
    CHECK(destination.GetChild(0) == destinationBefore);
    CHECK(destination.GetChild(1) == second);
    CHECK(destination.GetChild(2) == third);
    CHECK(destination.GetChild(3) == first);
    CHECK(destination.GetChild(4) == destinationAfter);
    CHECK(firstParent.GetChildCount() == 0u);
    CHECK(secondParent.GetChildCount() == 0u);
    ExpectMatrixEqual(first.GetWorldMatrix(), firstWorld);
    ExpectMatrixEqual(second.GetWorldMatrix(), secondWorld);
    ExpectMatrixEqual(third.GetWorldMatrix(), thirdWorld);
}

TEST_CASE("Bulk hierarchy destroy normalizes selected descendants", "[Ecs][Hierarchy][Bulk]")
{
    Ref<Scene> scene = CreateRef<Scene>();
    Entity parent = scene->CreateEntity("Parent");
    Entity child = scene->CreateEntity("Child");
    Entity grandChild = scene->CreateEntity("Grand child");
    REQUIRE(child.SetParent(parent));
    REQUIRE(grandChild.SetParent(child));
    const UUID parentId = parent.GetUuid();
    const UUID childId = child.GetUuid();
    const UUID grandChildId = grandChild.GetUuid();
    const Array<Entity, 2> selected{ parent, child };

    const HierarchyMutationResult result = scene->DestroyEntities(selected);

    REQUIRE(result.Succeeded);
    CHECK(result.Stats.RootEntityCount == 1u);
    CHECK(result.Stats.DestroyedEntityCount == 3u);
    CHECK_FALSE(scene->TryGetEntityFromUuid(parentId));
    CHECK_FALSE(scene->TryGetEntityFromUuid(childId));
    CHECK_FALSE(scene->TryGetEntityFromUuid(grandChildId));
}

TEST_CASE("Bulk hierarchy preserve-children destroy rejects nested selections atomically", "[Ecs][Hierarchy][Bulk]")
{
    Ref<Scene> scene = CreateRef<Scene>(false);
    Entity parent = scene->CreateEntity("Parent");
    Entity child = scene->CreateEntity("Child");
    Entity grandChild = scene->CreateEntity("Grand child");
    REQUIRE(child.SetParent(parent));
    REQUIRE(grandChild.SetParent(child));
    const Array<Entity, 2> selected{ parent, child };

    const HierarchyMutationResult result =
        scene->DestroyEntities(selected, HierarchyDestroyMode::PreserveChildren);

    CHECK_FALSE(result.Succeeded);
    CHECK(parent.IsValid());
    CHECK(child.IsValid());
    CHECK(grandChild.IsValid());
    CHECK(parent.GetParent() == Entity{});
    REQUIRE(parent.GetChildCount() == 1u);
    CHECK(parent.GetChild(0) == child);
    CHECK(child.GetParent() == parent);
    REQUIRE(child.GetChildCount() == 1u);
    CHECK(child.GetChild(0) == grandChild);
    CHECK(grandChild.GetParent() == child);
}

TEST_CASE("Bulk hierarchy preserve-children destroy promotes multiple sibling groups in place", "[Ecs][Hierarchy][Bulk]")
{
    Ref<Scene> scene = CreateRef<Scene>(false);
    Entity parent = scene->CreateEntity("Parent");
    Entity before = scene->CreateEntity("Before");
    Entity firstRemoved = scene->CreateEntity("First removed");
    Entity between = scene->CreateEntity("Between");
    Entity secondRemoved = scene->CreateEntity("Second removed");
    Entity after = scene->CreateEntity("After");
    Entity firstChild = scene->CreateEntity("First child");
    Entity secondChild = scene->CreateEntity("Second child");
    Entity thirdChild = scene->CreateEntity("Third child");
    REQUIRE(before.SetParent(parent));
    REQUIRE(firstRemoved.SetParent(parent));
    REQUIRE(between.SetParent(parent));
    REQUIRE(secondRemoved.SetParent(parent));
    REQUIRE(after.SetParent(parent));
    REQUIRE(firstChild.SetParent(firstRemoved));
    REQUIRE(secondChild.SetParent(firstRemoved));
    REQUIRE(thirdChild.SetParent(secondRemoved));
    const Array<Entity, 2> selected{ secondRemoved, firstRemoved };

    const HierarchyMutationResult result =
        scene->DestroyEntities(selected, HierarchyDestroyMode::PreserveChildren);

    REQUIRE(result.Succeeded);
    CHECK(result.Stats.RootEntityCount == 2u);
    CHECK(result.Stats.DestroyedEntityCount == 2u);
    CHECK(result.Stats.ReparentedEntityCount == 3u);
    CHECK(result.Stats.ParentVectorRebuildCount == 1u);
    REQUIRE(parent.GetChildCount() == 6u);
    CHECK(parent.GetChild(0) == before);
    CHECK(parent.GetChild(1) == firstChild);
    CHECK(parent.GetChild(2) == secondChild);
    CHECK(parent.GetChild(3) == between);
    CHECK(parent.GetChild(4) == thirdChild);
    CHECK(parent.GetChild(5) == after);
    CHECK(firstChild.GetParent() == parent);
    CHECK(secondChild.GetParent() == parent);
    CHECK(thirdChild.GetParent() == parent);
}

TEST_CASE("Bulk hierarchy destroy rejects inconsistent child ownership atomically", "[Ecs][Hierarchy][Bulk]")
{
    Ref<Scene> scene = CreateRef<Scene>(false);
    Entity parent = scene->CreateEntity("Parent");
    Entity child = scene->CreateEntity("Child");
    REQUIRE(child.SetParent(parent));
    child.GetComponent<RelationshipComponent>().Parent = {};
    const Array<Entity, 1> selected{ parent };

    const HierarchyMutationResult result = scene->DestroyEntities(selected);

    CHECK_FALSE(result.Succeeded);
    CHECK(parent.IsValid());
    CHECK(child.IsValid());
    REQUIRE(parent.GetChildCount() == 1u);
    CHECK(parent.GetChild(0) == child);
    child.GetComponent<RelationshipComponent>().Parent = parent;
}

TEST_CASE("Bulk hierarchy reparent rejects the whole batch before mutation", "[Ecs][Hierarchy][Bulk]")
{
    Ref<Scene> scene = CreateRef<Scene>(false);
    Entity firstParent = scene->CreateEntity("First parent");
    Entity secondParent = scene->CreateEntity("Second parent");
    Entity destination = scene->CreateEntity("Destination");
    Entity first = scene->CreateEntity("First");
    Entity second = scene->CreateEntity("Second");
    REQUIRE(first.SetParent(firstParent));
    REQUIRE(second.SetParent(secondParent));
    destination.SetScale({ 0.0f, 1.0f, 1.0f });
    const glm::mat4 firstWorld = first.GetWorldMatrix();
    const glm::mat4 secondWorld = second.GetWorldMatrix();
    const Array<Entity, 2> moving{ first, second };

    const HierarchyMutationResult result = scene->ReparentEntities(moving, destination);

    CHECK_FALSE(result.Succeeded);
    CHECK(first.GetParent() == firstParent);
    CHECK(second.GetParent() == secondParent);
    CHECK(firstParent.GetChild(0) == first);
    CHECK(secondParent.GetChild(0) == second);
    CHECK(destination.GetChildCount() == 0u);
    ExpectMatrixEqual(first.GetWorldMatrix(), firstWorld);
    ExpectMatrixEqual(second.GetWorldMatrix(), secondWorld);
}

TEST_CASE("Bulk hierarchy destroy rebuilds one parent vector linearly", "[Ecs][Hierarchy][Bulk][Performance]")
{
    constexpr uint32_t childCount = 10000u;
    Ref<Scene> scene = CreateRef<Scene>(false);
    Entity parent = scene->CreateEntity("Parent");
    Vector<Entity> children;
    Vector<Entity> removed;
    children.reserve(childCount);
    removed.reserve(childCount / 2u);
    bool allParented = true;
    for (uint32_t index = 0; index < childCount; index++)
    {
        Entity child = scene->CreateEntity("Child");
        allParented = child.SetParent(parent) && allParented;
        children.push_back(child);
        if ((index & 1u) == 0u)
            removed.push_back(child);
    }

    REQUIRE(allParented);
    const HierarchyMutationResult result = scene->DestroyEntities(removed);

    REQUIRE(result.Succeeded);
    CHECK(result.Stats.RootEntityCount == childCount / 2u);
    CHECK(result.Stats.DestroyedEntityCount == childCount / 2u);
    CHECK(result.Stats.ParentVectorRebuildCount == 1u);
    CHECK(result.Stats.SiblingIndexWriteCount == childCount / 2u);
    REQUIRE(parent.GetChildCount() == childCount / 2u);
    bool orderAndIndicesCorrect = true;
    for (uint32_t index = 0; index < parent.GetChildCount(); index++)
    {
        orderAndIndicesCorrect = parent.GetChild(index) == children[index * 2u + 1u] &&
                                 parent.GetChild(index).GetSiblingIndex() == index && orderAndIndicesCorrect;
    }
    CHECK(orderAndIndicesCorrect);
}

TEST_CASE("Transform mutation scopes merge overlapping hierarchy ranges", "[Ecs][Hierarchy][Transform][Performance]")
{
    Ref<Scene> scene = CreateRef<Scene>(false);
    Entity parent = scene->CreateEntity("Parent");
    Entity child = scene->CreateEntity("Child");
    Entity grandChild = scene->CreateEntity("Grand child");
    Entity sibling = scene->CreateEntity("Sibling");
    REQUIRE(child.SetParent(parent));
    REQUIRE(grandChild.SetParent(child));
    REQUIRE(sibling.SetParent(parent));

    grandChild.GetWorldMatrix();
    sibling.GetWorldMatrix();
    {
        auto scope = scene->DeferTransformChanges();
        parent.SetLocalTransform(Transform({ 2.0f, 0.0f, 0.0f }, glm::quat(1.0f, 0.0f, 0.0f, 0.0f), { 1.0f, 1.0f, 1.0f }), false);
        child.NotifyTransformChanged(true);
    }

    const TransformPropagationStats& stats = scene->GetLastTransformPropagationStats();
    CHECK(stats.QueueRequestCount == 2u);
    CHECK(stats.UniqueRootCount == 2u);
    CHECK(stats.MergedRangeCount == 1u);
    CHECK(stats.FlushPassCount == 1u);
    CHECK(stats.VisitedEntityCount == 4u);
    CHECK(stats.PhysicsEnabledEntityVisitCount == 2u);
    CHECK_FALSE(parent.GetTransform().IsCachedWorldTransformValid());
    CHECK_FALSE(child.GetTransform().IsCachedWorldTransformValid());
    CHECK_FALSE(grandChild.GetTransform().IsCachedWorldTransformValid());
    CHECK_FALSE(sibling.GetTransform().IsCachedWorldTransformValid());
    CHECK(grandChild.GetWorldPosition() == glm::vec3(2.0f, 0.0f, 0.0f));
}

TEST_CASE("Deep hierarchy transform propagation and resolution are iterative", "[Ecs][Hierarchy][Transform][Performance]")
{
    constexpr uint32_t depth = 10000u;
    Ref<Scene> scene = CreateRef<Scene>(false);
    Entity root = scene->CreateEntity("Root");
    Entity deepest = root;
    bool allParented = true;
    for (uint32_t index = 1u; index < depth; index++)
    {
        Entity child = scene->CreateEntity("Child");
        allParented = child.SetParent(deepest) && allParented;
        deepest = child;
    }
    REQUIRE(allParented);

    root.SetPosition({ 3.0f, 4.0f, 5.0f });
    const TransformPropagationStats& stats = scene->GetLastTransformPropagationStats();
    CHECK(stats.VisitedEntityCount == depth);
    CHECK(deepest.GetWorldPosition() == glm::vec3(3.0f, 4.0f, 5.0f));
}
