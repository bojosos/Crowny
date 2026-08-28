#include <catch2/catch_test_macros.hpp>

#include "Crowny/Scripting/Managed/Interop/ManagedAbiValidation.h"
#include "Crowny/Scripting/Managed/Interop/ManagedJson.h"
#include "Crowny/Scripting/Managed/ManagedBackendSelection.h"
#include "Crowny/Scripting/Managed/ManagedProgramPackage.h"
#include "Crowny/Scripting/Managed/ManagedScripting.h"

#include <cstddef>
#include <cstdlib>

using namespace Crowny;

namespace
{
    String GetCoreClrTestPackage()
    {
#ifdef CW_PLATFORM_WIN32
        char* value = nullptr;
        size_t length = 0;
        if (_dupenv_s(&value, &length, "CROWNY_CORECLR_TEST_PACKAGE") != 0 || value == nullptr)
            return {};
        const String result(value);
        std::free(value);
        return result;
#else
        const char* value = std::getenv("CROWNY_CORECLR_TEST_PACKAGE");
        return value == nullptr ? String() : String(value);
#endif
    }

    String DescribeDiagnostics(const ManagedOperationResult& result)
    {
        String description;
        for (const ManagedDiagnostic& diagnostic : result.Diagnostics)
        {
            if (!description.empty())
                description += '\n';
            description += diagnostic.Code + ": " + diagnostic.Message;
            if (!diagnostic.ManagedStack.empty())
                description += '\n' + diagnostic.ManagedStack;
        }
        return description;
    }

    ScriptCatalog MakeCatalog()
    {
        ScriptFieldSchema speed;
        speed.StableId = 0x2001;
        speed.Name = "Speed";
        speed.ValueKind = ScriptValueKind::Float;
        speed.Flags = ScriptSchemaFieldFlags::Serializable | ScriptSchemaFieldFlags::Inspectable;

        ScriptTypeSchema mover;
        mover.StableId = 0x1001;
        mover.Identity = { "GameAssembly", "Contract", "Mover" };
        mover.FormerIdentities.push_back({ "GameAssembly", "Legacy", "Mover" });
        mover.Fields.push_back(speed);
        mover.Events.push_back(ScriptEventKind::Start);

        ScriptCatalog catalog;
        catalog.ManifestVersion = 1;
        catalog.ManifestHash = 0xC0DE;
        catalog.Types.push_back(std::move(mover));
        return catalog;
    }

    ScriptState MakeState(float speed)
    {
        ScriptState state;
        state.Identity = { "GameAssembly", "Contract", "Mover" };
        state.Root = ScriptValue::Object({ { "Speed", ScriptValue::Float(speed) } }, state.Identity);
        return state;
    }
} // namespace

TEST_CASE("Generated metadata backend exercises the managed scripting contract", "[Scripting][Managed][Contract]")
{
    ManagedScripting scripting;
    ManagedScriptingConfig config;
    config.Backend = ManagedBackendId::GeneratedMetadata;
    config.ExecutionMode = ManagedExecutionMode::Aot;
    REQUIRE(scripting.Start(config).Succeeded);
    CHECK_FALSE(scripting.GetCapabilities().RuntimeReflection);
    CHECK(scripting.GetCapabilities().AotOnly);

    ManagedProgramDefinition program;
    program.Generation = 1;
    program.Catalog = MakeCatalog();
    REQUIRE(scripting.LoadProgram(program).Succeeded);
    REQUIRE(scripting.GetScriptCatalog().FindType({ "GameAssembly", "Contract", "Mover" }) != nullptr);

    ScriptCreateRequest request;
    request.Identity = { "GameAssembly", "Contract", "Mover" };
    request.Entity = UUID("11111111-2222-3333-4444-555555555555");
    request.InitialState = MakeState(3.5f);
    const ScriptCreateResult created = scripting.CreateScript(request);
    REQUIRE(created.Result.Succeeded);
    REQUIRE(created.Handle.IsValid());

    REQUIRE(scripting.Dispatch(created.Handle, ScriptEvent::Lifecycle(ScriptEventKind::Start)).Succeeded);
    const ScriptStateResult captured = scripting.CaptureState(created.Handle);
    REQUIRE(captured.Result.Succeeded);
    CHECK(captured.State == MakeState(3.5f));

    REQUIRE(scripting.ApplyState(created.Handle, MakeState(9.0f)).Succeeded);
    const ScriptStateResult edited = scripting.CaptureState(created.Handle);
    REQUIRE(edited.Result.Succeeded);
    CHECK(edited.State == MakeState(9.0f));

    REQUIRE(scripting.DestroyScript(created.Handle).Succeeded);
    const ManagedOperationResult staleDispatch = scripting.Dispatch(created.Handle, ScriptEvent::Lifecycle(ScriptEventKind::Start));
    CHECK_FALSE(staleDispatch.Succeeded);
    CHECK(staleDispatch.HasDiagnosticCode("managed.instance.stale_handle"));

    scripting.Shutdown();
}

