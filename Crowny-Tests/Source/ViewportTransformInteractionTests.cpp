#include "Editor/ViewportTransformInteraction.h"

#include "Crowny/Scene/Scene.h"

#include <catch2/catch_test_macros.hpp>
#include <glm/gtc/matrix_transform.hpp>

using namespace Crowny;

TEST_CASE("Viewport transform interactions resolve commit and cancellation", "[Editor][Viewport][Undo]")
{
    CHECK(ResolveTransformInteractionCompletion(false, false, false) == TransformInteractionCompletion::None);
    CHECK(ResolveTransformInteractionCompletion(false, true, true) == TransformInteractionCompletion::None);
    CHECK(ResolveTransformInteractionCompletion(true, true, false) == TransformInteractionCompletion::None);
    CHECK(ResolveTransformInteractionCompletion(true, false, false) == TransformInteractionCompletion::Commit);
    CHECK(ResolveTransformInteractionCompletion(true, true, true) == TransformInteractionCompletion::Cancel);
    CHECK(ResolveTransformInteractionCompletion(true, false, true) == TransformInteractionCompletion::Cancel);
}

TEST_CASE("Escape cancels a viewport transform interaction without recording undo", "[Editor][Viewport][Undo]")
{
    Ref<Scene> scene = CreateRef<Scene>(false);
    Entity first = scene->CreateEntity("First");
    Entity second = scene->CreateEntity("Second");
    const glm::mat4 firstTransform = glm::translate(glm::mat4(1.0f), { 1.0f, 2.0f, 3.0f });
    const glm::mat4 secondTransform = glm::translate(glm::mat4(1.0f), { -4.0f, 5.0f, 6.0f });
    first.SetWorldTransform(firstTransform);
    second.SetWorldTransform(secondTransform);

    UndoRedo::StartUp();
    ViewportTransformInteraction interaction;
    REQUIRE(interaction.Begin({ first, second }, glm::mat4(1.0f)));
    REQUIRE(interaction.Update(glm::translate(glm::mat4(1.0f), { 10.0f, 0.0f, 0.0f })));
    CHECK(first.GetWorldMatrix() != firstTransform);
    CHECK(second.GetWorldMatrix() != secondTransform);

    const ViewportTransformResolution resolution = interaction.Resolve(true, true);
    CHECK(resolution.State == TransformInteractionCompletion::Cancel);
    CHECK(resolution.Action == nullptr);
    UndoRedo::Get().RegisterAction(resolution.Action);

    CHECK(first.GetWorldMatrix() == firstTransform);
    CHECK(second.GetWorldMatrix() == secondTransform);
    CHECK_FALSE(interaction.IsActive());
    CHECK_FALSE(UndoRedo::Get().CanUndo());
    UndoRedo::Shutdown();
}

TEST_CASE("Viewport transform release commits one grouped action for undo and redo", "[Editor][Viewport][Undo]")
{
    Ref<Scene> scene = CreateRef<Scene>(false);
    Entity first = scene->CreateEntity("First");
    Entity second = scene->CreateEntity("Second");
    const glm::mat4 firstTransform = glm::translate(glm::mat4(1.0f), { 1.0f, 2.0f, 3.0f });
    const glm::mat4 secondTransform = glm::translate(glm::mat4(1.0f), { -4.0f, 5.0f, 6.0f });
    const glm::mat4 delta = glm::translate(glm::mat4(1.0f), { 10.0f, 0.0f, 0.0f });
    first.SetWorldTransform(firstTransform);
    second.SetWorldTransform(secondTransform);

    UndoRedo::StartUp();
    ViewportTransformInteraction interaction;
    REQUIRE(interaction.Begin({ first, second }, glm::mat4(1.0f)));
    REQUIRE(interaction.Update(delta));

    const ViewportTransformResolution resolution = interaction.Resolve(false, false);
    CHECK(resolution.State == TransformInteractionCompletion::Commit);
    REQUIRE(resolution.Action != nullptr);
    UndoRedo::Get().RegisterAction(resolution.Action);
    CHECK(UndoRedo::Get().CanUndo());

    const glm::mat4 firstCommitted = first.GetWorldMatrix();
    const glm::mat4 secondCommitted = second.GetWorldMatrix();
    UndoRedo::Get().Undo();
    CHECK(first.GetWorldMatrix() == firstTransform);
    CHECK(second.GetWorldMatrix() == secondTransform);
    UndoRedo::Get().Redo();
    CHECK(first.GetWorldMatrix() == firstCommitted);
    CHECK(second.GetWorldMatrix() == secondCommitted);
    UndoRedo::Shutdown();
}
