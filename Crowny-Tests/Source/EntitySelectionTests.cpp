#include <catch2/catch_test_macros.hpp>

#include "Crowny/Memory/AllocationCounter.h"
#include "Crowny/Scene/Scene.h"
#include "Editor/EntitySelection.h"

using namespace Crowny;

TEST_CASE("Hierarchy selection modifiers resolve without ImGui state", "[Editor][Selection][Hierarchy]")
{
    CHECK(ResolveEntitySelectionMode(false, false) == EntitySelectionMode::Replace);
    CHECK(ResolveEntitySelectionMode(true, false) == EntitySelectionMode::Toggle);
    CHECK(ResolveEntitySelectionMode(false, true) == EntitySelectionMode::Range);
    CHECK(ResolveEntitySelectionMode(true, true) == EntitySelectionMode::AddRange);
}

TEST_CASE("Hierarchy control toggles keep a stable range anchor", "[Editor][Selection][Hierarchy]")
{
    Ref<Scene> scene = CreateRef<Scene>(false);
    Entity first = scene->CreateEntity("First");
    Entity second = scene->CreateEntity("Second");
    Entity third = scene->CreateEntity("Third");
    Entity fourth = scene->CreateEntity("Fourth");
    const Vector<Entity> visible{ first, second, third, fourth };

    EntitySelection selection;
    REQUIRE(selection.Select(second, EntitySelectionMode::Toggle));
    REQUIRE(selection.Select(second, EntitySelectionMode::Toggle));
    CHECK(selection.Empty());
    CHECK_FALSE(selection.GetPrimary());

    REQUIRE(selection.Select(fourth, EntitySelectionMode::Range, visible));
    CHECK((selection.GetAll() == Vector<Entity>{ second, third, fourth }));
    CHECK(selection.GetPrimary() == fourth);

    REQUIRE(selection.Select(third, EntitySelectionMode::Toggle));
    CHECK(selection.Contains(second));
    CHECK_FALSE(selection.Contains(third));
    CHECK(selection.GetPrimary() == fourth);

    REQUIRE(selection.Select(first, EntitySelectionMode::Range, visible));
    CHECK((selection.GetAll() == Vector<Entity>{ first, second, third }));
    CHECK(selection.GetPrimary() == first);
}

TEST_CASE("Hierarchy ranges use the current filtered and reordered view", "[Editor][Selection][Hierarchy]")
{
    Ref<Scene> scene = CreateRef<Scene>(false);
    Entity first = scene->CreateEntity("First");
    Entity second = scene->CreateEntity("Second");
    Entity third = scene->CreateEntity("Third");
    Entity fourth = scene->CreateEntity("Fourth");
    const Vector<Entity> fullOrder{ first, second, third, fourth };

    EntitySelection selection;
    REQUIRE(selection.Select(first, EntitySelectionMode::Replace));
    REQUIRE(selection.Select(third, EntitySelectionMode::Range, fullOrder));
    CHECK((selection.GetAll() == Vector<Entity>{ first, second, third }));

    const Vector<Entity> filtered{ second, third, fourth };
    REQUIRE(selection.Select(fourth, EntitySelectionMode::Range, filtered));
    CHECK((selection.GetAll() == Vector<Entity>{ third, fourth }));

    const Vector<Entity> reordered{ fourth, third, second };
    REQUIRE(selection.Select(second, EntitySelectionMode::Range, reordered));
    CHECK((selection.GetAll() == Vector<Entity>{ third, second }));

    const Vector<Entity> noSelectedEntities{ first };
    REQUIRE(selection.Select(first, EntitySelectionMode::Range, noSelectedEntities));
    CHECK(selection.GetAll() == Vector<Entity>{ first });
    CHECK(selection.GetPrimary() == first);
}

