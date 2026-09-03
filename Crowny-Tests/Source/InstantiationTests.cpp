#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "Crowny/Assets/AssetManager.h"
#include "Crowny/Ecs/Components.h"
#include "Crowny/Ecs/Entity.h"
#include "Crowny/Scene/EntityInstantiation.h"
#include "Crowny/Scene/Prefab.h"
#include "Crowny/Scene/Scene.h"
#include "Crowny/Scene/ScriptRuntime.h"
#include "Crowny/Scripting/ScriptTypeIdentity.h"

using namespace Crowny;

namespace
{
    struct CapturedPrefab
    {
        Ref<Prefab> Asset;
        AssetHandle<Prefab> Handle;
        UUID RootUuid;
        UUID ChildUuid;
    };

    // Builds a two-level prefab (root sprite + child sprite) and wraps it in an asset handle.
    CapturedPrefab CreateTurretPrefab(AssetManager& manager, const UUID& assetUuid)
    {
        Ref<Scene> source = CreateRef<Scene>(false);
        Entity sourceRoot = source->CreateEntity("Turret");
        Entity sourceChild = source->CreateEntity("Barrel");
        sourceChild.SetParent(sourceRoot);
        sourceRoot.SetPosition({ 3.0f, 4.0f, 5.0f });
        sourceRoot.AddComponent<SpriteRendererComponent>().Color = { 1.0f, 0.5f, 0.25f, 1.0f };
        sourceChild.AddComponent<SpriteRendererComponent>().Color = { 0.0f, 1.0f, 0.0f, 1.0f };

        const UUID rootUuid = sourceRoot.GetUuid();
        const UUID childUuid = sourceChild.GetUuid();

        Ref<Prefab> prefab = CreateRef<Prefab>();
        prefab->CaptureFromEntity(*source, sourceRoot);

        CapturedPrefab captured;
        captured.Asset = prefab;
        captured.Handle = static_asset_cast<Prefab>(manager.CreateAssetHandle(prefab, assetUuid));
        captured.RootUuid = rootUuid;
        captured.ChildUuid = childUuid;
        return captured;
    }
} // namespace

TEST_CASE("Prefab instantiation creates a fresh linked hierarchy", "[Ecs][Prefab][Instantiate]")
{
    AssetManager manager;
    const UUID prefabAssetUuid = UuidGenerator::Generate();
    const CapturedPrefab captured = CreateTurretPrefab(manager, prefabAssetUuid);

    Ref<Scene> target = CreateRef<Scene>(false);
    Entity instance = EntityInstantiator::InstantiatePrefab(*target, captured.Handle);

    REQUIRE(instance);
    CHECK(instance.GetScene() == target.get());
    CHECK(instance.GetName() == "Turret");
    CHECK(instance.GetUuid() != captured.RootUuid);

    REQUIRE(instance.GetChildCount() == 1);
    Entity instanceChild = instance.GetChild(0);
    CHECK(instanceChild.GetName() == "Barrel");
    CHECK(instanceChild.GetParent() == instance);
    CHECK(instanceChild.GetUuid() != captured.ChildUuid);

    // Component values are copied from the prefab definition.
    REQUIRE(instance.HasComponent<SpriteRendererComponent>());
    CHECK(instance.GetComponent<SpriteRendererComponent>().Color == glm::vec4(1.0f, 0.5f, 0.25f, 1.0f));
    REQUIRE(instanceChild.HasComponent<SpriteRendererComponent>());
    CHECK(instanceChild.GetComponent<SpriteRendererComponent>().Color == glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));

    // The saved root transform becomes the world transform at the scene root.
    CHECK(instance.GetLocalPosition() == glm::vec3(3.0f, 4.0f, 5.0f));
    CHECK(instance.GetWorldPosition() == glm::vec3(3.0f, 4.0f, 5.0f));

    // Every instance entity links back to the prefab asset and its definition entity.
    REQUIRE(instance.HasComponent<PrefabComponent>());
    CHECK(instance.GetComponent<PrefabComponent>().PrefabAssetUuid == prefabAssetUuid);
    CHECK(instance.GetComponent<PrefabComponent>().PrefabEntityUuid == captured.RootUuid);
    REQUIRE(instanceChild.HasComponent<PrefabComponent>());
    CHECK(instanceChild.GetComponent<PrefabComponent>().PrefabAssetUuid == prefabAssetUuid);
    CHECK(instanceChild.GetComponent<PrefabComponent>().PrefabEntityUuid == captured.ChildUuid);

    // Instantiating twice yields independent hierarchies and leaves the definition untouched.
    Entity second = EntityInstantiator::InstantiatePrefab(*target, captured.Handle);
    REQUIRE(second);
    CHECK(second.GetUuid() != instance.GetUuid());
    CHECK(second.GetComponent<PrefabComponent>().PrefabEntityUuid == captured.RootUuid);
    REQUIRE(captured.Asset->GetInternalScene());
    CHECK(captured.Asset->GetRootEntity().GetChildCount() == 1);
}

