#include <catch2/catch_test_macros.hpp>

#include "Crowny/Scripting/Backends/CoreCLR/CoreClrHostContext.h"
#include "Crowny/Scripting/Managed/Internal/ManagedBackend.h"
#include "Crowny/Scripting/Managed/Interop/ManagedHostBindings.h"

#include <chrono>
#include <future>
#include <thread>

using namespace Crowny;

namespace
{
    cw_managed_mat4 IdentityMatrix()
    {
        cw_managed_mat4 matrix{};
        for (size_t index = 0; index < 4; ++index)
            matrix.values[index * 5] = 1.0f;
        return matrix;
    }

    cw_managed_string_view View(StringView value) { return { reinterpret_cast<const uint8_t*>(value.data()), static_cast<uint32_t>(value.size()) }; }

    struct ReentryProbe
    {
        cw_managed_host_api Api{};
        CoreClrHostContext* Context = nullptr;
        bool Revoke = false;
        bool WorkerCompletedInsideCallback = false;
        cw_managed_status ReentryStatus = CW_MANAGED_STATUS_NOT_INITIALIZED;
        std::thread Worker;
        std::future<cw_managed_status> Released;
    };

    thread_local ReentryProbe* s_ReentryProbe = nullptr;

    cw_managed_status CW_MANAGED_CALL ReenterHost(void* token, cw_managed_uuid, cw_managed_uuid*)
    {
        ReentryProbe& probe = *s_ReentryProbe;
        const cw_managed_mat4 matrix = IdentityMatrix();
        float determinant = 0.0f;
        probe.ReentryStatus = probe.Api.math_matrix_determinant(token, &matrix, &determinant);

        std::promise<cw_managed_status> released;
        probe.Released = released.get_future();
        probe.Worker = std::thread([api = probe.Api, result = std::move(released)]() mutable {
            api.log(api.context, 1, View("worker.reentry"), View("worker finished"), {});
            result.set_value(api.asset_release(api.context, {}));
        });
        probe.WorkerCompletedInsideCallback = probe.Released.wait_for(std::chrono::seconds(2)) == std::future_status::ready;
        if (probe.Revoke)
            probe.Context->Revoke();
        return CW_MANAGED_STATUS_OK;
    }
} // namespace

TEST_CASE("CoreCLR host table invokes native bindings and owns diagnostic text", "[Scripting][Managed][CoreCLR][HostContext]")
{
    CoreClrHostContext context;
    const cw_managed_host_api api = context.GetApi();
    const cw_managed_mat4 matrix = IdentityMatrix();
    float determinant = -1.0f;
    CHECK(api.math_matrix_determinant(api.context, &matrix, &determinant) == CW_MANAGED_STATUS_OK);
    CHECK(determinant == 1.0f);

    String message = "managed callback message";
    api.log(api.context, 2, View("test.callback"), View(message), View("managed stack"));
    message.clear();
    const Vector<ManagedDiagnostic> diagnostics = context.TakeDiagnostics();
    REQUIRE(diagnostics.size() == 1);
    CHECK(diagnostics.front().Code == "test.callback");
    CHECK(diagnostics.front().Message == "managed callback message");
    CHECK(diagnostics.front().ManagedStack == "managed stack");
    CHECK(context.TakeDiagnostics().empty());
}

TEST_CASE("CoreCLR copied callbacks reject destroyed and superseded host contexts", "[Scripting][Managed][CoreCLR][HostContext]")
{
    cw_managed_host_api staleApi{};
    {
        CoreClrHostContext context;
        staleApi = context.GetApi();
    }
    CoreClrHostContext replacement;
    REQUIRE(replacement.GetApi().context != staleApi.context);
    const cw_managed_mat4 matrix = IdentityMatrix();
    float determinant = -1.0f;
    CHECK(staleApi.math_matrix_determinant(staleApi.context, &matrix, &determinant) == CW_MANAGED_STATUS_NOT_INITIALIZED);
    CHECK(determinant == -1.0f);
    CHECK(staleApi.asset_release(staleApi.context, {}) == CW_MANAGED_STATUS_NOT_INITIALIZED);
    // A rejected callback must not inspect borrowed payloads after their owning program has gone away.
    const cw_managed_string_view invalidText{ reinterpret_cast<const uint8_t*>(uintptr_t{ 1 }), 1 };
    staleApi.log(staleApi.context, 2, invalidText, invalidText, invalidText);
    CHECK(replacement.TakeDiagnostics().empty());

    replacement.Revoke();
    replacement.Revoke();
    CHECK(replacement.GetApi().math_matrix_determinant(replacement.GetApi().context, &matrix, &determinant) == CW_MANAGED_STATUS_NOT_INITIALIZED);
    CHECK(staleApi.math_matrix_determinant(nullptr, &matrix, &determinant) == CW_MANAGED_STATUS_NOT_INITIALIZED);
}