TEST_CASE("Managed state migration honors former names and preserves unknown fields", "[Scripting][Managed][Contract]")
{
    ScriptCatalog catalog = MakeCatalog();
    catalog.Types.front().Fields.front().FormerNames = { "Velocity" };
    ScriptState legacy;
    legacy.Identity = { "GameAssembly", "Legacy", "Mover" };
    legacy.Root = ScriptValue::Object({ { "Velocity", ScriptValue::Float(4.0) }, { "Removed", ScriptValue::Signed(7) } });

    const ScriptStateResult migrated = MigrateScriptState(legacy, catalog.Types.front(), ManagedBackendId::GeneratedMetadata);
    REQUIRE(migrated.Result.Succeeded);
    CHECK(migrated.State.Identity == catalog.Types.front().Identity);
    CHECK(migrated.State.Root.Members.at("Speed") == ScriptValue::Float(4.0));
    CHECK(migrated.State.OrphanedMembers.at("Removed") == ScriptValue::Signed(7));
}

TEST_CASE("Managed backend presets resolve without exposing runtime objects", "[Scripting][Managed][Contract]")
{
    const ManagedBackendSelection editor = ResolveManagedBackendPreset(ManagedBackendPreset::CoreCLR);
    CHECK(editor.Runtime.Backend == ManagedBackendId::CoreCLR);
    CHECK(editor.Runtime.ExecutionMode == ManagedExecutionMode::Jit);
    CHECK(editor.SupportsProgramReload);
    CHECK_FALSE(editor.ClosedWorld);

    const ManagedBackendSelection browser = ResolveManagedBackendPreset(ManagedBackendPreset::DotNetWasmAOT);
    CHECK(browser.Runtime.Backend == ManagedBackendId::DotNetWasm);
    CHECK(browser.Runtime.ExecutionMode == ManagedExecutionMode::Aot);
    CHECK(browser.ClosedWorld);
    CHECK(browser.RequiresGeneratedMetadata);

    CHECK(GetManagedBackendAvailability(ManagedBackendPreset::CoreCLR, BuildPlatform::WindowsX64, BuildConfiguration::Development, false).Available);
    const ManagedBackendAvailability invalidEditor =
      GetManagedBackendAvailability(ManagedBackendPreset::DotNetWasmInterpreter, BuildPlatform::WindowsX64, BuildConfiguration::Development, true);
    CHECK_FALSE(invalidEditor.Available);
    CHECK_FALSE(invalidEditor.Reason.empty());
    CHECK_FALSE(
      GetManagedBackendAvailability(ManagedBackendPreset::NativeAOT, BuildPlatform::LinuxX64, BuildConfiguration::Development, false).Available);
}

TEST_CASE("Managed ABI rejects incompatible tables before invoking them", "[Scripting][Managed][Contract]")
{
    CHECK(CW_MANAGED_ABI_VERSION == 9);

    cw_managed_program_api api{};
    api.size = sizeof(api);
    api.abi_version = CW_MANAGED_ABI_VERSION + 1;
    const ManagedOperationResult result = ValidateManagedProgramApi(api, ManagedBackendId::CoreCLR);
    CHECK_FALSE(result.Succeeded);
    CHECK(result.HasDiagnosticCode("managed.abi.version_mismatch"));
}

TEST_CASE("Managed font ABI exposes stable glyph and fallback data", "[Scripting][Managed][Contract][Font]")
{
    CHECK(CW_MANAGED_BINDING_TEXT_HIT_TEST == 268);
    CHECK(CW_MANAGED_BINDING_FONT_GET_IS_VALID == 300);
    CHECK(CW_MANAGED_BINDING_FONT_GET_CHARACTER_INFO == 307);
    CHECK(CW_MANAGED_BINDING_FONT_CLEAR_FALLBACKS == 311);
    CHECK(sizeof(cw_managed_font_character_info) == 112);
    CHECK(offsetof(cw_managed_font_character_info, source_font) == 0);
    CHECK(offsetof(cw_managed_font_character_info, advance) == 32);
    CHECK(offsetof(cw_managed_font_character_info, valid) == 105);
}