TEST_CASE("Prefab instantiation honors parent and world pose options", "[Ecs][Prefab][Instantiate]")
{
    AssetManager manager;
    const CapturedPrefab captured = CreateTurretPrefab(manager, UuidGenerator::Generate());

    Ref<Scene> target = CreateRef<Scene>(false);
    Entity parent = target->CreateEntity("Parent");
    parent.SetPosition({ 10.0f, 0.0f, 0.0f });

    SECTION("Parenting preserves the prefab's world transform")
    {
        EntityInstantiateOptions options;
        options.Parent = parent;
        Entity instance = EntityInstantiator::InstantiatePrefab(*target, captured.Handle, options);
        REQUIRE(instance);
        CHECK(instance.GetParent() == parent);
        CHECK(instance.GetWorldPosition() == glm::vec3(3.0f, 4.0f, 5.0f));
        CHECK(instance.GetLocalPosition() == glm::vec3(-7.0f, 4.0f, 5.0f));
    }

    SECTION("World pose overrides are applied to the instance root")
    {
        EntityInstantiateOptions options;
        options.Parent = parent;
        options.ApplyWorldPosition = true;
        options.WorldPosition = { 0.0f, 5.0f, 0.0f };
        options.ApplyWorldRotation = true;
        options.WorldRotation = glm::angleAxis(glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        Entity instance = EntityInstantiator::InstantiatePrefab(*target, captured.Handle, options);
        REQUIRE(instance);
        CHECK(instance.GetParent() == parent);
        CHECK(instance.GetWorldPosition() == glm::vec3(0.0f, 5.0f, 0.0f));
        CHECK(instance.GetChildCount() == 1);
    }

    SECTION("Invalid handles are rejected")
    {
        AssetHandle<Prefab> empty;
        CHECK(EntityInstantiator::InstantiatePrefab(*target, empty) == Entity::Invalid);
    }
}

TEST_CASE("Entity instantiation deep-copies hierarchies with fresh identities", "[Ecs][Instantiate]")
{
    Ref<Scene> scene = CreateRef<Scene>(false);
    Entity parent = scene->CreateEntity("Parent");
    Entity child = scene->CreateEntity("Child");
    child.SetParent(parent);
    parent.SetPosition({ 10.0f, 0.0f, 0.0f });
    child.SetPosition({ 0.0f, 2.0f, 0.0f });
    parent.AddComponent<LightComponent>().Intensity = 0.75f;
    child.AddComponent<AudioSourceComponent>().SetVolume(0.4f);
    const UUID parentUuid = parent.GetUuid();
    const UUID childUuid = child.GetUuid();
    const float childWorldY = child.GetWorldPosition().y;

    SECTION("Deep copy keeps values and hierarchy, but not identities")
    {
        Entity clone = EntityInstantiator::InstantiateEntity(*scene, parent);
        REQUIRE(clone);
        CHECK(clone.GetScene() == scene.get());
        CHECK(clone.GetUuid() != parentUuid);
        CHECK(clone.GetName() == "Parent");
        CHECK(clone.GetLocalPosition() == glm::vec3(10.0f, 0.0f, 0.0f));
        CHECK(clone.HasComponent<LightComponent>());
        CHECK(clone.GetComponent<LightComponent>().Intensity == 0.75f);

        REQUIRE(clone.GetChildCount() == 1);
        Entity cloneChild = clone.GetChild(0);
        CHECK(cloneChild.GetUuid() != childUuid);
        CHECK(cloneChild.GetParent() == clone);
        CHECK(cloneChild.GetWorldPosition().y == childWorldY);
        REQUIRE(cloneChild.HasComponent<AudioSourceComponent>());
        CHECK(cloneChild.GetComponent<AudioSourceComponent>().GetVolume() == 0.4f);

        // The source subtree is untouched.
        CHECK(parent.GetUuid() == parentUuid);
        REQUIRE(parent.GetChildCount() == 1);
        CHECK(parent.GetChild(0).GetUuid() == childUuid);
        CHECK(scene->TryGetEntityFromUuid(childUuid) == child);
    }

    SECTION("A different parent keeps the source's world transform")
    {
        Entity other = scene->CreateEntity("Other");
        other.SetPosition({ 0.0f, 20.0f, 0.0f });
        EntityInstantiateOptions options;
        options.Parent = other;
        Entity clone = EntityInstantiator::InstantiateEntity(*scene, parent, options);
        REQUIRE(clone);
        CHECK(clone.GetParent() == other);
        CHECK(clone.GetWorldPosition() == glm::vec3(10.0f, 0.0f, 0.0f));
    }

    SECTION("The scene root cannot be instantiated")
    {
        CHECK(EntityInstantiator::InstantiateEntity(*scene, scene->GetRootEntity()) == Entity::Invalid);
    }

    SECTION("Duplicating an invalid source fails cleanly")
    {
        CHECK(EntityInstantiator::InstantiateEntity(*scene, Entity::Invalid) == Entity::Invalid);
    }
}

TEST_CASE("Instantiated script components stay dormant outside play mode", "[Ecs][Instantiate][Script]")
{
    Ref<Scene> scene = CreateRef<Scene>(false);
    Entity scripted = scene->CreateEntity("Scripted");
    ManagedScript& script = scripted.AddComponent<ManagedScriptComponent>().Scripts.emplace_back(ScriptTypeIdentity{ "Game", "Sandbox", "Spinner" });
    const uint64_t sourceInstanceId = script.InstanceId;

    Entity clone = EntityInstantiator::InstantiateEntity(*scene, scripted);
    REQUIRE(clone);
    REQUIRE(clone.HasComponent<ManagedScriptComponent>());
    REQUIRE(clone.GetComponent<ManagedScriptComponent>().Scripts.size() == 1);
    ManagedScript& clonedScript = clone.GetComponent<ManagedScriptComponent>().Scripts[0];

    // Copies receive a fresh script identity and are never awakened outside play mode.
    CHECK(clonedScript.InstanceId != sourceInstanceId);
    CHECK(clonedScript.GetRuntimeHandle() == ScriptInstanceHandle{});
    CHECK_FALSE(ScriptRuntime::IsScriptAwake(clonedScript));
}
