#include "cwpch.h"

#include "Crowny/Common/ConsoleBuffer.h"
#include "Crowny/Common/Log.h"
#include "Crowny/Scripting/Managed/Interop/ManagedHostBindings.h"
#include "Crowny/Scripting/Mono/MonoAssembly.h"
#include "Crowny/Scripting/Mono/MonoClass.h"
#include "Crowny/Scripting/Mono/MonoManager.h"
#include "Crowny/Scripting/Mono/MonoMethod.h"
#include "ManagedTestPaths.h"

#include <catch2/catch_test_macros.hpp>

#include <mono/metadata/object.h>
#include <mono/metadata/threads.h>

#include <cstdlib>
#include <iostream>
#include <stdexcept>

using namespace Crowny;

namespace
{
    cw_managed_host_api* g_HostApi = nullptr;

    void EnsureMonoStarted()
    {
        if (MonoManager::IsStartedUp())
            return;

        if (!ConsoleBuffer::IsStartedUp())
            ConsoleBuffer::StartUp();
        Log::Init("CrownyTests");

        const MonoRuntimePaths monoPaths = ResolveMonoRuntimePaths(fs::current_path());
        if (!monoPaths.HasRuntime())
            throw std::runtime_error("Unable to resolve a Mono runtime for Crowny tests. Set CROWNY_MONO_ROOT to a valid Mono installation.");
        MonoManager::StartUp(monoPaths.LibraryDirectory, monoPaths.EtcDirectory, 0);
    }

    void AttachThread()
    {
        EnsureMonoStarted();
        mono_thread_attach(MonoManager::Get().GetDomain());
    }

    void* GetNativeHostApi() { return g_HostApi; }

    float DirectValue() { return 1.0f; }

    cw_managed_status CW_MANAGED_CALL HostValue(void*, float* result)
    {
        if (result == nullptr)
            return CW_MANAGED_STATUS_INVALID_ARGUMENT;
        *result = 1.0f;
        return CW_MANAGED_STATUS_OK;
    }

    int GetIterationCount()
    {
        constexpr int defaultIterations = 10'000'000;
        const char* configured = std::getenv("CROWNY_MANAGED_BINDING_BENCHMARK_ITERATIONS");
        if (configured == nullptr || *configured == '\0')
            return defaultIterations;

        const int value = std::atoi(configured);
        return value > 0 ? value : defaultIterations;
    }

    int64_t InvokeMeasurement(MonoMethod& method, int iterations)
    {
        void* parameters[] = { &iterations };
        MonoObject* boxedResult = method.Invoke(nullptr, parameters);
        REQUIRE(boxedResult != nullptr);
        return *static_cast<int64_t*>(mono_object_unbox(boxedResult));
    }
} // namespace

TEST_CASE("Managed host table dispatch benchmark", "[Mono][Scripting][Benchmark][.ProcessIsolated]")
{
    if (std::getenv("CROWNY_RUN_MANAGED_BINDING_BENCHMARK") == nullptr)
        SKIP("Set CROWNY_RUN_MANAGED_BINDING_BENCHMARK=1 to run this opt-in benchmark.");

    AttachThread();

    MonoAssembly* assembly = MonoManager::Get().GetAssembly(CROWNY_ASSEMBLY);
    if (assembly == nullptr)
    {
        const Path assemblyPath = Test::ResolveManagedAssembly("CrownySharp.dll", "Crowny-Sharp/CrownySharp.dll");
        REQUIRE(fs::is_regular_file(assemblyPath));
        assembly = &MonoManager::Get().LoadAssembly(assemblyPath, CROWNY_ASSEMBLY);
    }

    cw_managed_host_api hostApi{};
    hostApi.size = sizeof(hostApi);
    hostApi.abi_version = CW_MANAGED_ABI_VERSION;
    hostApi.context = &hostApi;
    PopulateManagedHostBindings(hostApi);
    hostApi.time_get_delta_time = &HostValue;
    g_HostApi = &hostApi;

    MonoClass* runtimeContext = assembly->GetClass(CROWNY_NS, "ManagedRuntimeContext");
    MonoClass* benchmark = assembly->GetClass(CROWNY_NS, "ManagedBindingBenchmark");
    REQUIRE(runtimeContext != nullptr);
    REQUIRE(benchmark != nullptr);
    runtimeContext->AddInternalCall("Internal_GetNativeHostApi", reinterpret_cast<const void*>(&GetNativeHostApi));
    benchmark->AddInternalCall("Internal_GetValue", reinterpret_cast<const void*>(&DirectValue));

    MonoMethod* direct = benchmark->GetMethod("MeasureDirectInternalCall", 1);
    MonoMethod* host = benchmark->GetMethod("MeasureHostTableCall", 1);
    REQUIRE(direct != nullptr);
    REQUIRE(host != nullptr);

    const int iterations = GetIterationCount();
    InvokeMeasurement(*direct, 10'000);
    InvokeMeasurement(*host, 10'000);
    const int64_t directTicks = InvokeMeasurement(*direct, iterations);
    const int64_t hostTicks = InvokeMeasurement(*host, iterations);

    const double directNanoseconds = static_cast<double>(directTicks);
    const double hostNanoseconds = static_cast<double>(hostTicks);
    const double ratio = hostNanoseconds / directNanoseconds;
    std::cout << "Managed binding benchmark (" << iterations << " calls): direct Mono internal call "
              << directNanoseconds / iterations << " ns/call, host table " << hostNanoseconds / iterations
              << " ns/call, ratio " << ratio << 'x' << std::endl;

    CHECK(directTicks > 0);
    CHECK(hostTicks > 0);
}
