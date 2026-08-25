#include <catch2/catch_test_macros.hpp>
#include "Crowny/Common/ConsoleBuffer.h"
#include "Crowny/Common/Log.h"
#include "Crowny/Scripting/ManagedReload.h"
#include "Crowny/Scripting/Mono/MonoManager.h"
#include "Crowny/Scripting/Mono/MonoUtils.h"
#include <cstdio>
#include <filesystem>
#include <stdexcept>

#include <mono/metadata/threads.h>

using namespace Crowny;

namespace
{
    void PrintMonoStartupPhase(const char* phase)
    {
        std::fprintf(stderr, "[Crowny-Tests][Mono] %s\n", phase);
        std::fflush(stderr);
    }

    void EnsureMonoStarted()
    {
        if (MonoManager::IsStartedUp())
            return;

        PrintMonoStartupPhase("initializing test logging");
        if (!ConsoleBuffer::IsStartedUp())
            ConsoleBuffer::StartUp();
        Log::Init("CrownyTests");

        PrintMonoStartupPhase("resolving runtime paths");
        const MonoRuntimePaths monoPaths = ResolveMonoRuntimePaths(fs::current_path());
        if (!monoPaths.HasRuntime())
            throw std::runtime_error(
              "Unable to resolve a Mono runtime for Crowny tests. Set CROWNY_MONO_ROOT to a valid Mono installation.");

        std::fprintf(stderr, "[Crowny-Tests][Mono] starting runtime (lib=%s, etc=%s)\n",
                     monoPaths.LibraryDirectory.string().c_str(), monoPaths.EtcDirectory.string().c_str());
        std::fflush(stderr);
        MonoManager::StartUp(monoPaths.LibraryDirectory, monoPaths.EtcDirectory, 0);

        if (!MonoManager::IsStartedUp() || MonoManager::Get().GetDomain() == nullptr)
            throw std::runtime_error("Mono failed to start for Crowny tests using the resolved runtime directories.");
        PrintMonoStartupPhase("runtime ready");
    }
} // namespace

void AttachThread()
{
    EnsureMonoStarted();
    mono_thread_attach(MonoManager::Get().GetDomain());
}

TEST_CASE("Mono::Utils::StringConversion", "[Mono]")
{
    AttachThread();

    SECTION("String to MonoString and back")
    {
        String original = "Hello Crowny Mono!";
        MonoString* monoStr = MonoUtils::ToMonoString(original);
        REQUIRE(monoStr != nullptr);

        String back = MonoUtils::FromMonoString(monoStr);
        CHECK(back == original);
    }

    SECTION("Empty string conversion")
    {
        String original = "";
        MonoString* monoStr = MonoUtils::ToMonoString(original);
        REQUIRE(monoStr != nullptr);

        String back = MonoUtils::FromMonoString(monoStr);
        CHECK(back == original);
    }
}

TEST_CASE("Mono::Utils::Boxing", "[Mono]")
{
    AttachThread();

    SECTION("Box/Unbox Int32")
    {
        int32_t value = 12345;
        MonoObject* boxed = MonoUtils::Box(MonoUtils::GetI32Class(), &value);
        REQUIRE(boxed != nullptr);

        int32_t unboxed = *(int32_t*)MonoUtils::Unbox(boxed);
        CHECK(unboxed == value);
    }

    SECTION("Box/Unbox Float")
    {
        float value = 3.14159f;
        MonoObject* boxed = MonoUtils::Box(MonoUtils::GetFloatClass(), &value);
        REQUIRE(boxed != nullptr);

        float unboxed = *(float*)MonoUtils::Unbox(boxed);
        CHECK(unboxed == value);
    }

    SECTION("Box/Unbox Bool")
    {
        bool value = true;
        MonoObject* boxed = MonoUtils::Box(MonoUtils::GetBoolClass(), &value);
        REQUIRE(boxed != nullptr);

        bool unboxed = *(bool*)MonoUtils::Unbox(boxed);
        CHECK(unboxed == value);
    }
}

TEST_CASE("Mono::Utils::GCHandles", "[Mono]")
{
    AttachThread();

    ::MonoClass* i32Class = MonoUtils::GetI32Class();
    REQUIRE(i32Class != nullptr);

    int32_t value = 42;
    MonoObject* obj = MonoUtils::Box(i32Class, &value);
    REQUIRE(obj != nullptr);

    SECTION("Normal GCHandle")
    {
        uint32_t handle = MonoUtils::NewGCHandle(obj, false);
        CHECK(handle != 0);

        MonoObject* retrieved = MonoUtils::GetObjectFromGCHandle(handle);
        CHECK(retrieved == obj);

        MonoUtils::FreeGCHandle(handle);
    }
}
