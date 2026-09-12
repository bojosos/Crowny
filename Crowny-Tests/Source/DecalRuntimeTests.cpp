#include "Crowny/Application/Application.h"
#include "Crowny/Application/EngineRuntime.h"
#include "Crowny/Scene/Scene.h"
#include "Crowny/Scene/SceneManager.h"
#include "Crowny/Scripting/Managed/ManagedProgramPackage.h"
#include "Crowny/Scripting/Managed/ManagedScripting.h"
#include "ManagedTestPaths.h"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace Crowny;

namespace
{
    void RunDecalProbe(ManagedScripting& scripting)
    {
        const bool ownsManager = !SceneManager::IsStartedUp();
        if (ownsManager)
            SceneManager::StartUp();
        const auto previous = SceneManager::Get().GetActiveScene();
        struct Restore
        {
            bool Owns;
            Ref<Scene> Previous;
            ~Restore()
            {
                if (Owns)
                    SceneManager::Shutdown();
                else
                    SceneManager::Get().SetActiveScene(Previous);
            }
        } restore{ ownsManager, previous };
        auto scene = CreateRef<Scene>(false);
        SceneManager::Get().SetActiveScene(scene);
        Entity entity = scene->CreateEntity("Managed decal");
        ScriptCreateRequest request;
        request.Identity = { "GameAssembly", "Sandbox", "DecalRuntimeProbe" };
        request.Entity = entity.GetUuid();
        const auto created = scripting.CreateScript(request);
        for (const auto& diagnostic : created.Result.Diagnostics)
            INFO(diagnostic.Message);
        REQUIRE(created.Result.Succeeded);
        const auto dispatch = [&](ScriptEventKind kind) {
            const auto result = scripting.Dispatch(created.Handle, ScriptEvent::Lifecycle(kind));
            for (const auto& diagnostic : result.Diagnostics)
                INFO(diagnostic.Message);
            REQUIRE(result.Succeeded);
        };
        dispatch(ScriptEventKind::Start);
        REQUIRE(entity.HasComponent<DecalComponent>());
        auto& decal = entity.GetComponent<DecalComponent>();
        CHECK(decal.Projection == DecalProjection::Cylinder);
        CHECK(decal.TopRadius == Catch::Approx(0.4f));
        CHECK(decal.UVScale == glm::vec2(-2, 3));
        CHECK(decal.UVOffset == glm::vec2(0.1f, 0.2f));
        CHECK(decal.Target == entity.GetUuid());
        decal.Age = 3;
        dispatch(ScriptEventKind::Update);
        CHECK_FALSE(decal.LifetimeRunning);
        CHECK(decal.Age == 3);
        decal.Enabled = false;
        dispatch(ScriptEventKind::Update);
        CHECK(decal.LifetimeRunning);
        CHECK(decal.Enabled);
        CHECK(decal.Age == 0);
        dispatch(ScriptEventKind::Update);
        CHECK_FALSE(entity.HasComponent<DecalComponent>());
        REQUIRE(scripting.DestroyScript(created.Handle).Succeeded);
    }
} // namespace

TEST_CASE("Mono can create mutate and remove decals", "[decals][Mono][.ProcessIsolated]")
{
    if (!Application::IsStartedUp())
    {
        ApplicationDesc description;
        description.Name = "Decal Mono integration";
        description.Headless = true;
        description.WorkingDirectory = fs::current_path();
        Application::StartUp(description);
    }
    auto* scripting = Application::Get().GetRuntime().GetManagedScripting();
    REQUIRE(scripting);
    ManagedProgramDefinition program;
    program.Generation = 1;
    program.Artifacts.push_back(
      { ManagedProgramArtifactKind::EngineAssembly, "CrownySharp", Test::ResolveManagedAssembly("CrownySharp.dll", "Crowny-Sharp/CrownySharp.dll") });
    program.Artifacts.push_back({ ManagedProgramArtifactKind::GameAssembly, "GameAssembly",
                                  Test::ResolveManagedAssembly("GameAssembly.dll", "Crowny-Sandbox/GameAssembly.dll") });
    const auto loaded = scripting->LoadProgram(program);
    REQUIRE((loaded.Succeeded || loaded.HasDiagnosticCode("managed.mono.program_already_loaded")));
    RunDecalProbe(*scripting);
}

TEST_CASE("CoreCLR can create mutate and remove decals", "[decals][CoreCLR][.ProcessIsolated]")
{
    const auto path = Test::ReadEnvironmentVariable("CROWNY_CORECLR_TEST_PACKAGE");
    if (path.empty())
        SKIP("Set CROWNY_CORECLR_TEST_PACKAGE to the published managed-program.json.");
    const auto package = LoadManagedProgramPackage(Path(path), 1);
    REQUIRE(package.Result.Succeeded);
    ManagedScripting scripting;
    REQUIRE(scripting.Start(package.Package.Runtime).Succeeded);
    REQUIRE(scripting.LoadProgram(package.Package.Program).Succeeded);
    RunDecalProbe(scripting);
}
