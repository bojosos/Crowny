#include <catch2/catch_test_macros.hpp>

#include "Crowny/Application/Application.h"
#include "Crowny/Application/EngineRuntime.h"
#include "Crowny/Scene/Scene.h"
#include "Crowny/Scene/SceneManager.h"
#include "Crowny/Scripting/Backends/Mono/MonoScriptRuntime.h"
#include "Crowny/Scripting/Bindings/Scene/ScriptEntityBehaviour.h"
#include "Crowny/Scripting/Managed/ManagedScripting.h"
#include "Crowny/Scripting/Mono/MonoClass.h"
#include "Crowny/Scripting/Mono/MonoManager.h"
#include "Crowny/Scripting/Mono/MonoUtils.h"
#include "Crowny/Scripting/ScriptSceneObjectManager.h"
#include "ManagedTestPaths.h"

#include <mono/metadata/mono-gc.h>
#include <type_traits>

using namespace Crowny;

void AttachThread();

TEST_CASE("Temporary Mono GC roots transfer ownership safely", "[Mono][Scripting][GcLifetime]")
{
    static_assert(!std::is_copy_constructible_v<MonoGCHandle>);
    static_assert(!std::is_copy_assignable_v<MonoGCHandle>);
    static_assert(std::is_nothrow_move_constructible_v<MonoGCHandle>);
    static_assert(std::is_nothrow_move_assignable_v<MonoGCHandle>);
    AttachThread();
    MonoGCHandle first(reinterpret_cast<MonoObject*>(MonoUtils::ToMonoString("retained")));
    MonoGCHandle second(std::move(first));
    CHECK(first.Get() == nullptr);
    MonoGCHandle third(reinterpret_cast<MonoObject*>(MonoUtils::ToMonoString("replaced")));
    third = std::move(second);
    CHECK(second.Get() == nullptr);
    mono_gc_collect(mono_gc_max_generation());
    CHECK(MonoUtils::FromMonoString(reinterpret_cast<MonoString*>(third.Get())) == "retained");
}

TEST_CASE("Mono runtime resolves the current wrapper through collections and replacement", "[Mono][Scripting][GcLifetime][.ProcessIsolated]")
{
    if (!Application::IsStartedUp())
    {
        ApplicationDesc description;
        description.Name = "Mono GC lifetime test";
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
    } restore{ ownsSceneManager, previousScene };

    Entity entity = scene->CreateEntity("GC script");
    Entity other = scene->CreateEntity("GC collision peer");
    const ScriptTypeIdentity identity{ GAME_ASSEMBLY, "Sandbox", "MonoGcLifetimeProbe" };
    ScriptCreateRequest request;
    request.Identity = identity;
    request.Entity = entity.GetUuid();
    const ScriptCreateResult created = scripting->CreateScript(request);
    REQUIRE(created.Result.Succeeded);
    REQUIRE(created.Handle.IsValid());
    auto& scripts = entity.GetComponent<ManagedScriptComponent>().Scripts;
    REQUIRE(scripts.size() == 1);
    const uint64_t instanceId = scripts.front().InstanceId;
    Crowny::MonoClass* scriptClass = MonoManager::Get().FindClass(identity.Assembly, identity.Namespace, identity.TypeName);
    REQUIRE(scriptClass != nullptr);
    MonoScriptRuntime runtime;
    REQUIRE(runtime.Bind(instanceId, scriptClass));
    const MonoGCHandle original(runtime.GetInstance());
    REQUIRE(original.Get() != nullptr);

    for (int index = 0; index < 3; ++index)
    {
        mono_gc_collect(mono_gc_max_generation());
        CHECK(runtime.GetInstance() == original.Get());
        REQUIRE(scripting->Dispatch(created.Handle, ScriptEvent::Lifecycle(ScriptEventKind::Update)).Succeeded);
    }
    ScriptStateResult state = scripting->CaptureState(created.Handle);
    REQUIRE(state.Result.Succeeded);
    CHECK(state.State.Root.Members.at("ConstructorValue").SignedValue == 73);
    CHECK(state.State.Root.Members.at("UpdateCount").SignedValue == 3);

    // Retain the old managed object so allocator address reuse cannot hide a stale cache.
    ScriptSceneObjectManager::Get().DestroyManagedScriptComponent(entity, &scripts.front());
    CHECK(runtime.GetInstance() == nullptr);
    const MonoGCHandle replacement(scriptClass->CreateInstance(true));
    REQUIRE(replacement.Get() != original.Get());
    REQUIRE(ScriptSceneObjectManager::Get().CreateManagedScriptComponent(replacement.Get(), entity, scripts.front()) != nullptr);
    CHECK(runtime.GetInstance() == replacement.Get());
    REQUIRE(scripting->Dispatch(created.Handle, ScriptEvent::Lifecycle(ScriptEventKind::Update)).Succeeded);

    ScriptEvent collision;
    collision.OtherEntity = other.GetUuid();
    collision.Contacts.push_back({ { 4.0f, 5.0f, 6.0f }, { 0.0f, 1.0f, 0.0f }, 0.0f, 1.0f });
    for (ScriptEventKind kind : { ScriptEventKind::CollisionEnter2D, ScriptEventKind::CollisionEnter3D })
    {
        collision.Kind = kind;
        REQUIRE(scripting->Dispatch(created.Handle, collision).Succeeded);
    }
    state = scripting->CaptureState(created.Handle);
    REQUIRE(state.Result.Succeeded);
    CHECK(state.State.Root.Members.at("UpdateCount").SignedValue == 1);
    CHECK(state.State.Root.Members.at("CollisionCount").SignedValue == 2);
    CHECK(scripting->DestroyScript(created.Handle).Succeeded);
    CHECK(runtime.GetInstance() == nullptr);
    CHECK(ScriptEntityBehaviour::ToNative(replacement.Get()) == nullptr);
    runtime.Clear();
    CHECK(runtime.GetScriptClass() == nullptr);
}
