#include "Editor/SelectionProperty.h"
#include "Editor/ComponentUndoSnapshot.h"

#include "Crowny/Scene/Scene.h"

#include <catch2/catch_test_macros.hpp>

using namespace Crowny;

TEST_CASE("Selection properties aggregate only accessible entities", "[Editor][Inspector]")
{
    Ref<Scene> scene = CreateRef<Scene>(false);
    Entity first = scene->CreateEntity("First");
    Entity second = scene->CreateEntity("Second");
    Entity withoutLight = scene->CreateEntity("Without light");
    first.AddComponent<LightComponent>().Intensity = 2.0f;
    second.AddComponent<LightComponent>().Intensity = 4.0f;

    const Vector<Entity> entities{ {}, first, withoutLight, second };
    const auto intensity = InspectorSelection(entities, "Light").Bind("Intensity", &LightComponent::Intensity);
    const SelectionPropertyValue<float> value = intensity.Read();

    REQUIRE(value.Primary.has_value());
    CHECK(*value.Primary == 2.0f);
    CHECK(value.Mixed);
    CHECK(value.TargetCount == 2u);
}

TEST_CASE("Selection property writes skip equal values and track prefab overrides", "[Editor][Inspector]")
{
    Ref<Scene> scene = CreateRef<Scene>(false);
    Entity first = scene->CreateEntity("First");
    Entity second = scene->CreateEntity("Second");
    first.AddComponent<LightComponent>().Intensity = 2.0f;
    second.AddComponent<LightComponent>().Intensity = 4.0f;
    first.AddComponent<PrefabComponent>();
    second.AddComponent<PrefabComponent>();

    const Vector<Entity> entities{ first, second };
    const auto intensity = InspectorSelection(entities, "Light").Bind("Intensity", &LightComponent::Intensity);
    const SelectionPropertyWrite write = intensity.Assign(2.0f);

    CHECK(write.TargetCount == 2u);
    CHECK(write.ChangedCount == 1u);
    CHECK(first.GetComponent<LightComponent>().Intensity == 2.0f);
    CHECK(second.GetComponent<LightComponent>().Intensity == 2.0f);
    CHECK_FALSE(first.GetComponent<PrefabComponent>().IsPropertyOverridden("Light.Intensity"));
    CHECK(second.GetComponent<PrefabComponent>().IsPropertyOverridden("Light.Intensity"));
}

TEST_CASE("Projected selection properties preserve unrelated values per entity", "[Editor][Inspector]")
{
    Ref<Scene> scene = CreateRef<Scene>(false);
    Entity first = scene->CreateEntity("First");
    Entity second = scene->CreateEntity("Second");
    first.AddComponent<LightComponent>().Color = { 1.0f, 2.0f, 3.0f };
    second.AddComponent<LightComponent>().Color = { 4.0f, 5.0f, 6.0f };

    const Vector<Entity> entities{ first, second };
    const auto color = InspectorSelection(entities, "Light").Bind("Color", &LightComponent::Color);
    const SelectionPropertyValue<float> green = color.Element(1u).Read();
    REQUIRE(green.Primary.has_value());
    CHECK(*green.Primary == 2.0f);
    CHECK(green.Mixed);

    const SelectionPropertyWrite write = color.Element(1u).Assign(9.0f);
    CHECK(write.ChangedCount == 2u);
    CHECK(first.GetComponent<LightComponent>().Color == glm::vec3(1.0f, 9.0f, 3.0f));
    CHECK(second.GetComponent<LightComponent>().Color == glm::vec3(4.0f, 9.0f, 6.0f));
}

