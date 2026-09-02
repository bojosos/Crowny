#include "Editor/ViewportTransformInteraction.h"

#include "Crowny/Scene/Scene.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cmath>

using namespace Crowny;

namespace
{
    void CheckVector(const glm::vec3& actual, const glm::vec3& expected)
    {
        CHECK(actual.x == Catch::Approx(expected.x).margin(0.0001f));
        CHECK(actual.y == Catch::Approx(expected.y).margin(0.0001f));
        CHECK(actual.z == Catch::Approx(expected.z).margin(0.0001f));
    }

    void CheckRotation(const glm::quat& actual, const glm::quat& expected)
    {
        CHECK(std::abs(glm::dot(glm::normalize(actual), glm::normalize(expected))) == Catch::Approx(1.0f).margin(0.0001f));
    }
} // namespace

TEST_CASE("Viewport transform interactions resolve commit and cancellation", "[Editor][Viewport][Undo]")
{
    CHECK(ResolveTransformInteractionCompletion(false, false, false) == TransformInteractionCompletion::None);
    CHECK(ResolveTransformInteractionCompletion(false, true, true) == TransformInteractionCompletion::None);
    CHECK(ResolveTransformInteractionCompletion(true, true, false) == TransformInteractionCompletion::None);
    CHECK(ResolveTransformInteractionCompletion(true, false, false) == TransformInteractionCompletion::Commit);
    CHECK(ResolveTransformInteractionCompletion(true, true, true) == TransformInteractionCompletion::Cancel);
    CHECK(ResolveTransformInteractionCompletion(true, false, true) == TransformInteractionCompletion::Cancel);
}

TEST_CASE("Viewport gizmo frame applies a single world axis drag and records one undo action", "[Editor][Viewport][Transform][Undo]")
{
    Ref<Scene> scene = CreateRef<Scene>(false);
    Entity entity = scene->CreateEntity("Entity");
    entity.SetWorldPosition({ 1.0f, 2.0f, 3.0f });

    UndoRedo::StartUp();
    ViewportTransformInteraction interaction;
    const glm::mat4 initialPivot =
      ViewportTransformInteraction::CalculatePivot({ entity }, entity, SelectionTransformSpace::World);
    const glm::mat4 movedPivot = glm::translate(glm::mat4(1.0f), { 4.0f, 0.0f, 0.0f }) * initialPivot;

    const ViewportTransformFrameResult activation = interaction.ProcessGizmoFrame(
      { entity }, entity, SelectionTransformOperation::Translate, SelectionTransformSpace::World,
      ViewportGizmoFrame{ initialPivot, false, true, false });
    CHECK_FALSE(activation.Manipulated);
    CHECK(activation.GizmoUsing);
    CHECK(activation.BeginAttempted);
    CHECK(activation.Began);
    CHECK_FALSE(activation.UpdateAttempted);
    CHECK_FALSE(activation.Updated);
    CheckVector(activation.Before.WorldPosition, { 1.0f, 2.0f, 3.0f });
    CheckVector(activation.After.WorldPosition, { 1.0f, 2.0f, 3.0f });

    const ViewportTransformFrameResult drag = interaction.ProcessGizmoFrame(
      { entity }, entity, SelectionTransformOperation::Translate, SelectionTransformSpace::World,
      ViewportGizmoFrame{ movedPivot, true, true, false });
    CHECK(drag.Manipulated);
    CHECK(drag.GizmoUsing);
    CHECK_FALSE(drag.CancelRequested);
    CHECK_FALSE(drag.BeginAttempted);
    CHECK_FALSE(drag.Began);
    CHECK(drag.UpdateAttempted);
    CHECK(drag.Updated);
    CHECK(drag.Resolution.State == TransformInteractionCompletion::None);
    REQUIRE(drag.Before.Valid);
    REQUIRE(drag.After.Valid);
    CheckVector(drag.Before.WorldPosition, { 1.0f, 2.0f, 3.0f });
    CheckVector(drag.Before.LocalPosition, { 1.0f, 2.0f, 3.0f });
    CheckVector(drag.After.WorldPosition, { 5.0f, 2.0f, 3.0f });
    CheckVector(drag.After.LocalPosition, { 5.0f, 2.0f, 3.0f });
    CheckVector(entity.GetWorldPosition(), { 5.0f, 2.0f, 3.0f });

    const ViewportTransformFrameResult release = interaction.ProcessGizmoFrame(
      { entity }, entity, SelectionTransformOperation::Translate, SelectionTransformSpace::World,
      ViewportGizmoFrame{ movedPivot, false, false, false });
    CHECK_FALSE(release.Manipulated);
    CHECK_FALSE(release.GizmoUsing);
    CHECK(release.Resolution.State == TransformInteractionCompletion::Commit);
    REQUIRE(release.Resolution.Action != nullptr);
    UndoRedo::Get().RegisterAction(release.Resolution.Action);
    CHECK(UndoRedo::Get().CanUndo());

    UndoRedo::Get().Undo();
    CheckVector(entity.GetWorldPosition(), { 1.0f, 2.0f, 3.0f });
    CHECK_FALSE(UndoRedo::Get().CanUndo());
    CHECK(UndoRedo::Get().CanRedo());
    UndoRedo::Get().Redo();
    CheckVector(entity.GetWorldPosition(), { 5.0f, 2.0f, 3.0f });
    CHECK_FALSE(UndoRedo::Get().CanRedo());
    UndoRedo::Shutdown();
}

