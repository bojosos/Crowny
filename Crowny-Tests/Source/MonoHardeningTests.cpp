#include <catch2/catch_test_macros.hpp>

#include "Crowny/Application/Application.h"
#include "Crowny/Application/EngineRuntime.h"
#include "Crowny/Scene/Scene.h"
#include "Crowny/Scene/SceneManager.h"
#include "Crowny/Scripting/Bindings/Scene/ScriptEntity.h"
#include "Crowny/Scripting/Managed/Internal/ManagedBackend.h"
#include "Crowny/Scripting/Managed/ManagedScripting.h"
#include "Crowny/Scripting/Mono/MonoAssembly.h"
#include "Crowny/Scripting/Mono/MonoClass.h"
#include "Crowny/Scripting/Mono/MonoField.h"
#include "Crowny/Scripting/Mono/MonoManager.h"
#include "Crowny/Scripting/Mono/MonoMethod.h"
#include "Crowny/Scripting/Mono/MonoUtils.h"
#include "Crowny/Scripting/ScriptObjectManager.h"
#include "ManagedTestPaths.h"

#include <mono/metadata/class.h>
#include <mono/metadata/exception.h>
#include <mono/metadata/object.h>

using namespace Crowny;

void AttachThread();

namespace
{
    class ActiveMonoTestScene
    {
    public:
        explicit ActiveMonoTestScene(const Ref<Scene>& scene) : m_OwnsManager(!SceneManager::IsStartedUp())
        {
            if (m_OwnsManager)
                SceneManager::StartUp();
            else
                m_Previous = SceneManager::Get().GetActiveScene();
            SceneManager::Get().SetActiveScene(scene);
        }
        ~ActiveMonoTestScene()
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

    class MonoTestScriptObjects
    {
    public:
        MonoTestScriptObjects() : m_OwnsManager(!ScriptObjectManager::IsStartedUp())
        {
            if (m_OwnsManager)
                ScriptObjectManager::StartUp();
        }
        ~MonoTestScriptObjects()
        {
            if (m_OwnsManager)
                ScriptObjectManager::Shutdown();
        }

