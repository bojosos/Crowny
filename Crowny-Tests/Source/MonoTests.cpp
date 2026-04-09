#include <catch2/catch_test_macros.hpp>
#include "Crowny/Common/ConsoleBuffer.h"
#include "Crowny/Common/Log.h"
#include "Crowny/Scripting/Mono/MonoManager.h"
#include "Crowny/Scripting/Mono/MonoUtils.h"
#include <filesystem>

#include <mono/metadata/threads.h>

using namespace Crowny;

struct MonoGlobalFixture {
    MonoGlobalFixture() {
        if (!ConsoleBuffer::IsStartedUp())
            ConsoleBuffer::StartUp();
        Log::Init("CrownyTests");
        if (!MonoManager::IsStartedUp())
        {
            // Mono expects absolute paths usually
            String libDir = "C:\\\\Program Files\\\\Mono\\\\lib";
            String etcDir = "C:\\\\Program Files\\\\Mono\\\\etc";
            
            // Check if these paths exist
            if (!std::filesystem::exists(libDir)) {
                libDir = "C:/Program Files/Mono/lib";
            }
            if (!std::filesystem::exists(etcDir)) {
                etcDir = "C:/Program Files/Mono/etc";
            }

            printf("Initializing Mono with Lib: %s, Etc: %s\\n", libDir.c_str(), etcDir.c_str());
            MonoManager::StartUp(libDir, etcDir, 0);
            
            if (MonoManager::IsStartedUp()) {
                printf("Mono started up successfully\\n");
            } else {
                printf("Mono FAILED to start up\\n");
            }
        }
    }

    ~MonoGlobalFixture() {
        // MonoManager::ShutDown(); // Usually we don't shut down in unit tests to avoid crashes
    }
};

// Global fixture for all tests in this file
static MonoGlobalFixture g_MonoFixture;

void AttachThread()
{
    if (MonoManager::IsStartedUp())
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
