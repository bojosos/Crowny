#include <catch2/catch_test_macros.hpp>

#include "Crowny/Application/Application.h"
#include "Crowny/Application/EngineRuntime.h"
#include "Crowny/Scene/Scene.h"
#include "Crowny/Scene/SceneManager.h"
#include "Crowny/Scripting/Bindings/Scene/ScriptEntityBehaviour.h"
#include "Crowny/Scripting/Bindings/Scene/ScriptSceneManager.h"
#include "Crowny/Scripting/Managed/Interop/CrownyManagedAbi.h"
#include "Crowny/Scripting/Managed/ManagedScripting.h"
#include "Crowny/Scripting/Mono/MonoClass.h"
#include "Crowny/Scripting/Mono/MonoField.h"
#include "Crowny/Scripting/Mono/MonoManager.h"
#include "Crowny/Scripting/Mono/MonoMethod.h"
#include "Crowny/Scripting/Mono/MonoUtils.h"
#include "Crowny/Scripting/ScriptSceneObjectManager.h"
#include "ManagedTestPaths.h"

using namespace Crowny;

namespace
{
    decltype(cw_managed_host_api::entity_has_component) originalHasComponent = nullptr;
    decltype(cw_managed_host_api::transform_get_position) originalGetPosition = nullptr;
    uint32_t hasComponentCalls = 0;
    uint32_t getPositionCalls = 0;

    cw_managed_status CW_MANAGED_CALL CountHasComponent(void* context, cw_managed_uuid entity, cw_managed_string_view type, uint8_t* result)
    {
        ++hasComponentCalls;
        return originalHasComponent(context, entity, type, result);
    }

    cw_managed_status CW_MANAGED_CALL CountGetPosition(void* context, cw_managed_uuid entity, cw_managed_vec3* result)
    {
        ++getPositionCalls;
        return originalGetPosition(context, entity, result);
    }
} // namespace

