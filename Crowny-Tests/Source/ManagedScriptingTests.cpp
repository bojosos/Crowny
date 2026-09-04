#include <catch2/catch_test_macros.hpp>

#include <limits>

#include "Crowny/Ecs/Components.h"
#include "Crowny/Scene/Scene.h"
#include "Crowny/Scene/SceneManager.h"
#include "Crowny/Scripting/Managed/Interop/ManagedAbiValidation.h"
#include "Crowny/Scripting/Managed/Interop/ManagedHostBindings.h"
#include "Crowny/Scripting/Managed/Interop/ManagedJson.h"
#include "Crowny/Scripting/Managed/ManagedBackendSelection.h"
#include "Crowny/Scripting/Managed/ManagedProgramPackage.h"
#include "Crowny/Scripting/Managed/ManagedScripting.h"
#include "Crowny/Application/Application.h"
#include "Crowny/Application/EngineRuntime.h"
#include "Crowny/Common/Timestep.h"
#include "Crowny/Scene/ScriptRuntime.h"
#include "Crowny/Scripting/Mono/Mono.h"
#include "ManagedTestPaths.h"

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
    CHECK(CW_MANAGED_ABI_VERSION == 19);

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
    const cw_managed_uuid managedChildId = { { 0x11, 0x11, 0x11, 0x11, 0x22, 0x22, 0x33, 0x33, 0x44, 0x44, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55 } };
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