TEST_CASE("CoreCLR host callbacks reject worker scene access but allow logging and finalizer releases", "[Scripting][Managed][CoreCLR][HostContext]")
{
    CoreClrHostContext context;
    const cw_managed_host_api api = context.GetApi();
    cw_managed_status operationStatus = CW_MANAGED_STATUS_OK;
    cw_managed_status releaseStatus = CW_MANAGED_STATUS_NOT_INITIALIZED;
    float determinant = -1.0f;
    std::thread worker([&] {
        const cw_managed_mat4 matrix = IdentityMatrix();
        operationStatus = api.math_matrix_determinant(api.context, &matrix, &determinant);
        String message = "queued worker diagnostic";
        api.log(api.context, 2, View("worker"), View(message), View("worker stack"));
        message.clear();
        releaseStatus = api.asset_release(api.context, {});
    });
    worker.join();
    CHECK(operationStatus == CW_MANAGED_STATUS_INVALID_ARGUMENT);
    CHECK(determinant == -1.0f);
    CHECK(releaseStatus == CW_MANAGED_STATUS_OK);
    const Vector<ManagedDiagnostic> diagnostics = context.TakeDiagnostics();
    REQUIRE(diagnostics.size() == 1);
    CHECK(diagnostics.front().Code == "worker");
    CHECK(diagnostics.front().Message == "queued worker diagnostic");
    CHECK(diagnostics.front().ManagedStack == "worker stack");
}

TEST_CASE("CoreCLR owner callbacks can reenter and wait for finalizer releases", "[Scripting][Managed][CoreCLR][HostContext]")
{
    cw_managed_host_api bindings{};
    PopulateManagedHostBindings(bindings);
    bindings.get_entity_parent = &ReenterHost;
    CoreClrHostContext context(bindings);
    ReentryProbe probe;
    probe.Api = context.GetApi();
    probe.Context = &context;
    SECTION("live context") {}
    SECTION("revoked synchronously in callback") { probe.Revoke = true; }
    s_ReentryProbe = &probe;
    const cw_managed_status status = probe.Api.get_entity_parent(probe.Api.context, {}, nullptr);
    probe.Worker.join();
    s_ReentryProbe = nullptr;
    CHECK(status == CW_MANAGED_STATUS_OK);
    CHECK(probe.ReentryStatus == CW_MANAGED_STATUS_OK);
    CHECK(probe.WorkerCompletedInsideCallback);
    CHECK(probe.Released.get() == CW_MANAGED_STATUS_OK);
    if (probe.Revoke)
    {
        CHECK(probe.Api.asset_release(probe.Api.context, {}) == CW_MANAGED_STATUS_NOT_INITIALIZED);
        CHECK(context.TakeDiagnostics().empty());
    }
    else
    {
        const Vector<ManagedDiagnostic> diagnostics = context.TakeDiagnostics();
        REQUIRE(diagnostics.size() == 1);
        CHECK(diagnostics.front().Code == "worker.reentry");
        CHECK(diagnostics.front().Message == "worker finished");
    }
}

#ifndef CW_EMSCRIPTEN
TEST_CASE("CoreCLR backend rejects native operations outside its owning thread", "[Scripting][Managed][CoreCLR][HostContext]")
{
    Scope<ManagedBackend> backend = CreateCoreClrBackend();
    REQUIRE(backend != nullptr);
    ManagedOperationResult started;
    ManagedOperationResult dispatched;
    ManagedCapabilities capabilities;
    const ScriptCatalog* workerCatalog = nullptr;
    const ScriptCatalog* ownerCatalog = &backend->GetScriptCatalog();
    Vector<ManagedDiagnostic> diagnostics;
    std::thread worker([&] {
        started = backend->Start({});
        dispatched = backend->Dispatch(0, {});
        capabilities = backend->GetCapabilities();
        workerCatalog = &backend->GetScriptCatalog();
        diagnostics = backend->Update();
        backend->Shutdown();
    });
    worker.join();
    CHECK(started.HasDiagnosticCode("managed.coreclr.wrong_thread"));
    CHECK(dispatched.HasDiagnosticCode("managed.coreclr.wrong_thread"));
    CHECK_FALSE(capabilities.DynamicProgramLoading);
    CHECK_FALSE(capabilities.Reload);
    CHECK_FALSE(capabilities.RuntimeReflection);
    CHECK_FALSE(capabilities.ManagedDebugging);
    CHECK_FALSE(capabilities.Profiling);
    CHECK_FALSE(capabilities.Threads);
    CHECK_FALSE(capabilities.NativeDynamicLibraries);
    CHECK_FALSE(capabilities.AotOnly);
    CHECK_FALSE(capabilities.WebAssembly);
    REQUIRE(workerCatalog != nullptr);
    CHECK(workerCatalog != ownerCatalog);
    CHECK(workerCatalog->Types.empty());
    CHECK(backend->GetCapabilities().DynamicProgramLoading);
    REQUIRE(diagnostics.size() == 1);
    CHECK(diagnostics.front().Code == "managed.coreclr.wrong_thread");
    CHECK(backend->Start({}).HasDiagnosticCode("managed.coreclr.runtime_root_missing"));
}
#endif
