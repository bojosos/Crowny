#include <catch2/catch_test_macros.hpp>

#include "Crowny/Ecs/Components.h"
#include "Crowny/Scene/Scene.h"
#include "Crowny/Scripting/Backends/Mono/MonoBackend.h"
#include "Crowny/Scripting/Mono/MonoManager.h"
#include "Crowny/Scripting/ScriptObjectManager.h"

using namespace Crowny;

TEST_CASE("Mono refresh resolves duplicate script types by runtime occurrence", "[Scripting][Managed][Mono]")
{
    const ScriptTypeIdentity identity{ GAME_ASSEMBLY, "Tests", "DuplicateBehaviour" };
    ManagedScriptComponent component;
    component.Scripts.emplace_back(identity);
    component.Scripts.emplace_back(identity);

    ManagedScript& first = component.Scripts.front();
    ManagedScript& second = component.Scripts.back();
    REQUIRE(first.InstanceId != second.InstanceId);
    CHECK(component.FindScript(second.InstanceId) == &second);
}

TEST_CASE("Mono binding metadata registration is idempotent", "[Scripting][Managed][Mono]")
{
    static ScriptMeta metadata;
    static const ScriptMeta localMetadata("CrownyTests", "Tests", "RegistrationProbe", []() {});

    CHECK(MonoManager::RegisterScriptType(&metadata, localMetadata));
    CHECK_FALSE(MonoManager::RegisterScriptType(&metadata, localMetadata));
}

TEST_CASE("Mono failed creation rolls back only its added occurrence", "[Scripting][Managed][Mono]")
{
    const ScriptTypeIdentity identity{ GAME_ASSEMBLY, "Tests", "RollbackBehaviour" };
    Ref<Scene> scene = CreateRef<Scene>(false);
    Entity entity = scene->CreateEntity("Managed rollback");
    ManagedScriptComponent& component = entity.AddComponent<ManagedScriptComponent>();
    component.Scripts.emplace_back(identity);
    const uint64_t retainedId = component.Scripts.back().InstanceId;

    SECTION("an owned occurrence is erased from an existing component")
    {
        component.Scripts.emplace_back(identity);
        const uint64_t addedId = component.Scripts.back().InstanceId;
        MonoBackendDetail::RollbackAddedScriptOccurrence(entity, addedId, true, false);

        REQUIRE(entity.HasComponent<ManagedScriptComponent>());
        REQUIRE(entity.GetComponent<ManagedScriptComponent>().Scripts.size() == 1);
        CHECK(entity.GetComponent<ManagedScriptComponent>().Scripts.front().InstanceId == retainedId);
    }

    SECTION("a pre-existing occurrence is never erased")
    {
        MonoBackendDetail::RollbackAddedScriptOccurrence(entity, retainedId, false, false);
        REQUIRE(entity.HasComponent<ManagedScriptComponent>());
        REQUIRE(entity.GetComponent<ManagedScriptComponent>().Scripts.size() == 1);
        CHECK(entity.GetComponent<ManagedScriptComponent>().Scripts.front().InstanceId == retainedId);
    }

    SECTION("an owned component is removed when its owned occurrence was the only entry")
    {
        Entity addedEntity = scene->CreateEntity("Managed component rollback");
        ManagedScriptComponent& addedComponent = addedEntity.AddComponent<ManagedScriptComponent>();
        addedComponent.Scripts.emplace_back(identity);
        const uint64_t addedId = addedComponent.Scripts.back().InstanceId;

        MonoBackendDetail::RollbackAddedScriptOccurrence(addedEntity, addedId, true, true);
        CHECK_FALSE(addedEntity.HasComponent<ManagedScriptComponent>());
    }
}

TEST_CASE("Mono reload reports state rollback failure", "[Scripting][Managed][Mono]")
{
    SECTION("state restoration diagnostics are preserved")
    {
        ManagedOperationResult replacementFailure =
          ManagedOperationResult::Failure("managed.test.replacement_failed", "Replacement failed.", ManagedBackendId::Mono);
        const ManagedOperationResult stateFailure =
          ManagedOperationResult::Failure("managed.test.state_restore_failed", "State restoration failed.", ManagedBackendId::Mono);

        const ManagedBackendReloadResult result =
          MonoBackendDetail::AddReloadRollbackDiagnostics(std::move(replacementFailure), true, stateFailure);
        CHECK_FALSE(result.Result.Succeeded);
        CHECK(result.ProgramInvalidated);
        CHECK(result.Result.HasDiagnosticCode("managed.test.replacement_failed"));
        CHECK(result.Result.HasDiagnosticCode("managed.test.state_restore_failed"));
        CHECK(result.Result.HasDiagnosticCode("managed.mono.reload_state_rollback_failed"));
    }

    SECTION("assembly restoration failure is explicit")
    {
        ManagedOperationResult replacementFailure =
          ManagedOperationResult::Failure("managed.test.replacement_failed", "Replacement failed.", ManagedBackendId::Mono);
        const ManagedBackendReloadResult result =
          MonoBackendDetail::AddReloadRollbackDiagnostics(std::move(replacementFailure), false, ManagedOperationResult::Success());
        CHECK_FALSE(result.Result.Succeeded);
        CHECK(result.ProgramInvalidated);
        CHECK(result.Result.HasDiagnosticCode("managed.mono.reload_rollback_failed"));
    }
}

TEST_CASE("Mono assembly refresh failure reports whether the previous domain survived", "[Scripting][Managed][Mono]")
{
    SECTION("pre-unload rejection keeps the current program valid")
    {
        const ManagedBackendReloadResult result =
          MonoBackendDetail::BuildAssemblyRefreshFailure({ AssemblyRefreshStatus::CurrentDomainKept });
        CHECK_FALSE(result.Result.Succeeded);
        CHECK_FALSE(result.ProgramInvalidated);
        CHECK(result.Result.HasDiagnosticCode("managed.mono.reload_failed"));
        CHECK_FALSE(result.Result.HasDiagnosticCode("managed.mono.reload_rollback_failed"));
    }

    SECTION("successful rollback keeps the previous program valid")
    {
        const ManagedBackendReloadResult result =
          MonoBackendDetail::BuildAssemblyRefreshFailure({ AssemblyRefreshStatus::PreviousDomainRestored });
        CHECK_FALSE(result.Result.Succeeded);
        CHECK_FALSE(result.ProgramInvalidated);
        CHECK(result.Result.HasDiagnosticCode("managed.mono.reload_failed"));
        CHECK_FALSE(result.Result.HasDiagnosticCode("managed.mono.reload_rollback_failed"));
    }

    SECTION("failed rollback invalidates the managed program")
    {
        const ManagedBackendReloadResult result =
          MonoBackendDetail::BuildAssemblyRefreshFailure({ AssemblyRefreshStatus::PreviousDomainRestoreFailed });
        CHECK_FALSE(result.Result.Succeeded);
        CHECK(result.ProgramInvalidated);
        CHECK(result.Result.HasDiagnosticCode("managed.mono.reload_rollback_failed"));
    }
}