TEST_CASE("CoreCLR adapter validates its private runtime before activation", "[Scripting][Managed][CoreCLR]")
{
    ManagedScripting scripting;
    ManagedScriptingConfig config;
    config.Backend = ManagedBackendId::CoreCLR;
    config.ExecutionMode = ManagedExecutionMode::Jit;

    const ManagedOperationResult result = scripting.Start(config);
    CHECK_FALSE(result.Succeeded);
    CHECK(result.HasDiagnosticCode("managed.coreclr.runtime_root_missing"));
    CHECK_FALSE(result.HasDiagnosticCode("managed.backend.unavailable"));
}

TEST_CASE("CoreCLR adapter loads a published private package", "[Scripting][Managed][CoreCLR][Integration]")
{
    const String manifestPath = GetCoreClrTestPackage();
    if (manifestPath.empty())
        SKIP("Set CROWNY_CORECLR_TEST_PACKAGE to a published managed-program.json to run the CoreCLR integration test.");

    const ManagedProgramPackageResult loaded = LoadManagedProgramPackage(Path(manifestPath), 1);
    const String packageDiagnostic = DescribeDiagnostics(loaded.Result);
    INFO(packageDiagnostic);
    REQUIRE(loaded.Result.Succeeded);

    ManagedScripting scripting;
    const ManagedOperationResult started = scripting.Start(loaded.Package.Runtime);
    const String startupDiagnostic = DescribeDiagnostics(started);
    INFO(startupDiagnostic);
    REQUIRE(started.Succeeded);

    const ManagedOperationResult programLoaded = scripting.LoadProgram(loaded.Package.Program);
    const String loadDiagnostic = DescribeDiagnostics(programLoaded);
    INFO(loadDiagnostic);
    REQUIRE(programLoaded.Succeeded);
    ManagedOperationResult updateResult;
    updateResult.Diagnostics = scripting.Update();
    const String runtimeDiagnostics = DescribeDiagnostics(updateResult);
    INFO(runtimeDiagnostics);
    REQUIRE_FALSE(scripting.GetScriptCatalog().Types.empty());

    ScriptCreateRequest request;
    request.Identity = scripting.GetScriptCatalog().Types.front().Identity;
    request.Entity = UUID("11111111-2222-3333-4444-555555555555");
    request.InitialState.Identity = request.Identity;
    request.InitialState.Root = ScriptValue::Object({}, request.Identity);
    const ScriptCreateResult created = scripting.CreateScript(request);
    INFO(DescribeDiagnostics(created.Result));
    REQUIRE(created.Result.Succeeded);

    const ScriptStateResult beforeReload = scripting.CaptureState(created.Handle);
    INFO(DescribeDiagnostics(beforeReload.Result));
    REQUIRE(beforeReload.Result.Succeeded);
    ManagedProgramDefinition replacement = loaded.Package.Program;
    replacement.Generation = 2;
    const ManagedOperationResult reloaded = scripting.ReloadProgram(replacement);
    INFO(DescribeDiagnostics(reloaded));
    REQUIRE(reloaded.Succeeded);

    const ScriptStateResult afterReload = scripting.CaptureState(created.Handle);
    INFO(DescribeDiagnostics(afterReload.Result));
    REQUIRE(afterReload.Result.Succeeded);
    CHECK(afterReload.State == beforeReload.State);
    REQUIRE(scripting.DestroyScript(created.Handle).Succeeded);
    scripting.Shutdown();
}

TEST_CASE("Managed JSON uses catalog kinds to preserve ambiguous values", "[Scripting][Managed][Contract]")
{
    ScriptTypeSchema schema;
    schema.Identity = { "GameAssembly", "Contract", "StateCarrier" };
    schema.Fields = {
        { 1, "Id", {}, ScriptValueKind::Uuid },
        { 2, "Direction", {}, ScriptValueKind::Vector3 },
        { 3, "Unsigned", {}, ScriptValueKind::UnsignedInteger },
    };

    ScriptState source;
    source.Identity = schema.Identity;
    ScriptValue id;
    id.Kind = ScriptValueKind::Uuid;
    id.ReferenceValue = UUID("11111111-2222-3333-4444-555555555555");
    ScriptValue direction;
    direction.Kind = ScriptValueKind::Vector3;
    direction.VectorValue = glm::vec4(1.0f, 2.0f, 3.0f, 0.0f);
    source.Root = ScriptValue::Object({ { "Id", id }, { "Direction", direction }, { "Unsigned", ScriptValue::Unsigned(42) } }, source.Identity);

    ScriptState parsed;
    const String json = WriteManagedStateJson(source);
    const ManagedOperationResult result = ParseManagedStateJson(json, parsed, ManagedBackendId::CoreCLR, &schema);
    REQUIRE(result.Succeeded);
    CHECK(parsed == source);
}