TEST_CASE("Managed collider material overrides round trip through the shared host table", "[Scripting][Managed][Contract][Physics]")
{
    const UUID entity2DId("11111111-2222-3333-4444-555555555555");
    const UUID entity3DId("aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee");
    const cw_managed_uuid managedEntity2DId = { { 0x11, 0x11, 0x11, 0x11, 0x22, 0x22, 0x33, 0x33, 0x44, 0x44, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55 } };
    const cw_managed_uuid managedEntity3DId = { { 0xaa, 0xaa, 0xaa, 0xaa, 0xbb, 0xbb, 0xcc, 0xcc, 0xdd, 0xdd, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee } };

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
    override2D.fields =
      static_cast<uint32_t>(PhysicsMaterialOverrideBits::Friction) | static_cast<uint32_t>(PhysicsMaterialOverrideBits::RestitutionCombine);
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
    override3D.fields =
      static_cast<uint32_t>(PhysicsMaterialOverrideBits::Density) | static_cast<uint32_t>(PhysicsMaterialOverrideBits::RestitutionThreshold);
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

    REQUIRE(probe->Methods.size() == 1);
    const ScriptMethodSchema& button = probe->Methods.front();
    REQUIRE(button.Parameters.size() == 1);
    const ScriptInvocationResult invocation = scripting.InvokeButton(created.Handle, button.StableId,
                                                                      { button.Parameters.front().DefaultValue });
    INFO(DescribeDiagnostics(invocation.Result));
    REQUIRE(invocation.Result.Succeeded);
    REQUIRE(invocation.HasReturnValue);
    CHECK(invocation.ReturnValue.SignedValue == 3);

    const auto conditionalField = std::find_if(probe->Fields.begin(), probe->Fields.end(),
                                                [](const ScriptFieldSchema& field) { return field.Name == "conditionalValue"; });
    REQUIRE(conditionalField != probe->Fields.end());
    const ScriptConditionalSettings* conditions = conditionalField->Attributes.Get<ScriptConditionalSettings>();
    REQUIRE(conditions != nullptr);
    REQUIRE(conditions->Rules.size() == 2);
    CHECK(conditions->Rules[0].Effect == ScriptConditionEffect::Show);
    CHECK(conditions->Rules[0].Condition == "advanced");
    CHECK(conditions->Rules[1].Effect == ScriptConditionEffect::Enable);
    CHECK(conditions->Rules[1].Condition == "CanEdit");
    const ScriptOnValueChangedSettings* valueChanged = conditionalField->Attributes.Get<ScriptOnValueChangedSettings>();
    REQUIRE(valueChanged != nullptr);
    REQUIRE(valueChanged->Actions.size() == 1);
    CHECK(valueChanged->Actions.front().PassValue);
    CHECK(valueChanged->Actions.front().InvokeOnUndoRedo);

    const ScriptStateResult callbackInput = scripting.CaptureState(created.Handle);
    INFO(DescribeDiagnostics(callbackInput.Result));
    REQUIRE(callbackInput.Result.Succeeded);
    ScriptState edited = callbackInput.State;
    edited.Root.Members.at("advanced") = ScriptValue::Boolean(true);
    edited.Root.Members.at("conditionalValue") = ScriptValue::Signed(7);
    REQUIRE(scripting.ApplyState(created.Handle, edited).Succeeded);
    const ScriptInvocationResult callback =
      scripting.InvokeButton(created.Handle, valueChanged->Actions.front().MethodId, { ScriptValue::Signed(7) });
    INFO(DescribeDiagnostics(callback.Result));
    REQUIRE(callback.Result.Succeeded);

    const ScriptStateResult beforeReload = scripting.CaptureState(created.Handle);
    INFO(DescribeDiagnostics(beforeReload.Result));
    REQUIRE(beforeReload.Result.Succeeded);
    REQUIRE(beforeReload.State.Root.Members.contains("value"));
    CHECK(beforeReload.State.Root.Members.at("value").SignedValue == 3);
    CHECK(beforeReload.State.Root.Members.at("callbackValue").SignedValue == 7);
    const ScriptConditionalSettings* resolvedConditions =
      beforeReload.State.Root.Members.at("conditionalValue").Attributes.Get<ScriptConditionalSettings>();
    REQUIRE(resolvedConditions != nullptr);
    REQUIRE(resolvedConditions->Rules.size() == 2);
    CHECK(resolvedConditions->Rules[0].HasResolvedResult);
    CHECK(resolvedConditions->Rules[0].ResolvedResult);
    CHECK(resolvedConditions->Rules[1].HasResolvedResult);
    CHECK(resolvedConditions->Rules[1].ResolvedResult);
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

TEST_CASE("Managed catalog preserves searchable inspector settings", "[Scripting][Managed][Contract]")
{
    const String json =
      R"({"ManifestVersion":2,"Types":[{"StableId":11,"Assembly":"GameAssembly","Namespace":"Game",)"
      R"("TypeName":"Inventory","BaseType":null,"RunInEditor":false,"Searchable":{"FilterOptions":15,"FuzzySearch":true,"Recursive":false},)"
      R"("Events":[],"Fields":[{"StableId":12,"Name":"Items","ValueKind":"List","ElementKind":"Object","KeyKind":null,)"
      R"("DeclaredType":null,"IsNullable":false,"IsSerializable":true,"IsInspectable":true,"IsReadOnly":false,)"
      R"("Searchable":{"FilterOptions":9,"FuzzySearch":false,"Recursive":true}}]}]})";

    ScriptCatalog catalog;
    REQUIRE(ParseManagedCatalogJson(json, catalog, ManagedBackendId::CoreCLR).Succeeded);
    REQUIRE(catalog.Types.size() == 1);
    const ScriptSearchSettings* typeSearch = catalog.Types.front().Attributes.Get<ScriptSearchSettings>();
    REQUIRE(typeSearch != nullptr);
    CHECK(typeSearch->FilterOptions == ScriptSearchFilterOptions::All);
    CHECK(typeSearch->FuzzySearch);
    CHECK_FALSE(typeSearch->Recursive);
    const ScriptSearchSettings* fieldSearch = catalog.Types.front().Fields.front().Attributes.Get<ScriptSearchSettings>();
    REQUIRE(fieldSearch != nullptr);
    CHECK(fieldSearch->FilterOptions ==
          (ScriptSearchFilterOptions::PropertyName | ScriptSearchFilterOptions::ValueToString));
    CHECK_FALSE(fieldSearch->FuzzySearch);
    CHECK(fieldSearch->Recursive);

    String invalid = json;
    const size_t options = invalid.find("\"FilterOptions\":15");
    REQUIRE(options != String::npos);
    invalid.replace(options, String("\"FilterOptions\":15").size(), "\"FilterOptions\":16");
    CHECK_FALSE(ParseManagedCatalogJson(invalid, catalog, ManagedBackendId::CoreCLR).Succeeded);
}

TEST_CASE("Managed inspector attributes are cached by settings type", "[Scripting][Managed][Contract]")
{
    ScriptInspectorAttributeSet attributes;
    attributes.Set(ScriptTooltipSettings{ "Cached tooltip" });
    attributes.Set(ScriptMultilineSettings{ 6 });

    const ScriptTooltipSettings* tooltip = attributes.Get<ScriptTooltipSettings>();
    const ScriptMultilineSettings* multiline = attributes.Get<ScriptMultilineSettings>();
    REQUIRE(tooltip != nullptr);
    REQUIRE(multiline != nullptr);
    CHECK(tooltip->Text == "Cached tooltip");
    CHECK(multiline->Lines == 6);
    CHECK_FALSE(attributes.Has<ScriptSearchSettings>());

    const ScriptInspectorAttributeSet copy = attributes;
    REQUIRE(copy.Get<ScriptTooltipSettings>() != nullptr);
    CHECK(copy.Get<ScriptTooltipSettings>()->Text == "Cached tooltip");
}

TEST_CASE("Managed JSON preserves conditional and value-changed inspector settings", "[Scripting][Managed][Contract]")
{
    const String json =
      R"({"ManifestVersion":2,"Types":[{"StableId":11,"Assembly":"GameAssembly","Namespace":"Game",)"
      R"("TypeName":"ConditionalExample","BaseType":null,"RunInEditor":false,"Events":[],"Fields":[{)"
      R"("StableId":12,"Name":"Target","ValueKind":"SignedInteger","ElementKind":null,"KeyKind":null,)"
      R"("DeclaredType":null,"IsNullable":false,"IsSerializable":true,"IsInspectable":true,"IsReadOnly":false,)"
      R"("Conditions":[{"Effect":0,"Condition":"Advanced","Animate":false,"HasValue":false,"ValueKind":"Null","Value":null},)"
      R"({"Effect":3,"Condition":"Mode","Animate":true,"HasValue":true,"ValueKind":"SignedInteger","Value":2}],)"
      R"("OnValueChanged":[{"Action":"TargetChanged","MethodId":42,"IncludeChildren":true,"InvokeOnInitialize":true,)"
      R"("InvokeOnUndoRedo":false,"PassValue":true}]}]}]})";

    ScriptCatalog catalog;
    REQUIRE(ParseManagedCatalogJson(json, catalog, ManagedBackendId::CoreCLR).Succeeded);
    const ScriptFieldSchema& field = catalog.Types.front().Fields.front();
    const ScriptConditionalSettings* conditions = field.Attributes.Get<ScriptConditionalSettings>();
    REQUIRE(conditions != nullptr);
    REQUIRE(conditions->Rules.size() == 2);
    CHECK(conditions->Rules[0].Effect == ScriptConditionEffect::Show);
    CHECK(conditions->Rules[0].Condition == "Advanced");
    CHECK_FALSE(conditions->Rules[0].Animate);
    CHECK_FALSE(conditions->Rules[0].HasValue);
    CHECK(conditions->Rules[1].Effect == ScriptConditionEffect::Disable);
    CHECK(conditions->Rules[1].HasValue);
    CHECK(conditions->Rules[1].Value.SignedValue == 2);

    const ScriptOnValueChangedSettings* valueChanged = field.Attributes.Get<ScriptOnValueChangedSettings>();
    REQUIRE(valueChanged != nullptr);
    REQUIRE(valueChanged->Actions.size() == 1);
    const ScriptValueChangedAction& action = valueChanged->Actions.front();
    CHECK(action.Action == "TargetChanged");
    CHECK(action.MethodId == 42);
    CHECK(action.IncludeChildren);
    CHECK(action.InvokeOnInitialize);
    CHECK_FALSE(action.InvokeOnUndoRedo);
    CHECK(action.PassValue);

    String invalid = json;
    const size_t methodId = invalid.find("\"MethodId\":42");
    REQUIRE(methodId != String::npos);
    invalid.replace(methodId, String("\"MethodId\":42").size(), "\"MethodId\":0");
    CHECK_FALSE(ParseManagedCatalogJson(invalid, catalog, ManagedBackendId::CoreCLR).Succeeded);
}

TEST_CASE("Managed catalog preserves inspector button methods", "[Scripting][Managed][Contract]")
{
    const String json =
      R"({"ManifestVersion":2,"Types":[{"StableId":11,"Assembly":"GameAssembly","Namespace":"Game",)"
      R"("TypeName":"Actions","BaseType":null,"RunInEditor":false,"Events":[],"Fields":[],"Methods":[{)"
      R"("StableId":21,"Name":"AddScore","IsStatic":false,"ReturnKind":"SignedInteger","DeclaredReturnType":null,)"
      R"("Parameters":[{"Name":"amount","ValueKind":"SignedInteger","DeclaredType":null,"HasDefaultValue":true,"DefaultValue":5}],)"
      R"("Button":{"Name":"Add Score","ButtonHeight":30,"ButtonAlignment":0.25,"Stretch":false,"Style":2,)"
      R"("DisplayParameters":true,"Expanded":true,"DrawResult":true,"DirtyOnClick":false,"Icon":"plus","IconAlignment":1}}]}]})";

    ScriptCatalog catalog;
    REQUIRE(ParseManagedCatalogJson(json, catalog, ManagedBackendId::CoreCLR).Succeeded);
    REQUIRE(catalog.Types.size() == 1);
    REQUIRE(catalog.Types.front().Methods.size() == 1);
    const ScriptMethodSchema& method = catalog.Types.front().Methods.front();
    CHECK(method.StableId == 21);
    CHECK(method.Name == "AddScore");
    CHECK(method.ReturnKind == ScriptValueKind::SignedInteger);
    REQUIRE(method.Parameters.size() == 1);
    CHECK(method.Parameters.front().Name == "amount");
    CHECK(method.Parameters.front().HasDefaultValue);
    CHECK(method.Parameters.front().DefaultValue.SignedValue == 5);
    const ScriptButtonSettings* settings = method.Attributes.Get<ScriptButtonSettings>();
    REQUIRE(settings != nullptr);
    CHECK(settings->Name == "Add Score");
    CHECK(settings->ButtonHeight == 30);
    CHECK(settings->ButtonAlignment == 0.25f);
    CHECK_FALSE(settings->Stretch);
    CHECK(settings->Style == ScriptButtonStyle::FoldoutButton);
    CHECK(settings->Expanded);
    CHECK_FALSE(settings->DirtyOnClick);
    CHECK(settings->Icon == "plus");
    CHECK(settings->IconAlignment == ScriptButtonIconAlignment::Right);

    const Vector<ScriptValue> arguments = { ScriptValue::Signed(9), ScriptValue::Text("go") };
    CHECK(WriteManagedArgumentsJson(arguments) == R"([9,"go"])");
    const ScriptInvocationResult result =
      ParseManagedInvocationResultJson(R"({"HasResult":true,"ResultKind":"String","Result":"done"})", ManagedBackendId::CoreCLR);
    REQUIRE(result.Result.Succeeded);
    REQUIRE(result.HasReturnValue);
    CHECK(result.ReturnValue.Kind == ScriptValueKind::String);
    CHECK(result.ReturnValue.StringValue == "done");
}

TEST_CASE("Managed catalog preserves progress bar inspector settings", "[Scripting][Managed][Contract]")
{
    const String json = R"({"ManifestVersion":2,"Types":[{"StableId":11,"Assembly":"GameAssembly","Namespace":"Game",)"
                        R"("TypeName":"Player","BaseType":null,"RunInEditor":false,"Events":[],"Fields":[{"StableId":12,)"
                        R"("Name":"Health","ValueKind":"Float","ElementKind":null,"KeyKind":null,"DeclaredType":null,)"
                        R"("IsNullable":false,"IsSerializable":true,"IsInspectable":true,"IsReadOnly":false,"ProgressBar":{)"
                        R"("Min":0,"Max":100,"MinGetter":"","MaxGetter":"MaxHealth","R":0.2,"G":0.6,"B":0.3,"Height":18,)"
                        R"("Segmented":true,"DrawValueLabel":true,"ValueLabelAlignment":2,"ColorGetter":"HealthColor",)"
                        R"("BackgroundColorGetter":"BarBackground","CustomValueStringGetter":"HealthLabel"}}]}]})";

    ScriptCatalog catalog;
    REQUIRE(ParseManagedCatalogJson(json, catalog, ManagedBackendId::CoreCLR).Succeeded);
    REQUIRE(catalog.Types.size() == 1);
    const ScriptProgressBarSettings* progressBar =
      catalog.Types.front().Fields.front().Attributes.Get<ScriptProgressBarSettings>();
    REQUIRE(progressBar != nullptr);
    const ScriptProgressBarSettings& settings = *progressBar;
    CHECK(settings.Min == 0.0);
    CHECK(settings.Max == 100.0);
    CHECK(settings.MaxGetter == "MaxHealth");
    CHECK(settings.Color == glm::vec3(0.2f, 0.6f, 0.3f));
    CHECK(settings.Height == 18);
    CHECK(settings.Segmented);
    CHECK(settings.DrawValueLabel);
    CHECK(settings.ValueLabelAlignment == ScriptProgressBarLabelAlignment::Right);
    CHECK(settings.ColorGetter == "HealthColor");
    CHECK(settings.BackgroundColorGetter == "BarBackground");
    CHECK(settings.CustomValueStringGetter == "HealthLabel");

    String invalid = json;
    const size_t height = invalid.find("\"Height\":18");
    REQUIRE(height != String::npos);
    invalid.replace(height, String("\"Height\":18").size(), "\"Height\":0");
    CHECK_FALSE(ParseManagedCatalogJson(invalid, catalog, ManagedBackendId::CoreCLR).Succeeded);
}

TEST_CASE("Managed catalog preserves path and multiline inspector settings", "[Scripting][Managed][Contract]")
{
    const String json = R"({"ManifestVersion":2,"Types":[{"StableId":11,"Assembly":"GameAssembly","Namespace":"Game","TypeName":"Paths",)"
                        R"("BaseType":null,"RunInEditor":false,"Events":[],"Fields":[)"
                        R"({"StableId":12,"Name":"Source","ValueKind":"String","ElementKind":null,"KeyKind":null,"DeclaredType":null,)"
                        R"("IsNullable":false,"IsSerializable":true,"IsInspectable":true,"IsReadOnly":false,"FilePath":{)"
                        R"("AbsolutePath":false,"ParentFolder":"$Root","RequireExistingPath":true,"UseBackslashes":false,)"
                        R"("Extensions":"$Extensions","IncludeFileExtension":false}},)"
                        R"({"StableId":13,"Name":"Output","ValueKind":"String","ElementKind":null,"KeyKind":null,"DeclaredType":null,)"
                        R"("IsNullable":false,"IsSerializable":true,"IsInspectable":true,"IsReadOnly":false,"FolderPath":{)"
                        R"("AbsolutePath":true,"ParentFolder":"","RequireExistingPath":false,"UseBackslashes":true}},)"
                        R"({"StableId":14,"Name":"Description","ValueKind":"String","ElementKind":null,"KeyKind":null,"DeclaredType":null,)"
                        R"("IsNullable":false,"IsSerializable":true,"IsInspectable":true,"IsReadOnly":false,"Multiline":{"Lines":7}}]}]})";

    ScriptCatalog catalog;
    REQUIRE(ParseManagedCatalogJson(json, catalog, ManagedBackendId::CoreCLR).Succeeded);
    REQUIRE(catalog.Types.front().Fields.size() == 3);
    const ScriptPathSettings* fileSettings = catalog.Types.front().Fields[0].Attributes.Get<ScriptPathSettings>();
    REQUIRE(fileSettings != nullptr);
    const ScriptPathSettings& file = *fileSettings;
    CHECK(file.Kind == ScriptPathKind::File);
    CHECK(file.ParentFolder == "$Root");
    CHECK(file.Extensions == "$Extensions");
    CHECK(file.RequireExistingPath);
    CHECK_FALSE(file.IncludeFileExtension);
    const ScriptPathSettings* folderSettings = catalog.Types.front().Fields[1].Attributes.Get<ScriptPathSettings>();
    REQUIRE(folderSettings != nullptr);
    const ScriptPathSettings& folder = *folderSettings;
    CHECK(folder.Kind == ScriptPathKind::Folder);
    CHECK(folder.AbsolutePath);
    CHECK(folder.UseBackslashes);
    const ScriptMultilineSettings* multiline = catalog.Types.front().Fields[2].Attributes.Get<ScriptMultilineSettings>();
    REQUIRE(multiline != nullptr);
    CHECK(multiline->Lines == 7);
}

TEST_CASE("Managed catalog preserves enum button options and flags", "[Scripting][Managed][Contract]")
{
    const String json = R"({"ManifestVersion":2,"Types":[{"StableId":11,"Assembly":"GameAssembly","Namespace":"Game","TypeName":"Access",)"
                        R"("BaseType":null,"RunInEditor":false,"Events":[],"Fields":[{"StableId":12,"Name":"Permissions",)"
                        R"("ValueKind":"Enum","ElementKind":null,"KeyKind":null,"DeclaredType":null,"IsNullable":false,)"
                        R"("IsSerializable":true,"IsInspectable":true,"IsReadOnly":false,"EnumButtons":{"IsFlags":true,)"
                        R"("IsUnsigned":true,"IncludeObsolete":false,"Options":[{"Name":"None","Value":0},{"Name":"Read","Value":1},)"
                        R"({"Name":"Write","Value":2},{"Name":"All","Value":3}]}}]}]})";

    ScriptCatalog catalog;
    REQUIRE(ParseManagedCatalogJson(json, catalog, ManagedBackendId::CoreCLR).Succeeded);
    const ScriptEnumButtonsSettings* enumButtons =
      catalog.Types.front().Fields.front().Attributes.Get<ScriptEnumButtonsSettings>();
    REQUIRE(enumButtons != nullptr);
    const ScriptEnumButtonsSettings& settings = *enumButtons;
    CHECK(settings.IsFlags);
    CHECK(settings.IsUnsigned);
    CHECK_FALSE(settings.IncludeObsolete);
    REQUIRE(settings.Options.size() == 4);
    CHECK(settings.Options[3].Name == "All");
    CHECK(settings.Options[3].Value == 3);

    String invalid = json;
    const size_t kind = invalid.find("\"ValueKind\":\"Enum\"");
    REQUIRE(kind != String::npos);
    invalid.replace(kind, String("\"ValueKind\":\"Enum\"").size(), "\"ValueKind\":\"String\"");
    CHECK_FALSE(ParseManagedCatalogJson(invalid, catalog, ManagedBackendId::CoreCLR).Succeeded);
}

TEST_CASE("Managed state preserves unsigned enum bits", "[Scripting][Managed][Contract]")
{
    const String json = R"({"StateVersion":1,"Assembly":"GameAssembly","Namespace":"Game","TypeName":"Access",)"
                        R"("Metadata":{"Permissions":{"Kind":"Enum","Assembly":"GameAssembly","Namespace":"Game",)"
                        R"("TypeName":"Permissions","EnumUnsigned":true}},"Fields":{"Permissions":18446744073709551615}})";

    ScriptFieldSchema permissions;
    permissions.Name = "Permissions";
    permissions.ValueKind = ScriptValueKind::Enum;
    ScriptTypeSchema schema;
    schema.Fields.push_back(permissions);

    ScriptState state;
    REQUIRE(ParseManagedStateJson(json, state, ManagedBackendId::CoreCLR, &schema).Succeeded);
    const ScriptValue& value = state.Root.Members.at("Permissions");
    CHECK(value.Kind == ScriptValueKind::Enum);
    CHECK(value.EnumUnsigned);
    CHECK(static_cast<uint64_t>(value.SignedValue) == std::numeric_limits<uint64_t>::max());

    const String encoded = WriteManagedStateJson(state);
    CHECK(encoded.find("18446744073709551615") != String::npos);
}

TEST_CASE("Managed catalog preserves every dictionary display layout", "[Scripting][Managed][Contract]")
{
    const String json = R"({"ManifestVersion":2,"DictionaryDisplays":[{"TargetType":{"Assembly":"GameAssembly","Namespace":"Game",)"
                        R"("TypeName":"NestedDictionary"},"Layout":1,"KeyLabel":"Nested key","ValueLabel":"Nested value",)"
                        R"("KeyColumnFraction":0.35}],"Types":[{"StableId":11,"Assembly":"GameAssembly","Namespace":"Game",)"
                        R"("TypeName":"DictionaryExample","BaseType":null,"RunInEditor":false,"Events":[],"Fields":[)"
                        R"({"StableId":12,"Name":"Columns","ValueKind":"Dictionary","ElementKind":"Float","KeyKind":"String",)"
                        R"("DeclaredType":{"Assembly":"GameAssembly","Namespace":"Game","TypeName":"ColumnsDictionary"},"IsNullable":false,)"
                        R"("IsSerializable":true,"IsInspectable":true,"IsReadOnly":false,"DictionaryDisplay":{"Layout":0,"KeyLabel":"Name",)"
                        R"("ValueLabel":"Amount","KeyColumnFraction":0.6}},)"
                        R"({"StableId":13,"Name":"Visible","ValueKind":"Dictionary","ElementKind":"Object","KeyKind":"SignedInteger",)"
                        R"("DeclaredType":{"Assembly":"GameAssembly","Namespace":"Game","TypeName":"VisibleDictionary"},"IsNullable":false,)"
                        R"("IsSerializable":true,"IsInspectable":true,"IsReadOnly":false,"DictionaryDisplay":{"Layout":2,"KeyLabel":"",)"
                        R"("ValueLabel":"","KeyColumnFraction":0.5}}]}]})";

    ScriptCatalog catalog;
    REQUIRE(ParseManagedCatalogJson(json, catalog, ManagedBackendId::CoreCLR).Succeeded);
    REQUIRE(catalog.DictionaryDisplays.size() == 1);
    CHECK(catalog.DictionaryDisplays.front().Display.Layout == ScriptDictionaryLayout::OneColumnWithValueFoldout);
    CHECK(catalog.DictionaryDisplays.front().Display.KeyColumnFraction == Catch::Approx(0.35f));
    REQUIRE(catalog.Types.front().Fields.size() == 2);
    const ScriptDictionaryDisplaySettings* columnSettings =
      catalog.Types.front().Fields[0].Attributes.Get<ScriptDictionaryDisplaySettings>();
    REQUIRE(columnSettings != nullptr);
    const ScriptDictionaryDisplaySettings& columns = *columnSettings;
    CHECK(columns.Layout == ScriptDictionaryLayout::TwoColumns);
    CHECK(columns.KeyLabel == "Name");
    CHECK(columns.ValueLabel == "Amount");
    CHECK(columns.KeyColumnFraction == Catch::Approx(0.6f));
    const ScriptDictionaryDisplaySettings* visibleSettings =
      catalog.Types.front().Fields[1].Attributes.Get<ScriptDictionaryDisplaySettings>();
    REQUIRE(visibleSettings != nullptr);
    const ScriptDictionaryDisplaySettings& visible = *visibleSettings;
    CHECK(visible.Layout == ScriptDictionaryLayout::OneColumnWithValueVisible);
    CHECK(visible.KeyLabel == "Key");
    CHECK(visible.ValueLabel == "Value");

    const ScriptTypeIdentity nestedType{ "GameAssembly", "Game", "NestedDictionary" };
    REQUIRE(catalog.FindDictionaryDisplay(nestedType) != nullptr);
    CHECK(catalog.FindDictionaryDisplay(nestedType)->ValueLabel == "Nested value");

    String invalid = json;
    const size_t fraction = invalid.find("\"KeyColumnFraction\":0.6");
    REQUIRE(fraction != String::npos);
    invalid.replace(fraction, String("\"KeyColumnFraction\":0.6").size(), "\"KeyColumnFraction\":1.5");
    CHECK_FALSE(ParseManagedCatalogJson(invalid, catalog, ManagedBackendId::CoreCLR).Succeeded);
}

TEST_CASE("Managed state preserves nested dictionary display settings", "[Scripting][Managed][Contract]")
{
    const String json = R"({"StateVersion":1,"Assembly":"GameAssembly","Namespace":"Game","TypeName":"DictionaryExample",)"
                        R"("Metadata":{"Values":{"Kind":"Dictionary","Assembly":"GameAssembly","Namespace":"Game",)"
                        R"("TypeName":"ValuesDictionary","DictionaryDisplay":{"Layout":2,"KeyLabel":"Code","ValueLabel":"Text",)"
                        R"("KeyColumnFraction":0.4},"Elements":[]}},"Fields":{"Values":[]}})";

    ScriptFieldSchema values;
    values.Name = "Values";
    values.ValueKind = ScriptValueKind::Dictionary;
    ScriptTypeSchema schema;
    schema.Fields.push_back(values);
    ScriptState state;
    REQUIRE(ParseManagedStateJson(json, state, ManagedBackendId::CoreCLR, &schema).Succeeded);
    const ScriptValue& dictionary = state.Root.Members.at("Values");
    const ScriptDictionaryDisplaySettings* dictionaryDisplay = dictionary.Attributes.Get<ScriptDictionaryDisplaySettings>();
    REQUIRE(dictionaryDisplay != nullptr);
    CHECK(dictionaryDisplay->Layout == ScriptDictionaryLayout::OneColumnWithValueVisible);
    CHECK(dictionaryDisplay->KeyLabel == "Code");
    CHECK(dictionaryDisplay->ValueLabel == "Text");
    CHECK(WriteManagedStateJson(state).find("\"DictionaryDisplay\"") != String::npos);
}

TEST_CASE("Managed catalog preserves field tooltips", "[Scripting][Managed][Contract]")
{
    const String json = R"({"ManifestVersion":2,"Types":[{"StableId":21,"Assembly":"GameAssembly","Namespace":"Game",)"
                        R"("TypeName":"TooltipExample","BaseType":null,"RunInEditor":false,"Events":[],"Fields":[)"
                        R"({"StableId":22,"Name":"Health","ValueKind":"SignedInteger","ElementKind":null,"KeyKind":null,)"
                        R"("DeclaredType":null,"IsNullable":false,"IsSerializable":true,"IsInspectable":true,"IsReadOnly":false,)"
                        R"("Tooltip":"Health value between 0 and 100."}]}]})";

    ScriptCatalog catalog;
    REQUIRE(ParseManagedCatalogJson(json, catalog, ManagedBackendId::CoreCLR).Succeeded);
    REQUIRE(catalog.Types.size() == 1);
    REQUIRE(catalog.Types.front().Fields.size() == 1);
    const ScriptTooltipSettings* tooltip = catalog.Types.front().Fields.front().Attributes.Get<ScriptTooltipSettings>();
    REQUIRE(tooltip != nullptr);
    CHECK(tooltip->Text == "Health value between 0 and 100.");

    String invalid = json;
    const size_t tooltipOffset = invalid.find("\"Tooltip\":\"Health value between 0 and 100.\"");
    REQUIRE(tooltipOffset != String::npos);
    invalid.replace(tooltipOffset, String("\"Tooltip\":\"Health value between 0 and 100.\"").size(), "\"Tooltip\":42");
    CHECK_FALSE(ParseManagedCatalogJson(invalid, catalog, ManagedBackendId::CoreCLR).Succeeded);
}

TEST_CASE("Managed labels do not replace serialized member names", "[Scripting][Managed][Contract]")
{
    const String catalogJson = R"({"ManifestVersion":2,"Types":[{"StableId":41,"Assembly":"GameAssembly","Namespace":"Game",)"
                               R"("TypeName":"LabelExample","BaseType":null,"RunInEditor":false,"Events":[],"Fields":[)"
                               R"({"StableId":42,"Name":"internalHealth","ValueKind":"SignedInteger","ElementKind":null,)"
                               R"("KeyKind":null,"DeclaredType":null,"IsNullable":false,"IsSerializable":true,)"
                               R"("IsInspectable":true,"IsReadOnly":false,"Label":"Health"}]}]})";

    ScriptCatalog catalog;
    REQUIRE(ParseManagedCatalogJson(catalogJson, catalog, ManagedBackendId::CoreCLR).Succeeded);
    const ScriptFieldSchema& field = catalog.Types.front().Fields.front();
    CHECK(field.Name == "internalHealth");
    const ScriptLabelSettings* label = field.Attributes.Get<ScriptLabelSettings>();
    REQUIRE(label != nullptr);
    CHECK(label->Text == "Health");

    const String stateJson = R"({"StateVersion":1,"Assembly":"GameAssembly","Namespace":"Game","TypeName":"LabelExample",)"
                             R"("Metadata":{"internalHealth":{"Kind":"SignedInteger","Label":"Health"}},)"
                             R"("Fields":{"internalHealth":75}})";
    ScriptState state;
    REQUIRE(ParseManagedStateJson(stateJson, state, ManagedBackendId::CoreCLR, &catalog.Types.front()).Succeeded);
    REQUIRE(state.Root.Members.find("internalHealth") != state.Root.Members.end());
    CHECK(state.Root.Members.find("Health") == state.Root.Members.end());

    const String encoded = WriteManagedStateJson(state);
    CHECK(encoded.find("\"Fields\":{\"internalHealth\":75}") != String::npos);
    CHECK(encoded.find("\"Label\":\"Health\"") != String::npos);
}

TEST_CASE("Managed state preserves nested tooltips", "[Scripting][Managed][Contract]")
{
    const String json = R"({"StateVersion":1,"Assembly":"GameAssembly","Namespace":"Game","TypeName":"TooltipExample",)"
                        R"("Metadata":{"Settings":{"Kind":"Object","Assembly":"GameAssembly","Namespace":"Game",)"
                        R"("TypeName":"Settings","Members":{"Speed":{"Kind":"Float","Tooltip":"Units per second."}}}},)"
                        R"("Fields":{"Settings":{"Speed":3.5}}})";

    ScriptFieldSchema settings;
    settings.Name = "Settings";
    settings.ValueKind = ScriptValueKind::Object;
    ScriptTypeSchema schema;
    schema.Fields.push_back(settings);

    ScriptState state;
    REQUIRE(ParseManagedStateJson(json, state, ManagedBackendId::CoreCLR, &schema).Succeeded);
    const ScriptValue& speed = state.Root.Members.at("Settings").Members.at("Speed");
    const ScriptTooltipSettings* tooltip = speed.Attributes.Get<ScriptTooltipSettings>();
    REQUIRE(tooltip != nullptr);
    CHECK(tooltip->Text == "Units per second.");
    CHECK(WriteManagedStateJson(state).find("\"Tooltip\":\"Units per second.\"") != String::npos);
}

TEST_CASE("Managed catalog preserves color usage settings", "[Scripting][Managed][Contract]")
{
    const String json = R"({"ManifestVersion":2,"Types":[{"StableId":31,"Assembly":"GameAssembly","Namespace":"Game",)"
                        R"("TypeName":"ColorUsageExample","BaseType":null,"RunInEditor":false,"Events":[],"Fields":[)"
                        R"({"StableId":32,"Name":"Opaque","ValueKind":"Color","ElementKind":null,"KeyKind":null,"DeclaredType":null,)"
                        R"("IsNullable":false,"IsSerializable":true,"IsInspectable":true,"IsReadOnly":false,)"
                        R"("ColorUsage":{"ShowAlpha":false,"Hdr":false}},)"
                        R"({"StableId":33,"Name":"Emission","ValueKind":"Color","ElementKind":null,"KeyKind":null,"DeclaredType":null,)"
                        R"("IsNullable":false,"IsSerializable":true,"IsInspectable":true,"IsReadOnly":false,)"
                        R"("ColorUsage":{"ShowAlpha":true,"Hdr":true}}]}]})";

    ScriptCatalog catalog;
    REQUIRE(ParseManagedCatalogJson(json, catalog, ManagedBackendId::CoreCLR).Succeeded);
    REQUIRE(catalog.Types.front().Fields.size() == 2);
    const ScriptColorUsageSettings* opaqueSettings = catalog.Types.front().Fields[0].Attributes.Get<ScriptColorUsageSettings>();
    REQUIRE(opaqueSettings != nullptr);
    const ScriptColorUsageSettings& opaque = *opaqueSettings;
    CHECK_FALSE(opaque.ShowAlpha);
    CHECK_FALSE(opaque.Hdr);
    const ScriptColorUsageSettings* emissionSettings = catalog.Types.front().Fields[1].Attributes.Get<ScriptColorUsageSettings>();
    REQUIRE(emissionSettings != nullptr);
    const ScriptColorUsageSettings& emission = *emissionSettings;
    CHECK(emission.ShowAlpha);
    CHECK(emission.Hdr);

    String invalid = json;
    const size_t kind = invalid.find("\"ValueKind\":\"Color\"");
    REQUIRE(kind != String::npos);
    invalid.replace(kind, String("\"ValueKind\":\"Color\"").size(), "\"ValueKind\":\"String\"");
    CHECK_FALSE(ParseManagedCatalogJson(invalid, catalog, ManagedBackendId::CoreCLR).Succeeded);
}

TEST_CASE("Managed state preserves nested color usage settings", "[Scripting][Managed][Contract]")
{
    const String json = R"({"StateVersion":1,"Assembly":"GameAssembly","Namespace":"Game","TypeName":"ColorUsageExample",)"
                        R"("Metadata":{"Settings":{"Kind":"Object","Assembly":"GameAssembly","Namespace":"Game",)"
                        R"("TypeName":"Settings","Members":{"Emission":{"Kind":"Color",)"
                        R"("ColorUsage":{"ShowAlpha":false,"Hdr":true}}}}},)"
                        R"("Fields":{"Settings":{"Emission":[2.0,1.5,0.5,0.75]}}})";

    ScriptFieldSchema settings;
    settings.Name = "Settings";
    settings.ValueKind = ScriptValueKind::Object;
    ScriptTypeSchema schema;
    schema.Fields.push_back(settings);

    ScriptState state;
    REQUIRE(ParseManagedStateJson(json, state, ManagedBackendId::CoreCLR, &schema).Succeeded);
    const ScriptValue& emission = state.Root.Members.at("Settings").Members.at("Emission");
    const ScriptColorUsageSettings* colorUsage = emission.Attributes.Get<ScriptColorUsageSettings>();
    REQUIRE(colorUsage != nullptr);
    CHECK_FALSE(colorUsage->ShowAlpha);
    CHECK(colorUsage->Hdr);
    CHECK(WriteManagedStateJson(state).find("\"ColorUsage\"") != String::npos);
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

namespace
{
    struct LifecycleEntry
    {
        String Callback;
        int Sequence = 0;
    };

    // LifecycleProbe logs "Callback@Sequence" entries separated by commas.
    Vector<LifecycleEntry> ParseLifecycleLog(const String& log)
    {
        Vector<LifecycleEntry> entries;
        size_t begin = 0;
        while (begin <= log.size())
        {
            size_t end = log.find(',', begin);
            if (end == String::npos)
                end = log.size();
            const String entry = log.substr(begin, end - begin);
            const size_t at = entry.find('@');
            if (at != String::npos)
                entries.push_back({ entry.substr(0, at), std::stoi(entry.substr(at + 1)) });
            begin = end + 1;
        }
        return entries;
    }

    Vector<String> LifecycleNames(const Vector<LifecycleEntry>& entries)
    {
        Vector<String> names;
        for (const LifecycleEntry& entry : entries)
            names.push_back(entry.Callback);
        return names;
    }

    int LifecycleSequence(const Vector<LifecycleEntry>& entries, StringView callback)
    {
        for (const LifecycleEntry& entry : entries)
            if (entry.Callback == callback)
                return entry.Sequence;
        return -1;
    }

    Vector<LifecycleEntry> CaptureLifecycleLog(ManagedScripting& scripting, const ManagedScript& script)
    {
        const ScriptStateResult captured = scripting.CaptureState(script.GetRuntimeHandle());
        REQUIRE(captured.Result.Succeeded);
        REQUIRE(captured.State.Root.Members.contains("log"));
        return ParseLifecycleLog(captured.State.Root.Members.at("log").StringValue);
    }

    size_t CountOccurrences(const String& text, StringView needle)
    {
        size_t count = 0;
        for (size_t position = text.find(needle); position != String::npos; position = text.find(needle, position + needle.size()))
            ++count;
        return count;
    }

    ManagedScripting& StartMonoLifecycleFixture()
    {
        if (!Application::IsStartedUp())
        {
            ApplicationDesc description;
            description.Name = "Test";
            description.Headless = true;
            description.WorkingDirectory = fs::current_path();
            Application::StartUp(description);
        }
        const Path engineAssemblyPath = Crowny::Test::ResolveManagedAssembly("CrownySharp.dll", "Crowny-Sharp/CrownySharp.dll");
        const Path gameAssemblyPath = Crowny::Test::ResolveManagedAssembly("GameAssembly.dll", "Crowny-Sandbox/GameAssembly.dll");
        REQUIRE(fs::is_regular_file(engineAssemblyPath));
        REQUIRE(fs::is_regular_file(gameAssemblyPath));
        ManagedScripting* managedScripting = Application::Get().GetRuntime().GetManagedScripting();
        REQUIRE(managedScripting != nullptr);
        ManagedProgramDefinition program;
        program.Generation = 1;
        program.Artifacts.push_back({ ManagedProgramArtifactKind::EngineAssembly, CROWNY_ASSEMBLY, engineAssemblyPath });
        program.Artifacts.push_back({ ManagedProgramArtifactKind::GameAssembly, GAME_ASSEMBLY, gameAssemblyPath });
        ManagedOperationResult loadResult = managedScripting->LoadProgram(program);
        if (!loadResult.Succeeded && loadResult.HasDiagnosticCode("managed.mono.program_already_loaded"))
        {
            // Test cases share one process when the whole isolated lane runs at
            // once, and the program from an earlier test case stays loaded. The
            // assemblies are identical, so the loaded program is simply reused.
            loadResult = ManagedOperationResult::Success();
        }
        if (!loadResult.Succeeded)
        {
            for (const ManagedDiagnostic& diagnostic : loadResult.Diagnostics)
                WARN(diagnostic.Code << ": " << diagnostic.Message);
        }
        REQUIRE(loadResult.Succeeded);
        return *managedScripting;
    }
} // namespace

TEST_CASE("Mono lifecycle awakens every script before Start and runs Update before LateUpdate",
          "[Scripting][Managed][Lifecycle][Mono][.ProcessIsolated]")
{
    ManagedScripting& scripting = StartMonoLifecycleFixture();
    Ref<Scene> scene = CreateRef<Scene>(false);
    ScopedActiveScene activeScene(scene);
    const ScriptTypeIdentity probe{ GAME_ASSEMBLY, "Sandbox", "LifecycleProbe" };
    Entity first = scene->CreateEntity("Lifecycle first");
    Entity second = scene->CreateEntity("Lifecycle second");
    REQUIRE(scene->AddScriptComponent(first, probe, false));
    REQUIRE(scene->AddScriptComponent(second, probe, false));
    const auto firstScript = [&]() -> ManagedScript& { return first.GetComponent<ManagedScriptComponent>().Scripts.front(); };
    const auto secondScript = [&]() -> ManagedScript& { return second.GetComponent<ManagedScriptComponent>().Scripts.front(); };
    CHECK_FALSE(ScriptRuntime::IsScriptAwake(firstScript()));

    ScriptRuntime::OnStart(scene);
    REQUIRE(firstScript().GetRuntimeHandle().IsValid());
    REQUIRE(secondScript().GetRuntimeHandle().IsValid());
    CHECK(ScriptRuntime::IsScriptAwake(firstScript()));
    CHECK(ScriptRuntime::IsScriptAwake(secondScript()));
    Vector<LifecycleEntry> firstLog = CaptureLifecycleLog(scripting, firstScript());
    Vector<LifecycleEntry> secondLog = CaptureLifecycleLog(scripting, secondScript());
    CHECK(LifecycleNames(firstLog) == Vector<String>{ "Awake", "Start" });
    CHECK(LifecycleNames(secondLog) == Vector<String>{ "Awake", "Start" });
    // Both scripts awaken before either starts.
    CHECK(LifecycleSequence(firstLog, "Awake") < LifecycleSequence(secondLog, "Start"));
    CHECK(LifecycleSequence(secondLog, "Awake") < LifecycleSequence(firstLog, "Start"));

    // OnStart is idempotent for awakened scripts.
    ScriptRuntime::OnStart(scene);
    CHECK(CaptureLifecycleLog(scripting, firstScript()).size() == 2);

    const Timestep step(1.0f / 60.0f);
    ScriptRuntime::OnUpdate(scene, step);
    firstLog = CaptureLifecycleLog(scripting, firstScript());
    secondLog = CaptureLifecycleLog(scripting, secondScript());
    CHECK(LifecycleNames(firstLog) == Vector<String>{ "Awake", "Start", "Update", "LateUpdate" });
    CHECK(LifecycleNames(secondLog) == Vector<String>{ "Awake", "Start", "Update", "LateUpdate" });
    // Every Update finishes before the first LateUpdate of the frame.
    CHECK(LifecycleSequence(firstLog, "Update") < LifecycleSequence(secondLog, "LateUpdate"));
    CHECK(LifecycleSequence(secondLog, "Update") < LifecycleSequence(firstLog, "LateUpdate"));

    // Split phases let a frame loop run animation between Update and LateUpdate.
    ScriptRuntime::OnUpdate(scene, step, false);
    CHECK(LifecycleNames(CaptureLifecycleLog(scripting, firstScript())).back() == "Update");
    ScriptRuntime::OnLateUpdate(scene, step);
    CHECK(LifecycleNames(CaptureLifecycleLog(scripting, firstScript())).back() == "LateUpdate");
    ScriptRuntime::OnFixedUpdate(scene, Timestep(0.02f));
    CHECK(LifecycleNames(CaptureLifecycleLog(scripting, firstScript())).back() == "FixedUpdate");

    ScriptRuntime::OnShutdown(scene);
}

TEST_CASE("Mono lifecycle delivers OnDestroy on entity destruction and scene stop for awakened scripts only",
          "[Scripting][Managed][Lifecycle][Mono][.ProcessIsolated]")
{
    ManagedScripting& scripting = StartMonoLifecycleFixture();
    Ref<Scene> scene = CreateRef<Scene>(false);
    ScopedActiveScene activeScene(scene);
    const ScriptTypeIdentity probe{ GAME_ASSEMBLY, "Sandbox", "LifecycleProbe" };
    Entity sink = scene->CreateEntity("LifecycleProbeSink");
    Entity kept = scene->CreateEntity("Lifecycle kept");
    Entity destroyed = scene->CreateEntity("Lifecycle destroyed");
    REQUIRE(scene->AddScriptComponent(kept, probe, false));
    REQUIRE(scene->AddScriptComponent(destroyed, probe, false));
    ScriptRuntime::OnStart(scene);
    ScriptRuntime::OnUpdate(scene, Timestep(1.0f / 60.0f));
    CHECK(sink.GetName() == "LifecycleProbeSink");

    // Destroying the entity while other scripts stay alive delivers OnDestroy exactly once, with the full history.
    scene->DestroyEntity(destroyed);
    CHECK_FALSE(scene->TryGetEntityFromUuid(destroyed.GetUuid()));
    String sinkName = sink.GetName();
    CHECK(CountOccurrences(sinkName, "OnDestroy@") == 1);
    CHECK(CountOccurrences(sinkName, "Awake@") == 1);
    CHECK(CountOccurrences(sinkName, "LateUpdate@") == 1);
    // Surviving scripts keep updating after a sibling was destroyed mid-scene.
    ScriptRuntime::OnUpdate(scene, Timestep(1.0f / 60.0f));
    CHECK(LifecycleNames(CaptureLifecycleLog(scripting, kept.GetComponent<ManagedScriptComponent>().Scripts.front())).size() == 6);

    // A constructed but never awakened script (editor-style instance) is destroyed silently.
    Entity silent = scene->CreateEntity("Lifecycle silent");
    REQUIRE(scene->AddScriptComponent(silent, probe, false));
    ManagedScript& silentScript = silent.GetComponent<ManagedScriptComponent>().Scripts.front();
    REQUIRE(ScriptRuntime::CreateScript(silent, silentScript, false));
    CHECK_FALSE(ScriptRuntime::IsScriptAwake(silent.GetComponent<ManagedScriptComponent>().Scripts.front()));
    scene->DestroyEntity(silent);
    CHECK(sink.GetName() == sinkName);

    // Removing the script component (not the entity) also delivers OnDestroy.
    Entity removed = scene->CreateEntity("Lifecycle removed");
    REQUIRE(scene->AddScriptComponent(removed, probe, false));
    // StartScript ignores scripts that were not constructed yet.
    ScriptRuntime::StartScript(removed, removed.GetComponent<ManagedScriptComponent>().Scripts.front());
    CHECK_FALSE(ScriptRuntime::IsScriptAwake(removed.GetComponent<ManagedScriptComponent>().Scripts.front()));
    REQUIRE(ScriptRuntime::CreateScript(removed, removed.GetComponent<ManagedScriptComponent>().Scripts.front(), true));
    CHECK(ScriptRuntime::IsScriptAwake(removed.GetComponent<ManagedScriptComponent>().Scripts.front()));
    scene->RemoveScriptComponent(removed, probe);
    sinkName = sink.GetName();
    CHECK(CountOccurrences(sinkName, "OnDestroy@") == 2);
    CHECK_FALSE(removed.HasComponent<ManagedScriptComponent>());

    // Scene stop destroys every remaining awakened script and clears its runtime handle.
    ScriptRuntime::OnShutdown(scene);
    sinkName = sink.GetName();
    CHECK(CountOccurrences(sinkName, "OnDestroy@") == 3);
    const ManagedScript& keptScript = kept.GetComponent<ManagedScriptComponent>().Scripts.front();
    CHECK_FALSE(keptScript.GetRuntimeHandle().IsValid());
    CHECK_FALSE(ScriptRuntime::IsScriptAwake(keptScript));

    // A second stop is a no-op: nothing is awake anymore.
    ScriptRuntime::OnShutdown(scene);
    CHECK(sink.GetName() == sinkName);
}
