#include <catch2/catch_test_macros.hpp>

#include "Editor/EditorScenePersistence.h"

#include "Crowny/Ecs/Components.h"
#include "Crowny/Ecs/Entity.h"
#include "Crowny/Memory/AllocationCounter.h"
#include "Crowny/Scene/Scene.h"
#include "Crowny/Scene/SceneManager.h"
#include "Crowny/Serialization/SceneSerializer.h"

#include <fstream>

using namespace Crowny;

TEST_CASE("Play and simulation restore the untouched edit scene", "[Scene][Lifecycle]")
{
    SceneManager manager;
    Ref<Scene> editScene = CreateRef<Scene>("Edit scene");
    editScene->SetEditorScene(true);
    Entity source = editScene->CreateEntity("Source");
    source.GetTransform().SetPosition({ 1.0f, 2.0f, 3.0f });
    const UUID sourceId = source.GetUuid();

    manager.SetActiveScene(editScene);
    REQUIRE(manager.BeginPlay(sourceId) == SceneOperationStatus::Completed);
    REQUIRE(manager.GetExecutionState() == SceneExecutionState::Play);
    Ref<Scene> playScene = manager.GetActiveScene();
    REQUIRE(playScene != editScene);
    CHECK(playScene->IsRuntimeActive());
    REQUIRE(playScene->TryGetEntityFromUuid(sourceId));
    playScene->TryGetEntityFromUuid(sourceId).GetTransform().SetPosition({ 9.0f, 8.0f, 7.0f });

    REQUIRE(manager.Stop() == SceneOperationStatus::Completed);
    CHECK(manager.GetActiveScene() == editScene);
    CHECK_FALSE(editScene->IsRuntimeActive());
    CHECK(manager.GetEditSelection() == sourceId);
    CHECK(editScene->TryGetEntityFromUuid(sourceId).GetTransform().GetLocalTransform().GetPosition() == glm::vec3(1.0f, 2.0f, 3.0f));

    REQUIRE(manager.BeginSimulation(sourceId) == SceneOperationStatus::Completed);
    CHECK(manager.GetExecutionState() == SceneExecutionState::Simulate);
    CHECK(manager.GetActiveScene() != editScene);
    CHECK(manager.GetActiveScene()->IsSimulating());
    REQUIRE(manager.Stop() == SceneOperationStatus::Completed);
    CHECK(manager.GetActiveScene() == editScene);
    CHECK(manager.GetExecutionState() == SceneExecutionState::Edit);
}

TEST_CASE("Runtime clones create fresh component and script identities", "[Scene][Lifecycle][Scripting]")
{
    Ref<Scene> editScene = CreateRef<Scene>("Identity scene");
    Entity entity = editScene->CreateEntity("Script host");
    ManagedScriptComponent& scripts = entity.AddComponent<ManagedScriptComponent>();
    scripts.Scripts.emplace_back(ScriptTypeIdentity{ "GameAssembly", "Tests", "IdentityBehaviour" });
    const uint64_t editComponentId = scripts.InstanceId;
    const uint64_t editScriptId = scripts.Scripts.front().InstanceId;

    uint64_t previousRuntimeScriptId = 0;
    for (uint32_t iteration = 0; iteration < 3; ++iteration)
    {
        Ref<Scene> runtimeScene = CreateRef<Scene>(*editScene);
        const Entity runtimeEntity = runtimeScene->GetEntityFromUuid(entity.GetUuid());
        const ManagedScriptComponent& runtimeScripts = runtimeEntity.GetComponent<ManagedScriptComponent>();
        CHECK(runtimeScripts.InstanceId != editComponentId);
        CHECK(runtimeScripts.Scripts.front().InstanceId != editScriptId);
        CHECK(runtimeScripts.Scripts.front().InstanceId != previousRuntimeScriptId);
        CHECK_FALSE(runtimeScripts.Scripts.front().GetRuntimeHandle().IsValid());
        previousRuntimeScriptId = runtimeScripts.Scripts.front().InstanceId;
    }
}

TEST_CASE("Scene changes requested in callback scopes are deferred", "[Scene][Lifecycle]")
{
    SceneManager manager;
    Ref<Scene> first = CreateRef<Scene>("First");
    Ref<Scene> second = CreateRef<Scene>("Second");
    const UUID firstId = UuidGenerator::Generate();
    const UUID secondId = UuidGenerator::Generate();
    manager.SetActiveScene(first, firstId);

    {
        SceneManager::CallbackScope callbackScope = manager.DeferSceneChanges();
        manager.SetActiveScene(second, secondId);
        CHECK(manager.GetActiveScene() == first);
    }

    CHECK(manager.GetActiveScene() == second);
    CHECK(manager.GetActiveSceneId() == secondId);
    CHECK(manager.GetLoadedScene(firstId) == first);
    CHECK(manager.GetLoadedScene(secondId) == second);

    SceneOperationStatus callbackStatus = SceneOperationStatus::Failed;
    const SceneManager::ListenerId listener = manager.AddLifecycleListener([&](const SceneLifecycleEvent& event) {
        if (event.Type == SceneLifecycleEventType::ActiveChanged && event.SceneId == firstId)
            callbackStatus = manager.SetActiveScene(secondId);
    });
    REQUIRE(manager.SetActiveScene(firstId) == SceneOperationStatus::Completed);
    CHECK(callbackStatus == SceneOperationStatus::Deferred);
    CHECK(manager.GetActiveSceneId() == secondId);
    manager.RemoveLifecycleListener(listener);
}

