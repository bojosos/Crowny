#include <catch2/catch_test_macros.hpp>

#include "Crowny/Scene/Scene.h"
#include "Editor/EntitySelection.h"

using namespace Crowny;

TEST_CASE("Editor entity selection is ordered and scene-local", "[Editor][Selection]")
{
    Ref<Scene> scene = CreateRef<Scene>(false);
    Entity first = scene->CreateEntity("First");
    Entity second = scene->CreateEntity("Second");
    Entity third = scene->CreateEntity("Third");
    Entity fourth = scene->CreateEntity("Fourth");
    const Vector<Entity> visible{ first, second, third, fourth };
    const Vector<Entity> firstRange{ first, second, third };
    const Vector<Entity> onlyFirst{ first };

    EntitySelection selection;
    REQUIRE(selection.Select(first, EntitySelectionMode::Replace));
    REQUIRE(selection.Select(third, EntitySelectionMode::Range, visible));
    REQUIRE(selection.GetAll() == firstRange);
    CHECK(selection.GetPrimary() == third);

    REQUIRE(selection.Select(second, EntitySelectionMode::Toggle));
    CHECK_FALSE(selection.Contains(second));
    CHECK(selection.Contains(first));
    CHECK(selection.Contains(third));

    REQUIRE(selection.Select(fourth, EntitySelectionMode::AddRange, visible));
    CHECK(selection.Contains(first));
    CHECK(selection.Contains(second));
    CHECK(selection.Contains(third));
    CHECK(selection.Contains(fourth));
    CHECK(selection.GetPrimary() == fourth);

    REQUIRE(selection.Select(first, EntitySelectionMode::Replace));
    REQUIRE(selection.Select(third, EntitySelectionMode::Add));

    third.Destroy();
    REQUIRE(selection.Prune(scene.get()));
    CHECK(selection.GetPrimary() == first);
    REQUIRE(selection.GetAll() == onlyFirst);

    Ref<Scene> replacement = CreateRef<Scene>(false);
    replacement->CreateEntity("Replacement");
    REQUIRE(selection.Prune(replacement.get()));
    CHECK(selection.Empty());
    CHECK_FALSE(selection.GetPrimary());
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