TEST_CASE("Hierarchy selection recovers when entities or scenes disappear", "[Editor][Selection][Hierarchy]")
{
    Ref<Scene> scene = CreateRef<Scene>(false);
    Entity first = scene->CreateEntity("First");
    Entity second = scene->CreateEntity("Second");
    Entity third = scene->CreateEntity("Third");
    Entity fourth = scene->CreateEntity("Fourth");

    EntitySelection selection;
    REQUIRE(selection.Select(second, EntitySelectionMode::Toggle));
    REQUIRE(selection.Select(second, EntitySelectionMode::Toggle));
    second.Destroy();
    REQUIRE(selection.Prune(scene.get()));
    CHECK_FALSE(selection.Prune(scene.get()));
    CHECK(selection.Empty());

    const Vector<Entity> staleOrder{ first, second, third, fourth };
    REQUIRE(selection.Select(first, EntitySelectionMode::Range, staleOrder));
    CHECK(selection.GetAll() == Vector<Entity>{ first });

    third.Destroy();
    REQUIRE(selection.Select(fourth, EntitySelectionMode::Range, staleOrder));
    CHECK((selection.GetAll() == Vector<Entity>{ first, fourth }));
    CHECK(selection.GetPrimary() == fourth);

    Ref<Scene> replacement = CreateRef<Scene>(false);
    Entity replacementEntity = replacement->CreateEntity("Replacement");
    REQUIRE(selection.Select(replacementEntity, EntitySelectionMode::Toggle));
    CHECK_FALSE(selection.Contains(first));
    CHECK(selection.GetAll() == Vector<Entity>{ replacementEntity });
    CHECK(selection.GetPrimary() == replacementEntity);

    Entity replacementSecond = replacement->CreateEntity("Replacement second");
    REQUIRE(selection.Select(replacementSecond, EntitySelectionMode::Add));
    replacementSecond.Destroy();
    REQUIRE(selection.Prune(replacement.get()));
    CHECK(selection.GetAll() == Vector<Entity>{ replacementEntity });
    CHECK(selection.GetPrimary() == replacementEntity);
}

TEST_CASE("Hierarchy additive ranges preserve prior selections", "[Editor][Selection][Hierarchy]")
{
    Ref<Scene> scene = CreateRef<Scene>(false);
    Entity first = scene->CreateEntity("First");
    Entity second = scene->CreateEntity("Second");
    Entity third = scene->CreateEntity("Third");
    Entity fourth = scene->CreateEntity("Fourth");
    const Vector<Entity> visible{ first, second, third, fourth };

    EntitySelection selection;
    REQUIRE(selection.Select(first, EntitySelectionMode::Replace));
    REQUIRE(selection.Select(second, EntitySelectionMode::Toggle));
    REQUIRE(selection.Select(fourth, EntitySelectionMode::AddRange, visible));
    CHECK(selection.Contains(first));
    CHECK(selection.Contains(second));
    CHECK(selection.Contains(third));
    CHECK(selection.Contains(fourth));
    CHECK(selection.GetPrimary() == fourth);
}

TEST_CASE("Large hierarchy selection reuses its membership storage", "[Editor][Selection][Hierarchy][Memory]")
{
    constexpr size_t entityCount = 10000u;
    Ref<Scene> scene = CreateRef<Scene>(false);
    Vector<Entity> forward;
    forward.reserve(entityCount);
    for (size_t index = 0; index < entityCount; ++index)
        forward.push_back(scene->CreateEntity("Entity"));

    Vector<Entity> reverse(forward.rbegin(), forward.rend());
    EntitySelection selection;
    REQUIRE(selection.Select(forward.front(), EntitySelectionMode::Replace));
    REQUIRE(selection.Select(forward.back(), EntitySelectionMode::AddRange, forward));
    REQUIRE(selection.GetAll() == forward);

    bool membershipCorrect = true;
    const auto exercise = [&]() {
        selection.Select(forward.back(), EntitySelectionMode::AddRange, forward);
        selection.Select(forward.back(), EntitySelectionMode::Range, reverse);
        for (Entity entity : forward)
            membershipCorrect = membershipCorrect && selection.Contains(entity);
        selection.Select(forward.back(), EntitySelectionMode::AddRange, reverse);
        selection.Select(forward.back(), EntitySelectionMode::Range, forward);
    };

    exercise();
    exercise();
    const Memory::ThreadAllocationSnapshot before = Memory::GetThreadAllocationSnapshot();
    for (uint32_t iteration = 0; iteration < 8u; ++iteration)
        exercise();
    const Memory::ThreadAllocationSnapshot delta = Memory::GetThreadAllocationDelta(before, Memory::GetThreadAllocationSnapshot());

    CHECK(membershipCorrect);
    CHECK(selection.GetAll() == forward);
    CHECK(selection.GetPrimary() == forward.back());
    CHECK(delta.AllocationCount == 0u);
    CHECK(delta.RequestedBytes == 0u);
}

TEST_CASE("Scene root cannot be parented or destroyed", "[Ecs][Hierarchy]")
{
    Ref<Scene> scene = CreateRef<Scene>();
    Entity root = scene->GetRootEntity();
    Entity child = scene->CreateEntity("Child");

    CHECK_FALSE(root.SetParent(child));
    root.Destroy();
    CHECK(root.IsValid());
    CHECK(child.GetParent() == root);
}