TEST_CASE("Managed catalog preserves unambiguous nested script identities", "[Scripting][Managed][Contract]")
{
    const String json = R"({"ManifestVersion":1,"Types":[{"StableId":11,"Assembly":"GameAssembly","Namespace":"Game",)"
                        R"("TypeName":"Outer+Mover","FormerIdentities":[{"Assembly":"GameAssembly","Namespace":"Game",)"
                        R"("TypeName":"Mover"}],"BaseType":null,"RunInEditor":true,"Events":["Start"],"Fields":[]}]})";
    ScriptCatalog catalog;
    const ManagedOperationResult result = ParseManagedCatalogJson(json, catalog, ManagedBackendId::CoreCLR);
    REQUIRE(result.Succeeded);
    REQUIRE(catalog.Types.size() == 1);
    CHECK(catalog.FindType({ "GameAssembly", "Game", "Outer+Mover" }) == &catalog.Types.front());
    CHECK(catalog.FindType({ "GameAssembly", "Game", "Mover" }) == &catalog.Types.front());
    CHECK((catalog.Types.front().Flags & ScriptTypeFlags::RunInEditor) != ScriptTypeFlags::None);

    String invalid = json;
    const size_t event = invalid.find("Start");
    REQUIRE(event != String::npos);
    invalid.replace(event, 5, "Unknown");
    CHECK_FALSE(ParseManagedCatalogJson(invalid, catalog, ManagedBackendId::CoreCLR).Succeeded);
}

TEST_CASE("CoreCLR package manifest resolves only package-local artifacts", "[Scripting][Managed][CoreCLR]")
{
    const Path packageRoot = fs::temp_directory_path() / ("crowny-managed-package-" + UuidGenerator::Generate().ToString());
    struct Cleanup
    {
        Path Root;
        ~Cleanup() { fs::remove_all(Root); }
    } cleanup{ packageRoot };

    fs::create_directories(packageRoot / "runtime");
    fs::create_directories(packageRoot / "host");
    fs::create_directories(packageRoot / "game");
    for (const Path& path : { packageRoot / "host" / "nethost.dll", packageRoot / "host" / "Crowny.ManagedHost.dll",
                              packageRoot / "host" / "Crowny.ManagedHost.deps.json", packageRoot / "host" / "Crowny.ManagedHost.runtimeconfig.json",
                              packageRoot / "game" / "GameAssembly.dll", packageRoot / "game" / "GameAssembly.deps.json" })
        std::ofstream(path).put('\n');

    const Path manifest = packageRoot / "managed-program.json";
    std::ofstream(manifest) << R"({"schemaVersion":1,"abiVersion":)" << CW_MANAGED_ABI_VERSION
                            << R"(,"backend":"CoreCLR","runtimeRoot":"runtime","artifacts":{)"
                            << R"("nethost":"host/nethost.dll","hostAssembly":"host/Crowny.ManagedHost.dll",)"
                            << R"("hostDependencies":"host/Crowny.ManagedHost.deps.json",)"
                            << R"("runtimeConfig":"host/Crowny.ManagedHost.runtimeconfig.json","gameAssembly":"game/GameAssembly.dll",)"
                            << R"("gameDependencies":"game/GameAssembly.deps.json"}})";

    const ManagedProgramPackageResult loaded = LoadManagedProgramPackage(manifest, 7);
    REQUIRE(loaded.Result.Succeeded);
    CHECK(loaded.Package.Runtime.Backend == ManagedBackendId::CoreCLR);
    CHECK(loaded.Package.Runtime.RuntimeRoot == fs::weakly_canonical(packageRoot / "runtime"));
    CHECK(loaded.Package.Program.Generation == 7);
    CHECK(loaded.Package.Program.Artifacts.size() == 6);

    std::ofstream(manifest, std::ios::trunc) << R"({"schemaVersion":1,"abiVersion":)" << CW_MANAGED_ABI_VERSION
                                             << R"(,"backend":"CoreCLR","runtimeRoot":"..","artifacts":{}})";
    const ManagedProgramPackageResult escaped = LoadManagedProgramPackage(manifest);
    CHECK_FALSE(escaped.Result.Succeeded);
    CHECK(escaped.Result.HasDiagnosticCode("managed.package.runtime_path_invalid"));
}