TEST_CASE("Transform selection axis resets preserve other axes and invalidate descendants", "[Editor][Inspector]")
{
    Ref<Scene> scene = CreateRef<Scene>(false);
    Entity first = scene->CreateEntity("First");
    Entity second = scene->CreateEntity("Second");
    Entity child = scene->CreateEntity("Child");
    child.SetParent(first);
    first.SetPosition({ 2.0f, 3.0f, 4.0f });
    second.SetPosition({ -5.0f, 6.0f, 7.0f });
    child.SetPosition({ 1.0f, 0.0f, 0.0f });
    first.AddComponent<PrefabComponent>();
    second.AddComponent<PrefabComponent>();

    REQUIRE(child.GetWorldPosition() == glm::vec3(3.0f, 3.0f, 4.0f));

    const Vector<Entity> entities{ first, second };
    const auto position = InspectorSelection(entities, "Transform")
                            .Components<TransformComponent>()
                            .Bind(
                              "Position", [](const TransformComponent& transform) { return transform.GetLocalTransform().GetPosition(); },
                              [](TransformComponent&, const glm::vec3& value, Entity entity) { entity.SetPosition(value); });
    const SelectionPropertyWrite write = position.Element(0u).Assign(0.0f);

    CHECK(write.TargetCount == 2u);
    CHECK(write.ChangedCount == 2u);
    CHECK(first.GetLocalPosition() == glm::vec3(0.0f, 3.0f, 4.0f));
    CHECK(second.GetLocalPosition() == glm::vec3(0.0f, 6.0f, 7.0f));
    CHECK(child.GetWorldPosition() == glm::vec3(1.0f, 3.0f, 4.0f));
    CHECK(first.GetComponent<PrefabComponent>().IsPropertyOverridden("Transform.Position"));
    CHECK(second.GetComponent<PrefabComponent>().IsPropertyOverridden("Transform.Position"));

    first.SetScale({ 2.0f, 3.0f, 4.0f });
    second.SetScale({ 5.0f, 6.0f, 7.0f });
    REQUIRE(child.GetWorldPosition() == glm::vec3(2.0f, 3.0f, 4.0f));

    const auto scale = InspectorSelection(entities, "Transform")
                         .Components<TransformComponent>()
                         .Bind(
                           "Scale", [](const TransformComponent& transform) { return transform.GetLocalTransform().GetScale(); },
                           [](TransformComponent&, const glm::vec3& value, Entity entity) { entity.SetScale(value); });
    const SelectionPropertyWrite scaleWrite = scale.Element(0u).Assign(1.0f);

    CHECK(scaleWrite.TargetCount == 2u);
    CHECK(scaleWrite.ChangedCount == 2u);
    CHECK(first.GetLocalScale() == glm::vec3(1.0f, 3.0f, 4.0f));
    CHECK(second.GetLocalScale() == glm::vec3(1.0f, 6.0f, 7.0f));
    CHECK(child.GetWorldPosition() == glm::vec3(1.0f, 3.0f, 4.0f));
    CHECK(first.GetComponent<PrefabComponent>().IsPropertyOverridden("Transform.Scale"));
    CHECK(second.GetComponent<PrefabComponent>().IsPropertyOverridden("Transform.Scale"));
}

