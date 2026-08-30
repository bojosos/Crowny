#include "Crowny/Common/ConsoleBuffer.h"
#include "Crowny/Common/Log.h"
#include "Crowny/Scripting/Backends/Mono/MonoObjectIdentity.h"
#include "Crowny/Scripting/ManagedReload.h"
#include "Crowny/Scripting/Mono/MonoAssembly.h"
#include "Crowny/Scripting/Mono/MonoField.h"
#include "Crowny/Scripting/Mono/MonoManager.h"
#include "Crowny/Scripting/Mono/MonoUtils.h"
#include "ManagedTestPaths.h"
#include <catch2/catch_test_macros.hpp>
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
            throw std::runtime_error("Unable to resolve a Mono runtime for Crowny tests. Set CROWNY_MONO_ROOT to a valid Mono installation.");

        std::fprintf(stderr, "[Crowny-Tests][Mono] starting runtime (lib=%s, etc=%s)\n", monoPaths.LibraryDirectory.string().c_str(),
                     monoPaths.EtcDirectory.string().c_str());
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

    SECTION("Resurrection-tracking weak GCHandle")
    {
        uint32_t handle = MonoUtils::NewWeakGCHandle(obj, true);
        CHECK(handle != 0);

        MonoObject* retrieved = MonoUtils::GetObjectFromGCHandle(handle);
        CHECK(retrieved == obj);

        MonoUtils::FreeGCHandle(handle);
    }
}

TEST_CASE("Mono wrappers bind stable managed identities", "[Mono][Scripting][Identity][.ProcessIsolated]")
{
    AttachThread();

    Crowny::MonoAssembly* assembly = MonoManager::Get().GetAssembly(CROWNY_ASSEMBLY);
    if (assembly == nullptr)
    {
        const Path assemblyPath = Crowny::Test::ResolveManagedAssembly("CrownySharp.dll", "Crowny-Sharp/CrownySharp.dll");
        REQUIRE(fs::is_regular_file(assemblyPath));
        assembly = &MonoManager::Get().LoadAssembly(assemblyPath, CROWNY_ASSEMBLY);
    }

    const UUID expected(0x10203040u, 0x50607080u, 0x90a0b0c0u, 0xd0e0f000u);
    const auto verify = [&](StringView typeName, StringView fieldName, const auto& bind) {
        const String managedTypeName(typeName);
        Crowny::MonoClass* type = assembly->GetClass(CROWNY_NS, managedTypeName);
        REQUIRE(type != nullptr);
        MonoObject* instance = type->CreateInstance(false);
        REQUIRE(instance != nullptr);
        REQUIRE(bind(instance, expected));
        const String declaringTypeName = fieldName == "m_ManagedEntityId" ? "Component" : managedTypeName;
        Crowny::MonoClass* declaringType = assembly->GetClass(CROWNY_NS, declaringTypeName);
        REQUIRE(declaringType != nullptr);
        Crowny::MonoField* field = declaringType->GetField(fieldName);
        REQUIRE(field != nullptr);
        UUID actual;
        field->Get(instance, &actual);
        CHECK(actual == expected);
    };

    verify("Entity", "m_ManagedUuid", MonoObjectIdentity::SetEntity);
    verify("AnimationComponent", "m_ManagedEntityId", MonoObjectIdentity::SetComponentEntity);
    verify("Material", "m_ManagedUuid", MonoObjectIdentity::SetAsset);
}

TEST_CASE("Managed animation API exposes clip identity and playback controls", "[Mono][Animation][.ProcessIsolated]")
{
    AttachThread();

    Crowny::MonoAssembly* assembly = MonoManager::Get().GetAssembly(CROWNY_ASSEMBLY);
    if (assembly == nullptr)
    {
        const Path assemblyPath = Crowny::Test::ResolveManagedAssembly("CrownySharp.dll", "Crowny-Sharp/CrownySharp.dll");
        REQUIRE(fs::is_regular_file(assemblyPath));
        assembly = &MonoManager::Get().LoadAssembly(assemblyPath, CROWNY_ASSEMBLY);
    }

    Crowny::MonoClass* asset = assembly->GetClass(CROWNY_NS, "Asset");
    Crowny::MonoClass* component = assembly->GetClass(CROWNY_NS, "Component");
    Crowny::MonoClass* clip = assembly->GetClass(CROWNY_NS, "AnimationClip");
    Crowny::MonoClass* animation = assembly->GetClass(CROWNY_NS, "AnimationComponent");
    REQUIRE(asset != nullptr);
    REQUIRE(component != nullptr);
    REQUIRE(clip != nullptr);
    REQUIRE(animation != nullptr);
    CHECK(clip->IsSubClassOf(asset));
    CHECK(animation->IsSubClassOf(component));

    for (StringView property : { "length", "sampleRate", "isAdditive" })
    {
        CAPTURE(property);
        CHECK(clip->GetProperty(property) != nullptr);
    }
    for (StringView property : { "clip", "speed", "wrapMode", "playOnAwake", "applyRootMotion", "time", "normalizedTime", "state" })
    {
        CAPTURE(property);
        CHECK(animation->GetProperty(property) != nullptr);
    }
    for (StringView method : { "Play", "Pause", "Stop" })
    {
        CAPTURE(method);
        CHECK(animation->GetMethod(method, 0) != nullptr);
    }
}

TEST_CASE("Managed material API exposes explicit alpha routing", "[Mono][Renderer][Materials][.ProcessIsolated]")
{
    AttachThread();

    Crowny::MonoAssembly* assembly = MonoManager::Get().GetAssembly(CROWNY_ASSEMBLY);
    if (assembly == nullptr)
    {
        const Path assemblyPath = Crowny::Test::ResolveManagedAssembly("CrownySharp.dll", "Crowny-Sharp/CrownySharp.dll");
        REQUIRE(fs::is_regular_file(assemblyPath));
        assembly = &MonoManager::Get().LoadAssembly(assemblyPath, CROWNY_ASSEMBLY);
    }

    Crowny::MonoClass* alphaMode = assembly->GetClass(CROWNY_NS, "AlphaMode");
    Crowny::MonoClass* material = assembly->GetClass(CROWNY_NS, "Material");
    REQUIRE(alphaMode != nullptr);
    REQUIRE(material != nullptr);
    CHECK(material->GetProperty("AlphaModeOverride") != nullptr);
    CHECK(material->GetProperty("HasAlphaModeOverride") != nullptr);
    CHECK(material->GetMethod("ClearAlphaModeOverride", 0) != nullptr);
}
