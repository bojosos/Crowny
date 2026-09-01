#include <catch2/catch_test_macros.hpp>

#include "Crowny/Ecs/Components.h"
#include "Crowny/Scene/Scene.h"
#include "Crowny/Scene/SceneManager.h"
#include "Crowny/Scripting/Managed/Interop/ManagedAbiValidation.h"
#include "Crowny/Scripting/Managed/Interop/ManagedHostBindings.h"
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

    class ScopedActiveScene
    {
    public:
        explicit ScopedActiveScene(const Ref<Scene>& scene) : m_OwnsManager(!SceneManager::IsStartedUp())
        {
            if (m_OwnsManager)
                SceneManager::StartUp();
            else
                m_PreviousScene = SceneManager::Get().GetActiveScene();
            SceneManager::Get().SetActiveScene(scene);
        }

        ~ScopedActiveScene()
        {
            if (m_OwnsManager)
                SceneManager::Shutdown();
            else
                SceneManager::Get().SetActiveScene(m_PreviousScene);
        }

    private:
        bool m_OwnsManager;
        Ref<Scene> m_PreviousScene;
    };

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
        mover.Fields.push_back(speed);
        mover.Events.push_back(ScriptEventKind::Start);
        mover.Events.push_back(ScriptEventKind::FixedUpdate);

        ScriptCatalog catalog;
        catalog.ManifestVersion = MANAGED_CATALOG_VERSION;
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
    REQUIRE(scripting.Dispatch(created.Handle, ScriptEvent::Lifecycle(ScriptEventKind::FixedUpdate, 0.02f)).Succeeded);
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

TEST_CASE("Managed state normalization uses the exact current schema", "[Scripting][Managed][Contract]")
{
    ScriptCatalog catalog = MakeCatalog();
    ScriptState source;
    source.Identity = catalog.Types.front().Identity;
    source.Root = ScriptValue::Object({ { "Speed", ScriptValue::Float(4.0) }, { "Removed", ScriptValue::Signed(7) } });

    const ScriptStateResult normalized = NormalizeScriptState(source, catalog.Types.front(), ManagedBackendId::GeneratedMetadata);
    REQUIRE(normalized.Result.Succeeded);
    CHECK(normalized.State.Identity == catalog.Types.front().Identity);
    CHECK((normalized.State.Root.Members == Map<String, ScriptValue>{ { "Speed", ScriptValue::Float(4.0) } }));

    source.Identity = { "GameAssembly", "OldNamespace", "Mover" };
    CHECK_FALSE(NormalizeScriptState(source, catalog.Types.front(), ManagedBackendId::GeneratedMetadata).Result.Succeeded);
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
    CHECK(CW_MANAGED_ABI_VERSION == 14);

    cw_managed_program_api api{};
    api.size = sizeof(api);
    api.abi_version = CW_MANAGED_ABI_VERSION + 1;
    const ManagedOperationResult result = ValidateManagedProgramApi(api, ManagedBackendId::CoreCLR);
    CHECK_FALSE(result.Succeeded);
    CHECK(result.HasDiagnosticCode("managed.abi.version_mismatch"));
}

TEST_CASE("Managed host ABI exposes complete typed bindings and stable value layouts", "[Scripting][Managed][Contract]")
{
    cw_managed_host_api api{};
    PopulateManagedHostBindings(api);
    size_t missingBindings = 0;
#define CW_COUNT_MISSING_BINDING(functionName, fieldName) missingBindings += api.fieldName == nullptr ? 1u : 0u;
    CW_MANAGED_HOST_FUNCTION_LIST(CW_COUNT_MISSING_BINDING)
#undef CW_COUNT_MISSING_BINDING
    CHECK(missingBindings == 0);
    CHECK(api.text_hit_test != nullptr);
    CHECK(api.font_get_is_valid != nullptr);
    CHECK(api.font_get_character_info != nullptr);
    CHECK(api.font_clear_fallbacks != nullptr);
    CHECK(api.collider2d_get_material_override != nullptr);
    CHECK(api.collider2d_set_material_override != nullptr);
    CHECK(api.collider3d_get_material_override != nullptr);
    CHECK(api.collider3d_set_material_override != nullptr);
    CHECK(sizeof(cw_managed_font_character_info) == 112);
    CHECK(sizeof(cw_managed_physics_material_override) == 28);
    CHECK(offsetof(cw_managed_font_character_info, source_font) == 0);
    CHECK(offsetof(cw_managed_font_character_info, advance) == 32);
    CHECK(offsetof(cw_managed_font_character_info, valid) == 105);
}

TEST_CASE("Managed entity parent accepts the empty UUID as unparent", "[Scripting][Managed][Contract][Hierarchy]")
{
    const UUID childId("11111111-2222-3333-4444-555555555555");
    const cw_managed_uuid managedChildId = {
        { 0x11, 0x11, 0x11, 0x11, 0x22, 0x22, 0x33, 0x33, 0x44, 0x44, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55 }
    };
    Ref<Scene> scene = CreateRef<Scene>(false);
    ScopedActiveScene activeScene(scene);
    Entity parent = scene->CreateEntity("Parent");
    Entity child = scene->CreateEntityWithUuid(childId, "Child");
    REQUIRE(child.SetParent(parent));

    cw_managed_host_api api{};
    PopulateManagedHostBindings(api);
    int context = 0;
    const cw_managed_uuid emptyParent{};

    REQUIRE(api.set_entity_parent(&context, managedChildId, emptyParent) == CW_MANAGED_STATUS_OK);
    CHECK_FALSE(child.GetParent());
    CHECK(parent.GetChildCount() == 0u);
}