TEST_CASE("Transform selection axis reset undo restores values and prefab overrides as one action", "[Editor][Inspector][Undo]")
{
    Ref<Scene> scene = CreateRef<Scene>(false);
    Entity first = scene->CreateEntity("First");
    Entity second = scene->CreateEntity("Second");
    first.SetPosition({ 2.0f, 3.0f, 4.0f });
    second.SetPosition({ -5.0f, 6.0f, 7.0f });
    first.AddComponent<PrefabComponent>().MarkOverridden("Transform.Rotation");
    second.AddComponent<PrefabComponent>();

    const Vector<Entity> entities{ first, second };
    Ref<ComponentUndoSnapshot<TransformComponent>> snapshots = CreateRef<ComponentUndoSnapshot<TransformComponent>>();
    snapshots->Capture(entities);
    const auto position = InspectorSelection(entities, "Transform")
                            .Components<TransformComponent>()
                            .Bind(
                              "Position", [](const TransformComponent& transform) { return transform.GetLocalTransform().GetPosition(); },
                              [](TransformComponent&, const glm::vec3& value, Entity entity) { entity.SetPosition(value); });
    REQUIRE(position.Element(0u).Assign(0.0f).ChangedCount == 2u);

    Ref<UndoAction> action = snapshots->Build();
    REQUIRE(action != nullptr);
    snapshots->CompleteFrame();
    UndoRedo::StartUp();
    UndoRedo::Get().RegisterAction(action);
    CHECK(first.GetComponent<PrefabComponent>().IsPropertyOverridden("Transform.Position"));
    CHECK(second.GetComponent<PrefabComponent>().IsPropertyOverridden("Transform.Position"));

    UndoRedo::Get().Undo();
    CHECK(first.GetLocalPosition() == glm::vec3(2.0f, 3.0f, 4.0f));
    CHECK(second.GetLocalPosition() == glm::vec3(-5.0f, 6.0f, 7.0f));
    CHECK_FALSE(first.GetComponent<PrefabComponent>().IsPropertyOverridden("Transform.Position"));
    CHECK_FALSE(second.GetComponent<PrefabComponent>().IsPropertyOverridden("Transform.Position"));
    CHECK(first.GetComponent<PrefabComponent>().IsPropertyOverridden("Transform.Rotation"));
    CHECK_FALSE(UndoRedo::Get().CanUndo());

    UndoRedo::Get().Redo();
    CHECK(first.GetLocalPosition() == glm::vec3(0.0f, 3.0f, 4.0f));
    CHECK(second.GetLocalPosition() == glm::vec3(0.0f, 6.0f, 7.0f));
    CHECK(first.GetComponent<PrefabComponent>().IsPropertyOverridden("Transform.Position"));
    CHECK(second.GetComponent<PrefabComponent>().IsPropertyOverridden("Transform.Position"));
    CHECK(first.GetComponent<PrefabComponent>().IsPropertyOverridden("Transform.Rotation"));
    UndoRedo::Shutdown();
}

TEST_CASE("Nested member properties preserve each parent value and use the nested override path", "[Editor][Inspector]")
{
    Ref<Scene> scene = CreateRef<Scene>(false);
    Entity first = scene->CreateEntity("First");
    Entity second = scene->CreateEntity("Second");
    first.AddComponent<LightComponent>();
    second.AddComponent<LightComponent>();
    first.GetComponent<LightComponent>().Shadows.Bias = 1.0f;
    first.GetComponent<LightComponent>().Shadows.NormalBias = 2.0f;
    second.GetComponent<LightComponent>().Shadows.Bias = 3.0f;
    second.GetComponent<LightComponent>().Shadows.NormalBias = 4.0f;
    first.AddComponent<PrefabComponent>();
    second.AddComponent<PrefabComponent>();

    const Vector<Entity> entities{ first, second };
    const auto lights = InspectorSelection(entities, "Light").Components<LightComponent>();
    const auto shadows = lights.Bind("Shadows", &LightComponent::Shadows);
    const SelectionPropertyWrite write = shadows.Member("Shadows.Bias", &LightShadowSettings::Bias).Assign(8.0f);

    CHECK(write.ChangedCount == 2u);
    CHECK(first.GetComponent<LightComponent>().Shadows.Bias == 8.0f);
    CHECK(second.GetComponent<LightComponent>().Shadows.Bias == 8.0f);
    CHECK(first.GetComponent<LightComponent>().Shadows.NormalBias == 2.0f);
    CHECK(second.GetComponent<LightComponent>().Shadows.NormalBias == 4.0f);
    CHECK(first.GetComponent<PrefabComponent>().IsPropertyOverridden("Light.Shadows.Bias"));
    CHECK(second.GetComponent<PrefabComponent>().IsPropertyOverridden("Light.Shadows.Bias"));
}

