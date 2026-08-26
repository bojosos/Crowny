#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "Editor/BoxCollider2DBoundsTransaction.h"

#include "Crowny/Ecs/Components.h"
#include "Crowny/Scene/Scene.h"

using namespace Crowny;

namespace
{
    void CheckBounds(Entity entity, const glm::vec2& offset, const glm::vec2& size)
    {
        REQUIRE(entity.HasComponent<BoxCollider2DComponent>());
        const BoxCollider2DComponent& collider = entity.GetComponent<BoxCollider2DComponent>();
        CHECK(collider.GetOffset().x == Catch::Approx(offset.x));
        CHECK(collider.GetOffset().y == Catch::Approx(offset.y));
        CHECK(collider.GetSize().x == Catch::Approx(size.x));
        CHECK(collider.GetSize().y == Catch::Approx(size.y));
    }
} // namespace

TEST_CASE("Box collider bounds drag creates one exact undo action", "[Editor][Undo][Viewport][Collider]")
{
    Ref<Scene> scene = CreateRef<Scene>(false);
    Entity entity = scene->CreateEntity("Collider");
    BoxCollider2DComponent& collider = entity.AddComponent<BoxCollider2DComponent>();
    collider.SetOffset({ 1.0f, 2.0f }, {});
    collider.SetSize({ 3.0f, 4.0f }, {});

    UndoRedo::StartUp();
    BoxCollider2DBoundsTransaction transaction;
    REQUIRE(transaction.Begin(entity));
    REQUIRE(transaction.Update({ 2.0f, 3.0f }, { 4.0f, 5.0f }));
    REQUIRE(transaction.Update({ 6.0f, 7.0f }, { 8.0f, 9.0f }));

    entity.GetComponent<BoxCollider2DComponent>().SetIsTrigger(true);
    UndoRedo::Get().RegisterAction(transaction.Commit());
    REQUIRE(UndoRedo::Get().CanUndo());

    UndoRedo::Get().Undo();
    CheckBounds(entity, { 1.0f, 2.0f }, { 3.0f, 4.0f });
    CHECK(entity.GetComponent<BoxCollider2DComponent>().IsTrigger());
    CHECK_FALSE(UndoRedo::Get().CanUndo());

    UndoRedo::Get().Redo();
    CheckBounds(entity, { 6.0f, 7.0f }, { 8.0f, 9.0f });
    CHECK(entity.GetComponent<BoxCollider2DComponent>().IsTrigger());
    UndoRedo::Shutdown();
}

TEST_CASE("Box collider bounds no-op does not create undo", "[Editor][Undo][Viewport][Collider]")
{
    Ref<Scene> scene = CreateRef<Scene>(false);
    Entity entity = scene->CreateEntity("Collider");
    BoxCollider2DComponent& collider = entity.AddComponent<BoxCollider2DComponent>();
    collider.SetOffset({ 1.0f, 2.0f }, {});
    collider.SetSize({ 3.0f, 4.0f }, {});

    BoxCollider2DBoundsTransaction transaction;
    REQUIRE(transaction.Begin(entity));
    REQUIRE(transaction.Update({ 5.0f, 6.0f }, { 7.0f, 8.0f }));
    REQUIRE(transaction.Update({ 1.0f, 2.0f }, { 3.0f, 4.0f }));
    CHECK(transaction.Commit() == nullptr);
    CheckBounds(entity, { 1.0f, 2.0f }, { 3.0f, 4.0f });
}

TEST_CASE("Cancelling box collider bounds restores the initial values", "[Editor][Undo][Viewport][Collider]")
{
    Ref<Scene> scene = CreateRef<Scene>(false);
    Entity entity = scene->CreateEntity("Collider");
    BoxCollider2DComponent& collider = entity.AddComponent<BoxCollider2DComponent>();
    collider.SetOffset({ 1.0f, 2.0f }, {});
    collider.SetSize({ 3.0f, 4.0f }, {});

    BoxCollider2DBoundsTransaction transaction;
    REQUIRE(transaction.Begin(entity));
    REQUIRE(transaction.Update({ 5.0f, 6.0f }, { 7.0f, 8.0f }));
    transaction.Cancel();

    CHECK_FALSE(transaction.IsActive());
    CHECK(transaction.Commit() == nullptr);
    CheckBounds(entity, { 1.0f, 2.0f }, { 3.0f, 4.0f });
}

TEST_CASE("Box collider bounds transactions cannot cross scene selections", "[Editor][Undo][Viewport][Collider]")
{
    Ref<Scene> firstScene = CreateRef<Scene>(false);
    Ref<Scene> secondScene = CreateRef<Scene>(false);
    Entity first = firstScene->CreateEntity("First collider");
    Entity second = secondScene->CreateEntity("Second collider");
    first.AddComponent<BoxCollider2DComponent>();
    second.AddComponent<BoxCollider2DComponent>();

    BoxCollider2DBoundsTransaction transaction;
    REQUIRE(transaction.Begin(first));
    CHECK(transaction.Owns(first));
    CHECK_FALSE(transaction.Owns(second));
    REQUIRE(transaction.Update({ 1.0f, 2.0f }, { 3.0f, 4.0f }));

    Ref<UndoAction> firstAction = transaction.Commit();
    REQUIRE(firstAction != nullptr);
    REQUIRE(transaction.Begin(second));
    CHECK(transaction.Owns(second));
    REQUIRE(transaction.Update({ 5.0f, 6.0f }, { 7.0f, 8.0f }));

    Ref<UndoAction> secondAction = transaction.Commit();
    REQUIRE(secondAction != nullptr);
    firstAction->Revert();
    CheckBounds(first, { 0.0f, 0.0f }, { 0.5f, 0.5f });
    CheckBounds(second, { 5.0f, 6.0f }, { 7.0f, 8.0f });
    secondAction->Revert();
    CheckBounds(second, { 0.0f, 0.0f }, { 0.5f, 0.5f });
}

TEST_CASE("Box collider bounds transactions retain their target scene", "[Editor][Undo][Viewport][Collider][Lifetime]")
{
    Ref<Scene> scene = CreateRef<Scene>(false);
    Entity entity = scene->CreateEntity("Retained collider");
    entity.AddComponent<BoxCollider2DComponent>();

    BoxCollider2DBoundsTransaction transaction;
    REQUIRE(transaction.Begin(entity));
    entity = {};
    scene = nullptr;

    REQUIRE(transaction.Update({ 1.0f, 2.0f }, { 3.0f, 4.0f }));
    Ref<UndoAction> action = transaction.Commit();
    REQUIRE(action != nullptr);
    Entity retained = action->GetFocusEntity();
    REQUIRE(retained);
    CheckBounds(retained, { 1.0f, 2.0f }, { 3.0f, 4.0f });
    action->Revert();
    CheckBounds(retained, { 0.0f, 0.0f }, { 0.5f, 0.5f });
}