TEST_CASE("Managed component caches reduce transform calls and observe native lifetime changes",
          "[Mono][Scripting][ComponentCache][.ProcessIsolated]")
{
    if (!Application::IsStartedUp())
    {
        ApplicationDesc description;
        description.Name = "Component cache integration test";
        description.Headless = true;
        description.WorkingDirectory = fs::current_path();
        Application::StartUp(description);
    }
    ManagedScripting* scripting = Application::Get().GetRuntime().GetManagedScripting();
    REQUIRE(scripting != nullptr);
    ManagedProgramDefinition program;
    program.Generation = 1;
    program.Artifacts.push_back({ ManagedProgramArtifactKind::EngineAssembly, CROWNY_ASSEMBLY,
                                  Crowny::Test::ResolveManagedAssembly("CrownySharp.dll", "Crowny-Sharp/CrownySharp.dll") });
    program.Artifacts.push_back({ ManagedProgramArtifactKind::GameAssembly, GAME_ASSEMBLY,
                                  Crowny::Test::ResolveManagedAssembly("GameAssembly.dll", "Crowny-Sandbox/GameAssembly.dll") });
    const ManagedOperationResult loaded = scripting->LoadProgram(program);
    REQUIRE((loaded.Succeeded || loaded.HasDiagnosticCode("managed.mono.program_already_loaded")));

    const bool ownsSceneManager = !SceneManager::IsStartedUp();
    if (ownsSceneManager)
        SceneManager::StartUp();
    const Ref<Scene> previousScene = SceneManager::Get().GetActiveScene();
    Ref<Scene> scene = CreateRef<Scene>(false);
    SceneManager::Get().SetActiveScene(scene);
    struct RestoreScene
    {
        bool OwnsManager;
        Ref<Scene> Previous;
        ~RestoreScene()
        {
            if (OwnsManager)
                SceneManager::Shutdown();
            else
                SceneManager::Get().SetActiveScene(Previous);
        }
    } restoreScene{ ownsSceneManager, previousScene };
    Entity host = scene->CreateEntity("Component cache host");
    host.SetWorldPosition({ 2.0f, 0.0f, 0.0f });
    Entity target = scene->CreateEntity("Component cache target");
    target.AddComponent<CameraComponent>();
    ScriptCreateRequest request;
    request.Identity = { GAME_ASSEMBLY, "Sandbox", "ComponentCacheProbe" };
    request.Entity = host.GetUuid();
    const ScriptCreateResult created = scripting->CreateScript(request);
    REQUIRE(created.Result.Succeeded);
    const auto& scripts = host.GetComponent<ManagedScriptComponent>().Scripts;
    REQUIRE(scripts.size() == 1);
    ScriptEntityBehaviour* wrapper = ScriptSceneObjectManager::Get().GetManagedScriptComponent(scripts.front().InstanceId);
    REQUIRE(wrapper != nullptr);
    const MonoGCHandle probe(wrapper->GetManagedInstance());
    Crowny::MonoClass* probeClass = MonoManager::Get().FindClass(GAME_ASSEMBLY, "Sandbox", "ComponentCacheProbe");
    REQUIRE(probeClass != nullptr);
    MonoField* stageField = probeClass->GetField("Stage");
    REQUIRE(stageField != nullptr);
    const auto readBool = [&](const char* name) {
        MonoField* field = probeClass->GetField(name);
        REQUIRE(field != nullptr);
        bool result = false;
        field->Get(probe.Get(), &result);
        return result;
    };
    const auto readFloat = [&](const char* name) {
        MonoField* field = probeClass->GetField(name);
        REQUIRE(field != nullptr);
        float result = 0.0f;
        field->Get(probe.Get(), &result);
        return result;
    };
    const auto dispatch = [&](int32_t stage) {
        stageField->Set(probe.Get(), &stage);
        const ManagedOperationResult result = scripting->Dispatch(created.Handle, ScriptEvent::Lifecycle(ScriptEventKind::Update));
        for (const ManagedDiagnostic& diagnostic : result.Diagnostics)
            INFO(diagnostic.Message);
        REQUIRE(result.Succeeded);
    };

    Crowny::MonoClass* context = MonoManager::Get().FindClass(CROWNY_ASSEMBLY, CROWNY_NS, "ManagedRuntimeContext");
    REQUIRE(context != nullptr);
    Crowny::MonoMethod* getApi = context->GetMethod("Internal_GetNativeHostApi", 0);
    Crowny::MonoMethod* setApi = context->GetMethod("SetNativeHostApi", 1);
    REQUIRE(getApi != nullptr);
    REQUIRE(setApi != nullptr);
    MonoObject* apiResult = getApi->Invoke(nullptr, nullptr);
    REQUIRE(apiResult != nullptr);
    const auto* nativeApi = *static_cast<cw_managed_host_api**>(MonoUtils::Unbox(apiResult));
    REQUIRE(nativeApi != nullptr);
    struct RestoreHostApi
    {
        Crowny::MonoMethod* SetApi;
        cw_managed_host_api Api;
        ~RestoreHostApi()
        {
            void* parameters[] = { &Api };
            SetApi->Invoke(nullptr, parameters);
        }
    } restoreApi{ setApi, *nativeApi };
    cw_managed_host_api observedApi = *nativeApi;
    originalHasComponent = observedApi.entity_has_component;
    originalGetPosition = observedApi.transform_get_position;
    observedApi.entity_has_component = &CountHasComponent;
    observedApi.transform_get_position = &CountGetPosition;
    void* apiParameters[] = { &observedApi };
    setApi->Invoke(nullptr, apiParameters);
    ScriptSceneManager::DispatchPendingEvents();
    hasComponentCalls = 0;
    getPositionCalls = 0;

    dispatch(0);
    CHECK(readBool("TransformReused"));
    CHECK(readBool("EntityReused"));
    CHECK(readFloat("PositionSum") == 1026.0f);
    CHECK(hasComponentCalls == 1);
    CHECK(getPositionCalls == 513);
    dispatch(1);
    CHECK(readBool("CameraReused"));
    target.RemoveComponent<CameraComponent>();
    dispatch(2);
    CHECK(readBool("RemovedCameraMissing"));
    target.AddComponent<CameraComponent>();
    dispatch(3);
    CHECK(readBool("CameraReplaced"));
    dispatch(4);
    CHECK(readBool("CameraReplaced"));
    scene->DestroyEntity(target);
    dispatch(5);
    CHECK(readBool("DestroyedEntityRejected"));

    // Existing managed owners must refresh their cache when the active scene reuses a UUID.
    dispatch(0);
    Ref<Scene> nextScene = CreateRef<Scene>(false);
    Entity replacement = nextScene->CreateEntityWithUuid(host.GetUuid(), "Replacement cache host");
    replacement.SetWorldPosition({ 91.0f, 0.0f, 0.0f });
    SceneManager::Get().SetActiveScene(nextScene);
    ScriptSceneManager::DispatchPendingEvents();
    int32_t nextStage = 6;
    stageField->Set(probe.Get(), &nextStage);
    Crowny::MonoMethod* update = probeClass->GetMethod("Update", 0);
    REQUIRE(update != nullptr);
    MonoObject* exception = nullptr;
    update->Invoke(probe.Get(), nullptr, &exception);
    REQUIRE(exception == nullptr);
    CHECK(readBool("SceneCacheRenewed"));
    CHECK(readFloat("ScenePosition") == 91.0f);
    SceneManager::Get().SetActiveScene(scene);
    CHECK(scripting->DestroyScript(created.Handle).Succeeded);
}