TEST_CASE("Selection properties support guarded custom accessors", "[Editor][Inspector]")
{
    Ref<Scene> scene = CreateRef<Scene>(false);
    Entity accepted = scene->CreateEntity("Accepted");
    Entity rejected = scene->CreateEntity("Rejected");
    Entity other = scene->CreateEntity("Other");
    accepted.AddComponent<LightComponent>().Range = 3.0f;
    rejected.AddComponent<LightComponent>().Range = 4.0f;
    accepted.AddComponent<PrefabComponent>();
    rejected.AddComponent<PrefabComponent>();

    const Vector<Entity> entities{ accepted, other, rejected };
    const auto range = InspectorSelection(entities, "Light")
                         .BindWhere(
                           "Range", [](Entity entity) { return entity.GetComponent<LightComponent>().Range; },
                           [rejected](Entity entity, float value) {
                               if (entity == rejected)
                                   return false;
                               entity.GetComponent<LightComponent>().Range = value;
                               return true;
                           },
                           [](Entity entity) { return entity.HasComponent<LightComponent>(); });

    const SelectionPropertyWrite write = range.Assign(8.0f);
    CHECK(write.TargetCount == 2u);
    CHECK(write.ChangedCount == 1u);
    CHECK(accepted.GetComponent<LightComponent>().Range == 8.0f);
    CHECK(rejected.GetComponent<LightComponent>().Range == 4.0f);
    CHECK(accepted.GetComponent<PrefabComponent>().IsPropertyOverridden("Light.Range"));
    CHECK_FALSE(rejected.GetComponent<PrefabComponent>().IsPropertyOverridden("Light.Range"));
}

TEST_CASE("Component selection properties accept component methods and entity-aware setters", "[Editor][Inspector]")
{
    Ref<Scene> scene = CreateRef<Scene>(false);
    Entity first = scene->CreateEntity("First");
    Entity second = scene->CreateEntity("Second");
    Entity withoutBody = scene->CreateEntity("Without body");
    first.AddComponent<Rigidbody2DComponent>();
    second.AddComponent<Rigidbody2DComponent>();

    const Vector<Entity> entities{ first, withoutBody, second };
    const auto bodies = InspectorSelection(entities, "Rigidbody 2D").Components<Rigidbody2DComponent>();
    const auto layer = bodies.Bind("Layer", &Rigidbody2DComponent::GetLayerMask, &Rigidbody2DComponent::SetLayerMask);

    const SelectionPropertyWrite write = layer.Assign(7u);
    CHECK(write.TargetCount == 2u);
    CHECK(write.ChangedCount == 2u);
    CHECK(first.GetComponent<Rigidbody2DComponent>().GetLayerMask() == 7u);
    CHECK(second.GetComponent<Rigidbody2DComponent>().GetLayerMask() == 7u);
    CHECK_FALSE(layer.Read().Mixed);
}

TEST_CASE("Selection property writes report the effective value after setter normalization", "[Editor][Inspector]")
{
    Ref<Scene> scene = CreateRef<Scene>(false);
    Entity entity = scene->CreateEntity("Light");
    entity.AddComponent<LightComponent>().Range = 0.0f;
    entity.AddComponent<PrefabComponent>();

    const Vector<Entity> entities{ entity };
    const auto range = InspectorSelection(entities, "Light")
                         .Components<LightComponent>()
                         .Bind(
                           "Range", [](const LightComponent& light) { return light.Range; },
                           [](LightComponent& light, float value) { light.Range = std::max(value, 0.0f); });

    const SelectionPropertyWrite write = range.Assign(-1.0f);
    CHECK(write.TargetCount == 1u);
    CHECK(write.ChangedCount == 0u);
    CHECK(entity.GetComponent<LightComponent>().Range == 0.0f);
    CHECK_FALSE(entity.GetComponent<PrefabComponent>().IsPropertyOverridden("Light.Range"));
}