    private:
        bool m_OwnsManager;
    };
} // namespace

TEST_CASE("Mono invocation returns managed exceptions with their stack", "[Mono][Scripting][Hardening]")
{
    AttachThread();
    MonoUtils::DrainDiagnostics();
    ::MonoMethod* parse = mono_class_get_method_from_name(MonoUtils::GetI32Class(), "Parse", 1);
    REQUIRE(parse != nullptr);
    Crowny::MonoMethod method(parse);
    MonoString* text = MonoUtils::ToMonoString("not an integer");
    void* parameters[] = { text };
    MonoObject* exception = nullptr;
    method.Invoke(nullptr, parameters, &exception);
    REQUIRE(exception != nullptr);
    const ManagedDiagnostic diagnostic = MonoUtils::DescribeException(exception);
    CHECK(diagnostic.Backend == ManagedBackendId::Mono);
    CHECK(diagnostic.Severity == ManagedDiagnosticSeverity::Error);
    CHECK(diagnostic.Message.find("FormatException") != String::npos);
    CHECK_FALSE(diagnostic.ManagedStack.empty());
    // Explicit callers return their own operation result without double reporting.
    CHECK(MonoUtils::DrainDiagnostics().empty());

    text = MonoUtils::ToMonoString("42");
    parameters[0] = text;
    MonoObject* value = method.Invoke(nullptr, parameters, &exception);
    CHECK(exception == nullptr);
    REQUIRE(value != nullptr);
    CHECK(*static_cast<int32_t*>(MonoUtils::Unbox(value)) == 42);
}

TEST_CASE("Mono update surfaces queued exceptions once", "[Mono][Scripting][Hardening]")
{
    AttachThread();
    MonoUtils::DrainDiagnostics();
    ::MonoMethod* parse = mono_class_get_method_from_name(MonoUtils::GetI32Class(), "Parse", 1);
    REQUIRE(parse != nullptr);
    Crowny::MonoMethod method(parse);
    void* parameters[] = { MonoUtils::ToMonoString("not an integer") };
    method.Invoke(nullptr, parameters);

    Scope<ManagedBackend> backend = CreateMonoBackend();
    const Vector<ManagedDiagnostic> diagnostics = backend->Update();
    REQUIRE(diagnostics.size() == 1);
    CHECK(diagnostics.front().Message.find("FormatException") != String::npos);
    CHECK_FALSE(diagnostics.front().ManagedStack.empty());
    CHECK(backend->Update().empty());
}

TEST_CASE("Mono exception diagnostics accept an absent managed stack", "[Mono][Scripting][Hardening]")
{
    AttachThread();
    MonoException* exception = mono_get_exception_invalid_operation("No managed throw occurred.");
    REQUIRE(exception != nullptr);
    const ManagedDiagnostic diagnostic = MonoUtils::DescribeException(reinterpret_cast<MonoObject*>(exception));
    CHECK(diagnostic.Message.find("No managed throw occurred.") != String::npos);
    CHECK(diagnostic.ManagedStack.empty());
}

TEST_CASE("Mono UTF16 conversion preserves supplementary characters and embedded nulls", "[Mono][Scripting][Hardening]")
{
    AttachThread();
    const mono_unichar2 input[] = { 0x0041, 0xD83D, 0xDE80, 0x0000, 0x03A9 };
    MonoString* value = mono_string_new_utf16(MonoManager::Get().GetDomain(), input, static_cast<int32_t>(std::size(input)));
    REQUIRE(value != nullptr);
    const String expected("A\xF0\x9F\x9A\x80\0\xCE\xA9", 8);
    CHECK(MonoUtils::FromMonoString(value) == expected);
    CHECK(MonoUtils::FromMonoString(nullptr).empty());
}

TEST_CASE("Destroyed Mono scene wrappers clear the managed finalizer pointer", "[Mono][Scripting][Hardening][.ProcessIsolated]")
{
    AttachThread();
    MonoTestScriptObjects scriptObjects;
    Crowny::MonoAssembly* assembly = MonoManager::Get().GetAssembly(CROWNY_ASSEMBLY);
    if (assembly == nullptr)
    {
        const Path path = Crowny::Test::ResolveManagedAssembly("CrownySharp.dll", "Crowny-Sharp/CrownySharp.dll");
        REQUIRE(fs::is_regular_file(path));
        assembly = &MonoManager::Get().LoadAssembly(path, CROWNY_ASSEMBLY);
    }
    Crowny::MonoClass* type = assembly->GetClass(CROWNY_NS, "Entity");
    REQUIRE(type != nullptr);
    MonoObject* managed = type->CreateInstance(false);
    REQUIRE(managed != nullptr);
    const uint32_t retained = MonoUtils::NewGCHandle(managed, false);
    Ref<Scene> scene = CreateRef<Scene>(false);
    Entity entity = scene->CreateEntity("Finalizer pointer test");
    const auto destroyWrapper = [](ScriptEntity* value) {
        value->NotifyDestroyed();
        delete value;
    };
    std::unique_ptr<ScriptEntity, decltype(destroyWrapper)> wrapper(new ScriptEntity(managed, entity), destroyWrapper);
    REQUIRE(ScriptEntity::ToNative(MonoUtils::GetObjectFromGCHandle(retained)) == wrapper.get());
    wrapper->NotifyDestroyed();
    CHECK(ScriptEntity::ToNative(MonoUtils::GetObjectFromGCHandle(retained)) == nullptr);
    CHECK(wrapper->GetManagedInstance() == nullptr);
    wrapper.reset();
    // Retained managed references can survive native destruction and finalize later.
    CHECK(ScriptEntity::ToNative(MonoUtils::GetObjectFromGCHandle(retained)) == nullptr);
    MonoUtils::FreeGCHandle(retained);
}

TEST_CASE("Mono callback dispatch returns a failure with script context and managed stack", "[Mono][Scripting][Hardening][.ProcessIsolated]")
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
    const Path enginePath = Crowny::Test::ResolveManagedAssembly("CrownySharp.dll", "Crowny-Sharp/CrownySharp.dll");
    const Path gamePath = Crowny::Test::ResolveManagedAssembly("GameAssembly.dll", "Crowny-Sandbox/GameAssembly.dll");
    REQUIRE(fs::is_regular_file(enginePath));
    REQUIRE(fs::is_regular_file(gamePath));
    ManagedProgramDefinition program;
    program.Generation = 1;
    program.Artifacts.push_back({ ManagedProgramArtifactKind::EngineAssembly, CROWNY_ASSEMBLY, enginePath });
    program.Artifacts.push_back({ ManagedProgramArtifactKind::GameAssembly, GAME_ASSEMBLY, gamePath });
    const ManagedOperationResult loaded = scripting->LoadProgram(program);
    REQUIRE((loaded.Succeeded || loaded.HasDiagnosticCode("managed.mono.program_already_loaded")));

    Ref<Scene> scene = CreateRef<Scene>(false);
    ActiveMonoTestScene activeScene(scene);
    Entity entity = scene->CreateEntity("Mono exception test");
    const ScriptTypeIdentity identity{ GAME_ASSEMBLY, "Sandbox", "MonoExceptionProbe" };
    ScriptCreateRequest request;
    request.Identity = identity;
    request.Entity = entity.GetUuid();
    const ScriptCreateResult created = scripting->CreateScript(request);
    REQUIRE(created.Result.Succeeded);
    REQUIRE(created.Handle.IsValid());
    const ManagedOperationResult result = scripting->Dispatch(created.Handle, ScriptEvent::Lifecycle(ScriptEventKind::Update));
    CHECK_FALSE(result.Succeeded);
    CHECK(result.HasDiagnosticCode("managed.mono.callback_failed"));
    if (!result.Diagnostics.empty())
    {
        const ManagedDiagnostic& diagnostic = result.Diagnostics.front();
        CHECK(diagnostic.Message.find("Mono callback diagnostic probe.") != String::npos);
        CHECK(diagnostic.ManagedStack.find("MonoExceptionProbe.Update") != String::npos);
        CHECK(diagnostic.Script == identity);
        CHECK(diagnostic.Entity == entity.GetUuid());
    }
    CHECK(scripting->DestroyScript(created.Handle).Succeeded);
}