TEST_CASE("Viewport gizmo frame translates planes, local axes, parents, and selections logically", "[Editor][Viewport][Transform]")
{
    SECTION("World plane moves every selected entity")
    {
        Ref<Scene> scene = CreateRef<Scene>(false);
        Entity first = scene->CreateEntity("First");
        Entity second = scene->CreateEntity("Second");
        first.SetWorldPosition({ -1.0f, 0.0f, 0.0f });
        second.SetWorldPosition({ 1.0f, 0.0f, 0.0f });

        ViewportTransformInteraction interaction;
        const glm::mat4 initialPivot =
          ViewportTransformInteraction::CalculatePivot({ first, second }, first, SelectionTransformSpace::World);
        const ViewportTransformFrameResult drag = interaction.ProcessGizmoFrame(
          { first, second }, first, SelectionTransformOperation::Translate, SelectionTransformSpace::World,
          ViewportGizmoFrame{ glm::translate(glm::mat4(1.0f), { 2.0f, 3.0f, 0.0f }) * initialPivot, true, true, false });

        CHECK(drag.Began);
        CHECK(drag.Updated);
        CheckVector(first.GetWorldPosition(), { 1.0f, 3.0f, 0.0f });
        CheckVector(second.GetWorldPosition(), { 3.0f, 3.0f, 0.0f });
        interaction.Cancel();
    }

    SECTION("Local X follows the primary rotation")
    {
        Ref<Scene> scene = CreateRef<Scene>(false);
        Entity entity = scene->CreateEntity("Entity");
        const glm::quat quarterTurn = glm::angleAxis(glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        entity.SetWorldTransform(Math::ComposeMatrix({ 1.0f, 2.0f, 0.0f }, quarterTurn, glm::vec3(1.0f)));

        ViewportTransformInteraction interaction;
        const glm::mat4 initialPivot =
          ViewportTransformInteraction::CalculatePivot({ entity }, entity, SelectionTransformSpace::Local);
        const glm::vec3 localXInWorld = quarterTurn * glm::vec3(2.0f, 0.0f, 0.0f);
        const ViewportTransformFrameResult drag = interaction.ProcessGizmoFrame(
          { entity }, entity, SelectionTransformOperation::Translate, SelectionTransformSpace::Local,
          ViewportGizmoFrame{ glm::translate(glm::mat4(1.0f), localXInWorld) * initialPivot, true, true, false });

        CHECK(drag.Began);
        CHECK(drag.Updated);
        CheckVector(drag.Before.WorldPosition, { 1.0f, 2.0f, 0.0f });
        CheckVector(drag.After.WorldPosition, glm::vec3(1.0f, 2.0f, 0.0f) + localXInWorld);
        CheckVector(drag.After.LocalPosition, glm::vec3(1.0f, 2.0f, 0.0f) + localXInWorld);
        interaction.Cancel();
    }

    SECTION("World movement of a parented entity is converted back to local position")
    {
        Ref<Scene> scene = CreateRef<Scene>(false);
        Entity parent = scene->CreateEntity("Parent");
        Entity child = scene->CreateEntity("Child");
        const glm::quat quarterTurn = glm::angleAxis(glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        parent.SetWorldTransform(Math::ComposeMatrix({ 10.0f, 0.0f, 0.0f }, quarterTurn, glm::vec3(1.0f)));
        REQUIRE(child.SetParent(parent));
        child.SetPosition({ 2.0f, 0.0f, 0.0f });

        ViewportTransformInteraction interaction;
        const glm::mat4 initialPivot =
          ViewportTransformInteraction::CalculatePivot({ child }, child, SelectionTransformSpace::World);
        const ViewportTransformFrameResult drag = interaction.ProcessGizmoFrame(
          { child }, child, SelectionTransformOperation::Translate, SelectionTransformSpace::World,
          ViewportGizmoFrame{ glm::translate(glm::mat4(1.0f), { 3.0f, 0.0f, 0.0f }) * initialPivot, true, true, false });

        CHECK(drag.Began);
        CHECK(drag.Updated);
        CheckVector(drag.Before.WorldPosition, { 10.0f, 2.0f, 0.0f });
        CheckVector(drag.Before.LocalPosition, { 2.0f, 0.0f, 0.0f });
        CheckVector(drag.After.WorldPosition, { 13.0f, 2.0f, 0.0f });
        CheckVector(drag.After.LocalPosition, { 2.0f, -3.0f, 0.0f });
        interaction.Cancel();
    }
}

TEST_CASE("Escape cancels a gizmo frame and blocks reacquisition until mouse release", "[Editor][Viewport][Transform][Undo]")
{
    Ref<Scene> scene = CreateRef<Scene>(false);
    Entity entity = scene->CreateEntity("Entity");
    entity.SetWorldPosition({ 2.0f, 3.0f, 4.0f });

    UndoRedo::StartUp();
    ViewportTransformInteraction interaction;
    const glm::mat4 initialPivot =
      ViewportTransformInteraction::CalculatePivot({ entity }, entity, SelectionTransformSpace::World);
    const glm::mat4 movedPivot = glm::translate(glm::mat4(1.0f), { 5.0f, 0.0f, 0.0f }) * initialPivot;
    const ViewportTransformFrameResult drag = interaction.ProcessGizmoFrame(
      { entity }, entity, SelectionTransformOperation::Translate, SelectionTransformSpace::World,
      ViewportGizmoFrame{ movedPivot, true, true, false });
    REQUIRE(drag.Updated);

    const ViewportTransformFrameResult cancel = interaction.ProcessGizmoFrame(
      { entity }, entity, SelectionTransformOperation::Translate, SelectionTransformSpace::World,
      ViewportGizmoFrame{ movedPivot, false, true, true });
    CHECK(cancel.GizmoUsing);
    CHECK(cancel.CancelRequested);
    CHECK(cancel.Resolution.State == TransformInteractionCompletion::Cancel);
    CHECK(cancel.BlockedUntilRelease);
    CheckVector(cancel.Before.WorldPosition, { 7.0f, 3.0f, 4.0f });
    CheckVector(cancel.After.WorldPosition, { 2.0f, 3.0f, 4.0f });
    CHECK_FALSE(UndoRedo::Get().CanUndo());

    const ViewportTransformFrameResult suppressed = interaction.ProcessGizmoFrame(
      { entity }, entity, SelectionTransformOperation::Translate, SelectionTransformSpace::World,
      ViewportGizmoFrame{ movedPivot, true, true, false });
    CHECK(suppressed.Manipulated);
    CHECK_FALSE(suppressed.BeginAttempted);
    CHECK_FALSE(suppressed.UpdateAttempted);
    CHECK(suppressed.BlockedUntilRelease);
    CheckVector(entity.GetWorldPosition(), { 2.0f, 3.0f, 4.0f });

    const ViewportTransformFrameResult release = interaction.ProcessGizmoFrame(
      { entity }, entity, SelectionTransformOperation::Translate, SelectionTransformSpace::World,
      ViewportGizmoFrame{ initialPivot, false, false, false });
    CHECK_FALSE(release.BlockedUntilRelease);
    UndoRedo::Shutdown();
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
    REQUIRE(interaction.Begin({ first, second }, first, SelectionTransformOperation::Translate, SelectionTransformSpace::World));
    const glm::mat4 pivot = ViewportTransformInteraction::CalculatePivot({ first, second }, first, SelectionTransformSpace::World);
    REQUIRE(interaction.Update(glm::translate(glm::mat4(1.0f), { 10.0f, 0.0f, 0.0f }) * pivot));
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
    first.SetWorldTransform(firstTransform);
    second.SetWorldTransform(secondTransform);

    UndoRedo::StartUp();
    ViewportTransformInteraction interaction;
    REQUIRE(interaction.Begin({ first, second }, first, SelectionTransformOperation::Translate, SelectionTransformSpace::World));
    const glm::mat4 pivot = ViewportTransformInteraction::CalculatePivot({ first, second }, first, SelectionTransformSpace::World);
    REQUIRE(interaction.Update(glm::translate(glm::mat4(1.0f), { 10.0f, 0.0f, 0.0f }) * pivot));

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

TEST_CASE("Multi-selection translation preserves relative transforms", "[Editor][Viewport][Transform]")
{
    Ref<Scene> scene = CreateRef<Scene>(false);
    Entity first = scene->CreateEntity("First");
    Entity second = scene->CreateEntity("Second");
    first.SetWorldTransform(Math::ComposeMatrix({ 0.0f, 0.0f, 0.0f }, glm::quat(1.0f, 0.0f, 0.0f, 0.0f), { 2.0f, 1.0f, 1.0f }));
    second.SetWorldTransform(
      Math::ComposeMatrix({ 2.0f, 0.0f, 0.0f }, glm::angleAxis(glm::radians(30.0f), glm::vec3(0.0f, 0.0f, 1.0f)), glm::vec3(1.0f)));

    UndoRedo::StartUp();
    ViewportTransformInteraction interaction;
    REQUIRE(interaction.Begin({ first, second }, first, SelectionTransformOperation::Translate, SelectionTransformSpace::World));
    const glm::mat4 initialPivot = interaction.GetCurrentPivot();
    REQUIRE(interaction.Update(glm::translate(glm::mat4(1.0f), { 3.0f, 2.0f, 0.0f }) * initialPivot));

    CheckVector(first.GetWorldPosition(), { 3.0f, 2.0f, 0.0f });
    CheckVector(second.GetWorldPosition(), { 5.0f, 2.0f, 0.0f });
    CheckVector(first.GetWorldScale(), { 2.0f, 1.0f, 1.0f });
    CheckRotation(second.GetWorldRotation(), glm::angleAxis(glm::radians(30.0f), glm::vec3(0.0f, 0.0f, 1.0f)));

    interaction.Cancel();
    UndoRedo::Shutdown();
}

TEST_CASE("Returning a gizmo to its start restores transforms and override state", "[Editor][Viewport][Transform]")
{
    Ref<Scene> scene = CreateRef<Scene>(false);
    Entity entity = scene->CreateEntity("Entity");
    entity.SetWorldPosition({ 2.0f, 3.0f, 4.0f });
    entity.AddComponent<PrefabComponent>();

    UndoRedo::StartUp();
    ViewportTransformInteraction interaction;
    REQUIRE(interaction.Begin({ entity }, entity, SelectionTransformOperation::Translate, SelectionTransformSpace::World));
    const glm::mat4 initialPivot = interaction.GetCurrentPivot();
    REQUIRE(interaction.Update(glm::translate(glm::mat4(1.0f), { 5.0f, 0.0f, 0.0f }) * initialPivot));
    CHECK(entity.GetComponent<PrefabComponent>().IsPropertyOverridden("Transform.Position"));

    REQUIRE(interaction.Update(initialPivot));
    CheckVector(entity.GetWorldPosition(), { 2.0f, 3.0f, 4.0f });
    CHECK_FALSE(entity.GetComponent<PrefabComponent>().IsPropertyOverridden("Transform.Position"));

    const ViewportTransformResolution resolution = interaction.Resolve(false, false);
    CHECK(resolution.State == TransformInteractionCompletion::Commit);
    CHECK(resolution.Action == nullptr);
    UndoRedo::Shutdown();
}

TEST_CASE("Viewport transform undo and redo restore prefab override state", "[Editor][Viewport][Transform][Undo]")
{
    Ref<Scene> scene = CreateRef<Scene>(false);
    Entity entity = scene->CreateEntity("Entity");
    entity.AddComponent<PrefabComponent>();

    UndoRedo::StartUp();
    ViewportTransformInteraction interaction;
    REQUIRE(interaction.Begin({ entity }, entity, SelectionTransformOperation::Translate, SelectionTransformSpace::World));
    REQUIRE(interaction.Update(glm::translate(glm::mat4(1.0f), { 2.0f, 0.0f, 0.0f }) * interaction.GetCurrentPivot()));
    CHECK(entity.GetComponent<PrefabComponent>().IsPropertyOverridden("Transform.Position"));

    const ViewportTransformResolution resolution = interaction.Resolve(false, false);
    REQUIRE(resolution.Action != nullptr);
    UndoRedo::Get().RegisterAction(resolution.Action);
    UndoRedo::Get().Undo();
    CHECK_FALSE(entity.GetComponent<PrefabComponent>().IsPropertyOverridden("Transform.Position"));
    UndoRedo::Get().Redo();
    CHECK(entity.GetComponent<PrefabComponent>().IsPropertyOverridden("Transform.Position"));
    UndoRedo::Shutdown();
}

TEST_CASE("Multi-selection rotation uses the shared pivot", "[Editor][Viewport][Transform]")
{
    Ref<Scene> scene = CreateRef<Scene>(false);
    Entity first = scene->CreateEntity("First");
    Entity second = scene->CreateEntity("Second");
    first.SetWorldPosition({ -1.0f, 0.0f, 0.0f });
    second.SetWorldPosition({ 1.0f, 0.0f, 0.0f });

    UndoRedo::StartUp();
    ViewportTransformInteraction interaction;
    REQUIRE(interaction.Begin({ first, second }, first, SelectionTransformOperation::Rotate, SelectionTransformSpace::World));
    const glm::quat rotation = glm::angleAxis(glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    REQUIRE(interaction.Update(Math::ComposeMatrix({ 0.0f, 0.0f, 0.0f }, rotation, glm::vec3(1.0f))));

    CheckVector(first.GetWorldPosition(), { 0.0f, -1.0f, 0.0f });
    CheckVector(second.GetWorldPosition(), { 0.0f, 1.0f, 0.0f });
    CheckRotation(first.GetWorldRotation(), rotation);
    CheckRotation(second.GetWorldRotation(), rotation);

    interaction.Cancel();
    UndoRedo::Shutdown();
}

TEST_CASE("Local translation and rotation follow the primary axes", "[Editor][Viewport][Transform]")
{
    const glm::quat primaryRotation = glm::angleAxis(glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));

    SECTION("Translation")
    {
        Ref<Scene> scene = CreateRef<Scene>(false);
        Entity first = scene->CreateEntity("First");
        Entity second = scene->CreateEntity("Second");
        first.SetWorldTransform(Math::ComposeMatrix({ -1.0f, 0.0f, 0.0f }, primaryRotation, glm::vec3(1.0f)));
        second.SetWorldPosition({ 1.0f, 0.0f, 0.0f });

        UndoRedo::StartUp();
        ViewportTransformInteraction interaction;
        REQUIRE(interaction.Begin({ first, second }, first, SelectionTransformOperation::Translate, SelectionTransformSpace::Local));
        const glm::vec3 localMove = primaryRotation * glm::vec3(2.0f, 0.0f, 0.0f);
        REQUIRE(interaction.Update(glm::translate(glm::mat4(1.0f), localMove) * interaction.GetCurrentPivot()));

        CheckVector(first.GetWorldPosition(), glm::vec3(-1.0f, 0.0f, 0.0f) + localMove);
        CheckVector(second.GetWorldPosition(), glm::vec3(1.0f, 0.0f, 0.0f) + localMove);

        interaction.Cancel();
        UndoRedo::Shutdown();
    }

    SECTION("Rotation")
    {
        Ref<Scene> scene = CreateRef<Scene>(false);
        Entity first = scene->CreateEntity("First");
        Entity second = scene->CreateEntity("Second");
        first.SetWorldTransform(Math::ComposeMatrix({ -1.0f, 0.0f, 0.0f }, primaryRotation, glm::vec3(1.0f)));
        second.SetWorldPosition({ 1.0f, 0.0f, 0.0f });

        UndoRedo::StartUp();
        ViewportTransformInteraction interaction;
        REQUIRE(interaction.Begin({ first, second }, first, SelectionTransformOperation::Rotate, SelectionTransformSpace::Local));
        const glm::quat localTurn = glm::angleAxis(glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
        const glm::quat nextFrameRotation = glm::normalize(primaryRotation * localTurn);
        const glm::quat worldDelta = glm::normalize(nextFrameRotation * glm::inverse(primaryRotation));
        REQUIRE(interaction.Update(Math::ComposeMatrix(glm::vec3(0.0f), nextFrameRotation, glm::vec3(1.0f))));

        CheckVector(first.GetWorldPosition(), worldDelta * glm::vec3(-1.0f, 0.0f, 0.0f));
        CheckVector(second.GetWorldPosition(), worldDelta * glm::vec3(1.0f, 0.0f, 0.0f));
        CheckRotation(first.GetWorldRotation(), nextFrameRotation);
        CheckRotation(second.GetWorldRotation(), worldDelta);

        interaction.Cancel();
        UndoRedo::Shutdown();
    }
}

TEST_CASE("Multi-selection scale distinguishes world and local axes", "[Editor][Viewport][Transform]")
{
    const glm::quat quarterTurn = glm::angleAxis(glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));

    SECTION("Local scale follows the primary orientation")
    {
        Ref<Scene> scene = CreateRef<Scene>(false);
        Entity first = scene->CreateEntity("First");
        Entity second = scene->CreateEntity("Second");
        first.SetWorldTransform(Math::ComposeMatrix({ 0.0f, -1.0f, 0.0f }, quarterTurn, glm::vec3(1.0f)));
        second.SetWorldTransform(Math::ComposeMatrix({ 0.0f, 1.0f, 0.0f }, quarterTurn, glm::vec3(1.0f)));

        UndoRedo::StartUp();
        ViewportTransformInteraction interaction;
        REQUIRE(interaction.Begin({ first, second }, first, SelectionTransformOperation::Scale, SelectionTransformSpace::Local));
        REQUIRE(interaction.Update(Math::ComposeMatrix({ 0.0f, 0.0f, 0.0f }, quarterTurn, { 2.0f, 1.0f, 1.0f })));

        CheckVector(first.GetWorldPosition(), { 0.0f, -2.0f, 0.0f });
        CheckVector(second.GetWorldPosition(), { 0.0f, 2.0f, 0.0f });
        CheckVector(first.GetWorldScale(), { 2.0f, 1.0f, 1.0f });
        CheckVector(second.GetWorldScale(), { 2.0f, 1.0f, 1.0f });

        interaction.Cancel();
        UndoRedo::Shutdown();
    }

    SECTION("World scale follows world axes without rotating entities")
    {
        Ref<Scene> scene = CreateRef<Scene>(false);
        Entity first = scene->CreateEntity("First");
        Entity second = scene->CreateEntity("Second");
        first.SetWorldTransform(Math::ComposeMatrix({ 0.0f, -1.0f, 0.0f }, quarterTurn, glm::vec3(1.0f)));
        second.SetWorldTransform(Math::ComposeMatrix({ 0.0f, 1.0f, 0.0f }, quarterTurn, glm::vec3(1.0f)));

        UndoRedo::StartUp();
        ViewportTransformInteraction interaction;
        REQUIRE(interaction.Begin({ first, second }, first, SelectionTransformOperation::Scale, SelectionTransformSpace::World));
        REQUIRE(interaction.Update(Math::ComposeMatrix({ 0.0f, 0.0f, 0.0f }, glm::quat(1.0f, 0.0f, 0.0f, 0.0f), { 2.0f, 1.0f, 1.0f })));

        CheckVector(first.GetWorldPosition(), { 0.0f, -1.0f, 0.0f });
        CheckVector(second.GetWorldPosition(), { 0.0f, 1.0f, 0.0f });
        CheckVector(first.GetWorldScale(), { 1.0f, 2.0f, 1.0f });
        CheckVector(second.GetWorldScale(), { 1.0f, 2.0f, 1.0f });
        CheckRotation(first.GetWorldRotation(), quarterTurn);
        CheckRotation(second.GetWorldRotation(), quarterTurn);

        interaction.Cancel();
        UndoRedo::Shutdown();
    }
}

TEST_CASE("Selected descendants are transformed once with their selected parent", "[Editor][Viewport][Transform]")
{
    Ref<Scene> scene = CreateRef<Scene>(false);
    Entity parent = scene->CreateEntity("Parent");
    Entity child = scene->CreateEntity("Child");
    child.SetPosition({ 1.0f, 0.0f, 0.0f });
    REQUIRE(child.SetParent(parent));

    UndoRedo::StartUp();
    ViewportTransformInteraction interaction;
    REQUIRE(interaction.Begin({ child, parent }, child, SelectionTransformOperation::Translate, SelectionTransformSpace::World));
    REQUIRE(interaction.Update(glm::translate(glm::mat4(1.0f), { 1.0f, 0.0f, 0.0f }) * interaction.GetCurrentPivot()));

    CheckVector(parent.GetWorldPosition(), { 1.0f, 0.0f, 0.0f });
    CheckVector(child.GetWorldPosition(), { 2.0f, 0.0f, 0.0f });

    interaction.Cancel();
    UndoRedo::Shutdown();
}
