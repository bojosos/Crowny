#include <catch2/catch_test_macros.hpp>

#include "Crowny/Application/Application.h"
#include "Crowny/Application/EngineRuntime.h"
#include "Crowny/Ecs/Components.h"
#include "Crowny/Scene/Scene.h"
#include "Crowny/Scene/SceneManager.h"
#include "Crowny/Scene/ScriptRuntime.h"
#include "Crowny/Scripting/Managed/ManagedScripting.h"
#include "ManagedTestPaths.h"

using namespace Crowny;

namespace
{
    class ScopedActiveScene
    {
    public:
        explicit ScopedActiveScene(const Ref<Scene>& scene) : m_OwnsManager(!SceneManager::IsStartedUp())
        {
            if (m_OwnsManager)
                SceneManager::StartUp();
            else
                m_Previous = SceneManager::Get().GetActiveScene();
            SceneManager::Get().SetActiveScene(scene);
        }
        ~ScopedActiveScene()
        {
            if (m_OwnsManager)
                SceneManager::Shutdown();
            else
                SceneManager::Get().SetActiveScene(m_Previous);
        }

    private:
        bool m_OwnsManager;
        Ref<Scene> m_Previous;
    };

    ScriptTypeIdentity Probe(String name) { return { GAME_ASSEMBLY, "Sandbox", std::move(name) }; }

    ManagedScripting& StartMutationFixture()
    {
        if (!Application::IsStartedUp())
        {
            ApplicationDesc description;
            description.Name = "Test";
            description.Headless = true;
            description.WorkingDirectory = fs::current_path();
            Application::StartUp(description);
        }
        ManagedScripting* scripting = Application::Get().GetRuntime().GetManagedScripting();
        REQUIRE(scripting != nullptr);
        ManagedProgramDefinition program;
        program.Generation = 1;
        program.Artifacts.push_back({ ManagedProgramArtifactKind::EngineAssembly, CROWNY_ASSEMBLY,
                                      Test::ResolveManagedAssembly("CrownySharp.dll", "Crowny-Sharp/CrownySharp.dll") });
        program.Artifacts.push_back({ ManagedProgramArtifactKind::GameAssembly, GAME_ASSEMBLY,
                                      Test::ResolveManagedAssembly("GameAssembly.dll", "Crowny-Sandbox/GameAssembly.dll") });
        const ManagedOperationResult loaded = scripting->LoadProgram(program);
        if (!loaded.Succeeded && !loaded.HasDiagnosticCode("managed.mono.program_already_loaded"))
        {
            for (const ManagedDiagnostic& diagnostic : loaded.Diagnostics)
                WARN(diagnostic.Code << ": " << diagnostic.Message);
            REQUIRE(loaded.Succeeded);
        }
        REQUIRE(scripting->GetScriptCatalog().FindType(Probe("ScriptMutationProbe")) != nullptr);
        return *scripting;
    }
} // namespace

TEST_CASE("Physics script dispatch survives additions and removals in a callback",
          "[Scripting][Managed][Lifecycle][Mutation][Mono][.ProcessIsolated]")
{
    StartMutationFixture();
    Ref<Scene> scene = CreateRef<Scene>(false);
    ScopedActiveScene activeScene(scene);
    Entity entity = scene->CreateEntity("callbacks|");
    Entity other = scene->CreateEntity("other");
    REQUIRE(scene->AddScriptComponent(entity, Probe("ScriptMutationProbe"), false));
    REQUIRE(scene->AddScriptComponent(entity, Probe("ScriptMutationRemoved"), false));
    REQUIRE(scene->AddScriptComponent(entity, Probe("ScriptMutationSurvivor"), false));
    entity.GetComponent<ManagedScriptComponent>().Scripts.shrink_to_fit();
    ScriptRuntime::OnStart(scene);

    ScriptEvent event;
    event.OtherEntity = other.GetUuid();
    SECTION("2D trigger") { event.Kind = ScriptEventKind::TriggerEnter2D; }
    SECTION("3D trigger") { event.Kind = ScriptEventKind::TriggerEnter3D; }
    ScriptRuntime::Dispatch(entity, event);
    CHECK(entity.GetName() == "callbacks|mutator|survivor|");
    REQUIRE(entity.GetComponent<ManagedScriptComponent>().Scripts.size() == 3);

    ScriptRuntime::Dispatch(entity, event);
    CHECK(entity.GetName() == "callbacks|mutator|survivor|mutator|survivor|added|");
    ScriptRuntime::OnShutdown(scene);
}

TEST_CASE("Script removal survives recursive removal and vector growth in OnDestroy",
          "[Scripting][Managed][Lifecycle][Mutation][Mono][.ProcessIsolated]")
{
    ManagedScripting& scripting = StartMutationFixture();
    Ref<Scene> scene = CreateRef<Scene>(false);
    ScopedActiveScene activeScene(scene);
    Entity entity = scene->CreateEntity("callbacks|");
    const ScriptTypeIdentity identity = Probe("ScriptDestroyMutationProbe");
    REQUIRE(scene->AddScriptComponent(entity, identity, false));
    entity.GetComponent<ManagedScriptComponent>().Scripts.shrink_to_fit();
    ScriptRuntime::OnStart(scene);
    const ScriptInstanceHandle destroyedHandle = entity.GetComponent<ManagedScriptComponent>().Scripts.front().GetRuntimeHandle();

    scene->RemoveScriptComponent(entity, identity);
    CHECK(entity.GetName() == "callbacks|destroy|");
    REQUIRE(entity.HasComponent<ManagedScriptComponent>());
    const auto& scripts = entity.GetComponent<ManagedScriptComponent>().Scripts;
    REQUIRE(scripts.size() == 1);
    CHECK(scripts.front().GetTypeIdentity() == Probe("ScriptMutationAdded"));
    CHECK_FALSE(scripting.CaptureState(destroyedHandle).Result.Succeeded);
    ScriptRuntime::OnShutdown(scene);
}