TEST_CASE("Loaded scene enumeration is sorted and allocation-free after mutation", "[Scene][Lifecycle][Memory]")
{
    SceneManager manager;
    const UUID firstId(1, 0, 0, 0);
    const UUID secondId(2, 0, 0, 0);
    const UUID thirdId(3, 0, 0, 0);
    manager.SetActiveScene(CreateRef<Scene>("Third"), thirdId);
    manager.SetActiveScene(CreateRef<Scene>("First"), firstId);
    manager.SetActiveScene(CreateRef<Scene>("Second"), secondId);

    const std::span<const UUID> loaded = manager.GetLoadedScenes();
    REQUIRE(loaded.size() == 3);
    CHECK(loaded[0] == firstId);
    CHECK(loaded[1] == secondId);
    CHECK(loaded[2] == thirdId);

    uint32_t observed = 0;
    const Memory::ThreadAllocationSnapshot before = Memory::GetThreadAllocationSnapshot();
    for (uint32_t frame = 0; frame < 120; ++frame)
    {
        const std::span<const UUID> scenes = manager.GetLoadedScenes();
        observed += static_cast<uint32_t>(scenes.size());
    }
    const Memory::ThreadAllocationSnapshot delta = Memory::GetThreadAllocationDelta(before, Memory::GetThreadAllocationSnapshot());

    CHECK(observed == 360);
    CHECK(delta.AllocationCount == 0);

    manager.SetActiveScene(CreateRef<Scene>("Second replacement"), secondId);
    CHECK(manager.GetLoadedScenes().size() == 3);
    REQUIRE(manager.UnloadScene(secondId) == SceneOperationStatus::Completed);
    const std::span<const UUID> remaining = manager.GetLoadedScenes();
    REQUIRE(remaining.size() == 2);
    CHECK(remaining[0] == firstId);
    CHECK(remaining[1] == thirdId);
}

TEST_CASE("An editor scene can adopt a new asset identity after Save As", "[Scene][Lifecycle][Editor]")
{
    SceneManager manager;
    Ref<Scene> scene = CreateRef<Scene>("Saved scene");
    const UUID originalId(10, 0, 0, 0);
    const UUID savedAsId(20, 0, 0, 0);

    manager.SetActiveScene(scene, originalId);
    manager.SetActiveScene(scene, savedAsId);
    REQUIRE(manager.UnloadScene(originalId) == SceneOperationStatus::Completed);

    CHECK(manager.GetActiveScene() == scene);
    CHECK(manager.GetActiveSceneId() == savedAsId);
    CHECK(manager.GetLoadedScene(originalId) == nullptr);
    CHECK(manager.GetLoadedScene(savedAsId) == scene);
}

TEST_CASE("Editor scene persistence is disabled for runtime clones", "[Scene][Lifecycle][Editor]")
{
    CHECK(CanSaveEditorScene(SceneExecutionState::Edit));
    CHECK_FALSE(CanSaveEditorScene(SceneExecutionState::Play));
    CHECK_FALSE(CanSaveEditorScene(SceneExecutionState::PlayPaused));
    CHECK_FALSE(CanSaveEditorScene(SceneExecutionState::Simulate));
}

TEST_CASE("Runtime scene changes preserve play mode and the original edit scene", "[Scene][Lifecycle][Scripting]")
{
    SceneManager manager;
    Ref<Scene> editScene = CreateRef<Scene>("Edit");
    Ref<Scene> targetScene = CreateRef<Scene>("Target");
    const UUID editId = UuidGenerator::Generate();
    const UUID targetId = UuidGenerator::Generate();
    manager.SetActiveScene(editScene, editId);
    manager.SetActiveScene(targetScene, targetId);
    REQUIRE(manager.SetActiveScene(editId) == SceneOperationStatus::Completed);

    REQUIRE(manager.BeginPlay() == SceneOperationStatus::Completed);
    REQUIRE(manager.SetActiveScene(targetId) == SceneOperationStatus::Completed);
    CHECK(manager.GetExecutionState() == SceneExecutionState::Play);
    CHECK(manager.GetActiveSceneId() == targetId);
    CHECK(manager.GetActiveScene() != targetScene);
    CHECK(manager.UnloadScene(editId) == SceneOperationStatus::Failed);
    CHECK(manager.GetLoadedScene(editId) == editScene);

    REQUIRE(manager.Stop() == SceneOperationStatus::Completed);
    CHECK(manager.GetExecutionState() == SceneExecutionState::Edit);
    CHECK(manager.GetActiveScene() == editScene);
    CHECK(manager.GetActiveSceneId() == editId);
}

TEST_CASE("Scene deserialization reports invalid input without leaving an empty scene", "[Scene][Serialization]")
{
    const Path path = fs::temp_directory_path() / ("crowny-invalid-scene-" + UuidGenerator::Generate().ToString() + ".cwscene");
    std::ofstream(path) << "Scene: [unterminated";

    Ref<Scene> scene = CreateRef<Scene>(false);
    SceneSerializer serializer(scene);
    CHECK_FALSE(serializer.Deserialize(path));
    CHECK(scene->GetRootEntity());
    fs::remove(path);
}