TEST_CASE("Managed collider material overrides round trip through the shared host table",
          "[Scripting][Managed][Contract][Physics]")
{
    const UUID entity2DId("11111111-2222-3333-4444-555555555555");
    const UUID entity3DId("aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee");
    const cw_managed_uuid managedEntity2DId = {
        { 0x11, 0x11, 0x11, 0x11, 0x22, 0x22, 0x33, 0x33, 0x44, 0x44, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55 }
    };
    const cw_managed_uuid managedEntity3DId = {
        { 0xaa, 0xaa, 0xaa, 0xaa, 0xbb, 0xbb, 0xcc, 0xcc, 0xdd, 0xdd, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee }
    };

    Ref<Scene> scene = CreateRef<Scene>(false);
    ScopedActiveScene activeScene(scene);
    const Entity entity2D = scene->CreateEntityWithUuid(entity2DId, "Managed 2D collider");
    const Entity entity3D = scene->CreateEntityWithUuid(entity3DId, "Managed 3D collider");
    entity2D.AddComponent<BoxCollider2DComponent>();
    entity3D.AddComponent<SphereCollider3DComponent>();

    cw_managed_host_api api{};
    PopulateManagedHostBindings(api);
    int context = 0;

    cw_managed_physics_material_override override2D{};
    override2D.fields = static_cast<uint32_t>(PhysicsMaterialOverrideBits::Friction) |
                        static_cast<uint32_t>(PhysicsMaterialOverrideBits::RestitutionCombine);
    override2D.friction = 0.85f;
    override2D.restitution_combine = static_cast<int32_t>(PhysicsCombineMode::Multiply);
    REQUIRE(api.collider2d_set_material_override(&context, managedEntity2DId, &override2D) == CW_MANAGED_STATUS_OK);

    cw_managed_physics_material_override roundTripped2D{};
    REQUIRE(api.collider2d_get_material_override(&context, managedEntity2DId, &roundTripped2D) == CW_MANAGED_STATUS_OK);
    CHECK(roundTripped2D.fields == override2D.fields);
    CHECK(roundTripped2D.friction == 0.85f);
    CHECK(roundTripped2D.restitution_combine == static_cast<int32_t>(PhysicsCombineMode::Multiply));
    CHECK(entity2D.GetComponent<BoxCollider2DComponent>().GetMaterialData().Friction == 0.85f);

    cw_managed_physics_material_override override3D{};
    override3D.fields = static_cast<uint32_t>(PhysicsMaterialOverrideBits::Density) |
                        static_cast<uint32_t>(PhysicsMaterialOverrideBits::RestitutionThreshold);
    override3D.density = 2.5f;
    override3D.restitution_threshold = 1.75f;
    REQUIRE(api.collider3d_set_material_override(&context, managedEntity3DId, &override3D) == CW_MANAGED_STATUS_OK);

    cw_managed_physics_material_override roundTripped3D{};
    REQUIRE(api.collider3d_get_material_override(&context, managedEntity3DId, &roundTripped3D) == CW_MANAGED_STATUS_OK);
    CHECK(roundTripped3D.fields == override3D.fields);
    CHECK(roundTripped3D.density == 2.5f);
    CHECK(roundTripped3D.restitution_threshold == 1.75f);
    CHECK(entity3D.GetComponent<SphereCollider3DComponent>().GetMaterialData().Density == 2.5f);
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

    const ScriptCatalog& catalog = scripting.GetScriptCatalog();
    const auto probe = std::find_if(catalog.Types.begin(), catalog.Types.end(), [](const ScriptTypeSchema& type) {
        return type.Identity == ScriptTypeIdentity{ "GameAssembly", "Sandbox", "CoreClrIntegrationProbe" };
    });
    REQUIRE(probe != catalog.Types.end());

    const UUID entityId("11111111-2222-3333-4444-555555555555");
    Ref<Scene> scene = CreateRef<Scene>(false);
    ScopedActiveScene activeScene(scene);
    const Entity entity = scene->CreateEntityWithUuid(entityId, "CoreCLR integration host");

    ScriptCreateRequest request;
    request.Identity = probe->Identity;
    request.Entity = entityId;
    request.InitialState.Identity = request.Identity;
    request.InitialState.Root = ScriptValue::Object({}, request.Identity);
    const ScriptCreateResult created = scripting.CreateScript(request);
    INFO(DescribeDiagnostics(created.Result));
    REQUIRE(created.Result.Succeeded);
    CHECK(entity.HasComponent<CameraComponent>());

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
        { 1, "Id", ScriptValueKind::Uuid },
        { 2, "Direction", ScriptValueKind::Vector3 },
        { 3, "Unsigned", ScriptValueKind::UnsignedInteger },
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
    const String json = R"({"ManifestVersion":2,"Types":[{"StableId":11,"Assembly":"GameAssembly","Namespace":"Game",)"
                        R"("TypeName":"Outer+Mover","BaseType":null,"RunInEditor":true,"Events":["Start","FixedUpdate"],"Fields":[]}]})";
    ScriptCatalog catalog;
    const ManagedOperationResult result = ParseManagedCatalogJson(json, catalog, ManagedBackendId::CoreCLR);
    REQUIRE(result.Succeeded);
    REQUIRE(catalog.Types.size() == 1);
    CHECK(catalog.FindType({ "GameAssembly", "Game", "Outer+Mover" }) == &catalog.Types.front());
    CHECK(catalog.FindType({ "GameAssembly", "Game", "Mover" }) == nullptr);
    CHECK((catalog.Types.front().Flags & ScriptTypeFlags::RunInEditor) != ScriptTypeFlags::None);
    CHECK(std::find(catalog.Types.front().Events.begin(), catalog.Types.front().Events.end(), ScriptEventKind::FixedUpdate) !=
          catalog.Types.front().Events.end());

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
