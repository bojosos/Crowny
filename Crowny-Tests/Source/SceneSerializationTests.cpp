#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "Crowny/Application/Application.h"
#include "Crowny/Application/EngineRuntime.h"
#include "Crowny/Assets/AssetManager.h"
#include "Crowny/Common/ConsoleBuffer.h"
#include "Crowny/Common/FileSystem.h"
#include "Crowny/Common/Log.h"
#include "Crowny/Common/Yaml.h"
#include "Crowny/Ecs/Components.h"
#include "Crowny/Ecs/Entity.h"
#include "Crowny/Physics/Physics2D.h"
#include "Crowny/Physics/Physics3D.h"
#include "Crowny/Renderer/Font.h"
#include "Crowny/Renderer/TextLayout.h"
#include "Crowny/Scene/Scene.h"
#include "Crowny/Scene/SceneManager.h"
#include "Crowny/Scene/ScriptRuntime.h"
#include "Crowny/Scripting/Managed/ManagedScripting.h"
#include "Crowny/Serialization/CerealDataStreamArchive.h"
#include "Crowny/Serialization/SceneComponentCodec.h"
#include "Crowny/Serialization/SceneSerializer.h"
#include "ManagedTestPaths.h"

using namespace Crowny;

static_assert(std::is_nothrow_move_constructible_v<ManagedScript>);
static_assert(std::is_nothrow_move_assignable_v<ManagedScript>);

namespace
{
    constexpr TextLayoutFontData UNIT_LAYOUT_FONT{ 0.8, -0.2, 1.0, 1.0, 1.0, 4, nullptr };

    void AddLayoutToken(TextLayoutScratch& scratch, char32_t codePoint, bool whitespace = false, bool newLine = false)
    {
        TextLayoutToken& token = scratch.Tokens.Acquire();
        token = {};
        token.CodePoint = codePoint;
        token.Advance = newLine ? 0.0 : 1.0;
        token.NewLine = newLine;
        token.WhiteSpace = whitespace;
        token.BreakAfter = whitespace;
        token.Renderable = !whitespace && !newLine;
    }

    TextComponent MakeUnitTextComponent()
    {
        TextComponent component;
        component.Text = "prepared";
        component.Size = 36.0f;
        component.VerticalAlignment = TextVerticalAlignment::Baseline;
        return component;
    }

    ScriptState MakeScriptState(const ScriptTypeIdentity& identity, int64_t value)
    {
        ScriptState state;
        state.Identity = identity;
        state.Root = ScriptValue::Object({ { "Value", ScriptValue::Signed(value) } }, identity);
        return state;
    }

    class ScopedAssetManifestRegistration
    {
    public:
        ScopedAssetManifestRegistration(AssetManager& manager, const Ref<AssetManifest>& manifest)
          : m_Manager(manager), m_Manifest(manifest)
        {
            m_Manager.RegisterAssetManifest(m_Manifest);
        }

        ~ScopedAssetManifestRegistration() { m_Manager.UnregisterAssetManifest(m_Manifest); }

        ScopedAssetManifestRegistration(const ScopedAssetManifestRegistration&) = delete;
        ScopedAssetManifestRegistration& operator=(const ScopedAssetManifestRegistration&) = delete;

    private:
        AssetManager& m_Manager;
        Ref<AssetManifest> m_Manifest;
    };

} // namespace

// Minimal fixture for scene serialization tests.
class SerializationTestFixture
{
public:
    SerializationTestFixture()
    {
        if (!Application::IsStartedUp())
        {
            ApplicationDesc desc;
            desc.Name = "Test";
            desc.Headless = true;
            desc.WorkingDirectory = fs::current_path();
            Application::StartUp(desc);
        }
    }

    ~SerializationTestFixture() {}
};

class ScopedSceneManager
{
public:
    ScopedSceneManager() : m_OwnsInstance(!SceneManager::IsStartedUp())
    {
        if (m_OwnsInstance)
            SceneManager::StartUp();
        else
            m_PreviousScene = SceneManager::Get().GetActiveScene();
    }

    ~ScopedSceneManager()
    {
        if (m_OwnsInstance)
            SceneManager::Shutdown();
        else
            SceneManager::Get().SetActiveScene(m_PreviousScene);
    }

private:
    bool m_OwnsInstance;
    Ref<Scene> m_PreviousScene;
};

TEST_CASE("ManagedScript vector moves preserve runtime identity", "[Scene][Scripting][Lifetime]")
{
    Vector<ManagedScript> scripts;
    scripts.reserve(1);
    scripts.emplace_back(ScriptTypeIdentity{ GAME_ASSEMBLY, "Sandbox", "First" });
    const uint64_t firstId = scripts.front().InstanceId;

    scripts.emplace_back(ScriptTypeIdentity{ GAME_ASSEMBLY, "Sandbox", "Second" });
    const uint64_t secondId = scripts.back().InstanceId;
    CHECK(scripts.front().InstanceId == firstId);

    scripts.erase(scripts.begin());
    REQUIRE(scripts.size() == 1);
    CHECK(scripts.front().InstanceId == secondId);
}

TEST_CASE("ManagedScript retains backend-neutral state without a runtime instance", "[Serialization][Scripting][ScriptState]")
{
    const ScriptTypeIdentity identity{ "Missing.Assembly", "Missing.Namespace", "MissingType" };
    ScriptState input = MakeScriptState(identity, 37);

    ManagedScript script(identity);
    REQUIRE(script.SetState(input));
    CHECK(script.GetState() == input);
    CHECK_FALSE(script.GetRuntimeHandle().IsValid());

    ManagedScript copied(script);
    CHECK(copied.GetTypeIdentity() == identity);
    CHECK(copied.GetState() == input);
    CHECK_FALSE(copied.GetRuntimeHandle().IsValid());
    ManagedScript assigned(identity);
    assigned = script;
    CHECK(assigned.GetTypeIdentity() == identity);
    CHECK(assigned.GetState() == input);
    CHECK_FALSE(assigned.GetRuntimeHandle().IsValid());
}

TEST_CASE("ManagedScript rejects state for a different type", "[Serialization][Scripting][ScriptState]")
{
    const ScriptTypeIdentity identity{ "Missing.Assembly", "Missing.Namespace", "StateCarrier" };
    ManagedScript script(identity);
    const ScriptState original = script.GetState();
    CHECK_FALSE(script.SetState(MakeScriptState({ "Other.Assembly", "Other.Namespace", "OtherType" }, 23)));
    CHECK(script.GetState() == original);
}

TEST_CASE("Missing managed scripts round-trip with exact identity and state", "[Serialization][Scripting][ScriptState]")
{
    SerializationTestFixture fixture;
    const ScriptTypeIdentity identity{ "Unavailable.Assembly", "Preserved.Namespace", "GhostBehaviour" };
    ScriptState state = MakeScriptState(identity, 91);
    Ref<Scene> scene = CreateRef<Scene>(false);
    scene->SetName("Missing scripts");
    Entity entity = scene->CreateEntity("Script host");
    REQUIRE(scene->AddScriptComponent(entity, state, false));

    const auto verify = [&](const Ref<Scene>& loaded) {
        const auto view = loaded->GetAllEntitiesWith<ManagedScriptComponent>();
        REQUIRE(view.size() == 1);
        const ManagedScriptComponent& component = view.get<ManagedScriptComponent>(*view.begin());
        REQUIRE(component.Scripts.size() == 1);
        CHECK(component.Scripts.front().GetState() == state);
    };

    SECTION("YAML")
    {
        const Path path = fs::temp_directory_path() / "crowny-missing-script-state.yaml";
        SceneSerializer(scene).Serialize(path);
        const String yaml = FileSystem::OpenFile(path)->GetAsString();
        CHECK(yaml.find("Unavailable.Assembly") != String::npos);
        CHECK(yaml.find("Preserved.Namespace") != String::npos);
        CHECK(yaml.find("GhostBehaviour") != String::npos);
        Ref<Scene> loaded = CreateRef<Scene>(false);
        REQUIRE(SceneSerializer(loaded).Deserialize(path));
        verify(loaded);
        fs::remove(path);
    }

    SECTION("Binary")
    {
        const Path path = fs::temp_directory_path() / "crowny-missing-script-state.cwb";
        SceneSerializer(scene).SerializeBinary(path);
        Ref<Scene> loaded = CreateRef<Scene>(false);
        REQUIRE(SceneSerializer(loaded).DeserializeBinary(path));
        verify(loaded);
        fs::remove(path);
    }
}

TEST_CASE("Mono applies retained ScriptState and survives constructor reentry",
          "[Serialization][Scripting][ScriptState][Mono][.ProcessIsolated]")
{
    SerializationTestFixture fixture;
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

    ScopedSceneManager sceneManagerScope;
    Ref<Scene> stateScene = CreateRef<Scene>(false);
    SceneManager::Get().SetActiveScene(stateScene);
    Entity stateEntity = stateScene->CreateEntity("Mono state host");
    const ScriptTypeIdentity stateIdentity{ GAME_ASSEMBLY, "Sandbox", "CameraFollow" };
    ScriptState retainedState;
    retainedState.Identity = stateIdentity;
    retainedState.Root = ScriptValue::Object({ { "smoothSpeed", ScriptValue::Float(7.25) } }, stateIdentity);
    REQUIRE(stateScene->AddScriptComponent(stateEntity, retainedState, false));
    ManagedScript& stateScript = stateEntity.GetComponent<ManagedScriptComponent>().Scripts.front();
    const uint64_t stateInstanceId = stateScript.InstanceId;
    REQUIRE(ScriptRuntime::CreateScript(stateEntity, stateScript, false));
    ManagedScript* liveStateScript = stateEntity.GetComponent<ManagedScriptComponent>().FindScript(stateInstanceId);
    REQUIRE(liveStateScript != nullptr);
    REQUIRE(liveStateScript->GetRuntimeHandle().IsValid());
    const ScriptStateResult capturedState = managedScripting->CaptureState(liveStateScript->GetRuntimeHandle());
    REQUIRE(capturedState.Result.Succeeded);
    REQUIRE(capturedState.State.Root.Members.at("smoothSpeed").Kind == ScriptValueKind::Float);
    CHECK(capturedState.State.Root.Members.at("smoothSpeed").FloatingValue == Catch::Approx(7.25));

    Entity metadataEntity = stateScene->CreateEntity("Mono metadata host");
    const ScriptTypeIdentity metadataIdentity{ GAME_ASSEMBLY, "Sandbox", "Test" };
    REQUIRE(stateScene->AddScriptComponent(metadataEntity, metadataIdentity, false));
    ManagedScript& metadataScript = metadataEntity.GetComponent<ManagedScriptComponent>().Scripts.front();
    const uint64_t metadataInstanceId = metadataScript.InstanceId;
    REQUIRE(ScriptRuntime::CreateScript(metadataEntity, metadataScript, false));
    ManagedScript* liveMetadataScript = metadataEntity.GetComponent<ManagedScriptComponent>().FindScript(metadataInstanceId);
    REQUIRE(liveMetadataScript != nullptr);
    const ScriptStateResult metadataState = managedScripting->CaptureState(liveMetadataScript->GetRuntimeHandle());
    REQUIRE(metadataState.Result.Succeeded);
    const ScriptValue& mode = metadataState.State.Root.Members.at("dummyEnumInspector");
    CHECK(mode.Kind == ScriptValueKind::Enum);
    CHECK(mode.DeclaredType == (ScriptTypeIdentity{ GAME_ASSEMBLY, "Sandbox", "Test+DrawMode" }));
    const ScriptValue& structs = metadataState.State.Root.Members.at("TestStructList");
    REQUIRE(structs.Kind == ScriptValueKind::List);
    REQUIRE(structs.Elements.size() == 1);
    CHECK(structs.Elements.front().Kind == ScriptValueKind::Object);
    CHECK(structs.Elements.front().DeclaredType == (ScriptTypeIdentity{ GAME_ASSEMBLY, "Sandbox", "Test+TestStruct" }));

    // A managed constructor can append to the same script vector. Force the
    // vector to its current capacity so the nested append must relocate it.
    Ref<Scene> reentryScene = CreateRef<Scene>(false);
    SceneManager::Get().SetActiveScene(reentryScene);
    Entity reentryEntity = reentryScene->CreateEntity("Mono constructor reentry");
    const ScriptTypeIdentity reentryIdentity{ GAME_ASSEMBLY, "Sandbox", "ConstructorReentryProbe" };
    REQUIRE(reentryScene->AddScriptComponent(reentryEntity, reentryIdentity, false));

    ManagedScriptComponent& before = reentryEntity.GetComponent<ManagedScriptComponent>();
    const uint64_t reentryInstanceId = before.Scripts.front().InstanceId;
    uint32_t paddingIndex = 0;
    while (before.Scripts.size() < before.Scripts.capacity())
    {
        before.Scripts.emplace_back(
          ScriptTypeIdentity{ "Test.Padding", "Sandbox", "Padding" + std::to_string(paddingIndex++) });
    }
    const size_t saturatedSize = before.Scripts.size();
    REQUIRE(saturatedSize == before.Scripts.capacity());
    ManagedScript* initiatingBeforeCreate = before.FindScript(reentryInstanceId);
    REQUIRE(initiatingBeforeCreate != nullptr);
    REQUIRE(ScriptRuntime::CreateScript(reentryEntity, *initiatingBeforeCreate, false));

    ManagedScriptComponent& after = reentryEntity.GetComponent<ManagedScriptComponent>();
    REQUIRE(after.Scripts.size() == saturatedSize + 1);
    ManagedScript* initiating = after.FindScript(reentryInstanceId);
    REQUIRE(initiating != nullptr);
    CHECK(initiating->GetTypeIdentity() == reentryIdentity);
    REQUIRE(initiating->GetRuntimeHandle().IsValid());
    const ScriptStateResult captured = managedScripting->CaptureState(initiating->GetRuntimeHandle());
    REQUIRE(captured.Result.Succeeded);

    const ScriptTypeIdentity nestedIdentity{ GAME_ASSEMBLY, "Sandbox", "CameraFollow" };
    const auto nested = std::find_if(after.Scripts.begin(), after.Scripts.end(),
                                     [&](const ManagedScript& candidate) { return candidate.GetTypeIdentity() == nestedIdentity; });
    REQUIRE(nested != after.Scripts.end());
    CHECK(nested->GetRuntimeHandle().IsValid());
}

TEST_CASE("Nested managed type identities round-trip through scene formats", "[Serialization][Scripting][TypeIdentity]")
{
    SerializationTestFixture fixture;
    const ScriptTypeIdentity identity{ "Host.Managed", "Persisted", "MissingBehaviour" };
    const ScriptTypeIdentity enumIdentity{ "Enum.Managed", "Persisted", "Container+Mode" };
    const ScriptTypeIdentity payloadIdentity{ "Object.Managed", "Persisted", "Container+Payload" };
    ScriptValue mode = ScriptValue::Signed(2);
    mode.Kind = ScriptValueKind::Enum;
    mode.DeclaredType = enumIdentity;
    ScriptState state;
    state.Identity = identity;
    state.Root = ScriptValue::Object(
      { { "Mode", std::move(mode) }, { "Payload", ScriptValue::Object({ { "Value", ScriptValue::Signed(17) } }, payloadIdentity) } }, identity);
    Ref<Scene> scene = CreateRef<Scene>(false);
    scene->SetName("Nested managed metadata");
    Entity entity = scene->CreateEntity("Script host");
    REQUIRE(scene->AddScriptComponent(entity, state, false));

    const auto verify = [&](const Ref<Scene>& loaded) {
        const auto view = loaded->GetAllEntitiesWith<ManagedScriptComponent>();
        REQUIRE(view.size() == 1);
        const ManagedScriptComponent& component = view.get<ManagedScriptComponent>(*view.begin());
        REQUIRE(component.Scripts.size() == 1);
        CHECK(component.Scripts.front().GetState() == state);
    };

    SECTION("YAML")
    {
        const Path path = fs::temp_directory_path() / ("crowny-nested-type-" + UuidGenerator::Generate().ToString() + ".yaml");
        SceneSerializer(scene).Serialize(path);
        Ref<Scene> loaded = CreateRef<Scene>(false);
        REQUIRE(SceneSerializer(loaded).Deserialize(path));
        verify(loaded);
        fs::remove(path);
    }

    SECTION("Binary")
    {
        const Path path = fs::temp_directory_path() / ("crowny-nested-type-" + UuidGenerator::Generate().ToString() + ".cwb");
        SceneSerializer(scene).SerializeBinary(path);
        Ref<Scene> loaded = CreateRef<Scene>(false);
        REQUIRE(SceneSerializer(loaded).DeserializeBinary(path));
        verify(loaded);
        fs::remove(path);
    }
}

TEST_CASE("Duplicate managed script identities are rejected", "[Scene][Scripting][ScriptState]")
{
    Ref<Scene> scene = CreateRef<Scene>(false);
    Entity entity = scene->CreateEntity("Script host");
    const ScriptTypeIdentity identity{ "Assembly", "Namespace", "Behaviour" };
    REQUIRE(scene->AddScriptComponent(entity, identity, false));
    CHECK_FALSE(scene->AddScriptComponent(entity, identity, false));
    REQUIRE(entity.HasComponent<ManagedScriptComponent>());
    CHECK(entity.GetComponent<ManagedScriptComponent>().Scripts.size() == 1);
    CHECK_FALSE(scene->AddScriptComponent(entity, ScriptTypeIdentity{ "", "Namespace", "Malformed" }, false));
}

TEST_CASE("Complex Scene Serialization", "[Serialization]")
{
    SerializationTestFixture fixture;

    Ref<Scene> scene = CreateRef<Scene>(false);
    scene->SetName("ComplexScene");

    // Create a hierarchy:
    // Parent (10, 0, 0)
    //   +- Child1 (5, 0, 0) -> World (15, 0, 0)
    //   +- Child2 (0, 10, 0) -> World (10, 10, 0)
    //      +- GrandChild (0, 0, 5) -> World (10, 10, 5)

    Entity parent = scene->CreateEntity("Parent");
    parent.SetPosition({ 10.0f, 0.0f, 0.0f });
    auto& light = parent.AddComponent<LightComponent>();
    light.Type = LightType::Spot;
    light.Color = { 0.4f, 0.6f, 0.8f };
    light.Intensity = 2400.0f;
    light.Range = 31.0f;
    light.SpotInnerAngle = glm::radians(18.0f);
    light.SpotOuterAngle = glm::radians(37.0f);
    light.SourceRadius = 0.15f;
    light.UseColorTemperature = true;
    light.Temperature = 4200.0f;
    light.VisibilityLayers.Value = 0x00ff00ffu;
    light.AffectSpecular = false;
    light.Volumetric = true;
    light.Shadows.Mode = LightShadowMode::Soft;
    light.Shadows.Bias = 0.003f;
    light.Shadows.NormalBias = 0.04f;
    light.Shadows.NearPlane = 0.2f;
    light.Shadows.Importance = 2.5f;
    light.Shadows.Resolution = 2048;
    light.Shadows.CacheStaticCasters = false;

    Entity child1 = scene->CreateEntity("Child1");
    child1.SetParent(parent);
    child1.SetPosition({ 5.0f, 0.0f, 0.0f });

    auto& asc = child1.AddComponent<AudioSourceComponent>();
    asc.SetVolume(0.5f);
    asc.SetPitch(1.5f);
    asc.SetLooping(true);
    auto& rb2d = child1.AddComponent<Rigidbody2DComponent>();
    rb2d.SetAutoMass(false, child1);
    rb2d.SetCenterOfMass({ 0.25f, -0.5f });
    rb2d.SetInertia(2.75f);
    rb2d.SetLayerMask(99, child1);

    Entity child2 = scene->CreateEntity("Child2");
    child2.SetParent(parent);
    child2.SetPosition({ 0.0f, 10.0f, 0.0f });
    auto& rb3d = child2.AddComponent<Rigidbody3DComponent>();
    rb3d.SetBodyType(PhysicsBodyType3D::Dynamic, child2);
    rb3d.SetMass(3.5f, child2);
    rb3d.SetAutoMass(false, child2);
    rb3d.SetGravityScale(0.75f);
    rb3d.SetContinuousCollision(true, child2);
    auto& box3d = child2.AddComponent<BoxCollider3DComponent>();
    box3d.SetSize({ 2.0f, 3.0f, 4.0f }, child2);
    box3d.SetOffset({ 0.5f, 0.0f, -0.5f }, child2);
    Ref<PhysicsMaterial3D> material3d = CreateRef<PhysicsMaterial3D>();
    material3d->SetDensity(2.0f);
    material3d->SetFriction(0.25f);
    material3d->SetRestitution(0.6f);
    material3d->SetFrictionCombine(PhysicsCombineMode::Minimum);
    const AssetHandle<PhysicsMaterial3D> material3dHandle =
      static_asset_cast<PhysicsMaterial3D>(AssetManager::TryGet()->CreateAssetHandle(material3d));
    box3d.SetMaterial(material3dHandle);

    Entity grandChild = scene->CreateEntity("GrandChild");
    grandChild.SetParent(child2);
    grandChild.SetPosition({ 0.0f, 0.0f, 5.0f });
    auto& text = grandChild.AddComponent<TextComponent>();
    text.Text = "Aligned text";
    text.Size = 42.0f;
    text.AutoSize = true;
    text.AutoSizeMin = 12.0f;
    text.AutoSizeMax = 60.0f;
    text.LayoutSize = { 8.0f, 3.0f };
    text.Wrapping = true;
    text.WrapMode = TextWrapMode::WordThenCharacter;
    text.Overflow = TextOverflow::Ellipsis;
    text.ClipToBounds = true;
    text.MaxLines = 4;
    text.HorizontalAlignment = TextHorizontalAlignment::Justified;
    text.VerticalAlignment = TextVerticalAlignment::Middle;
    text.FontStyle = TextFontStyleBits::Underline | TextFontStyleBits::Strikethrough;
    text.CharacterSpacing = 0.05f;
    text.WordSpacing = 0.1f;
    text.LineSpacing = 0.2f;
    text.ParagraphSpacing = 0.35f;
    text.UseCustomDecorationColor = true;
    text.DecorationColor = { 0.2f, 0.4f, 0.6f, 0.8f };
    text.DecorationThickness = 0.03f;
    text.UnderlineOffset = -0.04f;
    text.StrikethroughOffset = 0.02f;
    text.SortingLayer = -3;
    text.OrderInLayer = 17;

    auto& sprite = child1.AddComponent<SpriteRendererComponent>();
    sprite.Color = { 0.25f, 0.5f, 0.75f, 1.0f };
    sprite.SortingLayer = 4;
    sprite.OrderInLayer = -9;

    Path testPath = "complex_scene.yaml";

    SECTION("Serialize and Deserialize Hierarchy and Components")
    {
        {
            SceneSerializer serializer(scene);
            serializer.Serialize(testPath);
        }

        Ref<Scene> deserializedScene = CreateRef<Scene>(false);
        {
            SceneSerializer serializer(deserializedScene);
            serializer.Deserialize(testPath);
        }

        Entity dParent = deserializedScene->FindEntityByName("Parent");
        Entity dChild1 = deserializedScene->FindEntityByName("Child1");
        Entity dChild2 = deserializedScene->FindEntityByName("Child2");
        Entity dGrandChild = deserializedScene->FindEntityByName("GrandChild");

        REQUIRE(dParent);
        REQUIRE(dChild1);
        REQUIRE(dChild2);
        REQUIRE(dGrandChild);

        CHECK(dChild1.GetParent() == dParent);
        CHECK(dChild2.GetParent() == dParent);
        CHECK(dGrandChild.GetParent() == dChild2);

        CHECK(dParent.GetWorldPosition() == glm::vec3(10.0f, 0.0f, 0.0f));
        CHECK(dChild1.GetWorldPosition() == glm::vec3(15.0f, 0.0f, 0.0f));
        CHECK(dChild2.GetWorldPosition() == glm::vec3(10.0f, 10.0f, 0.0f));
        CHECK(dGrandChild.GetWorldPosition() == glm::vec3(10.0f, 10.0f, 5.0f));

        REQUIRE(dParent.HasComponent<LightComponent>());
        const LightComponent& dLight = dParent.GetComponent<LightComponent>();
        CHECK(dLight.Type == LightType::Spot);
        CHECK(dLight.Color == glm::vec3(0.4f, 0.6f, 0.8f));
        CHECK_THAT(dLight.Intensity, Catch::Matchers::WithinAbs(2400.0f, 0.0001f));
        CHECK_THAT(dLight.Range, Catch::Matchers::WithinAbs(31.0f, 0.0001f));
        CHECK_THAT(dLight.SpotInnerAngle, Catch::Matchers::WithinAbs(glm::radians(18.0f), 0.0001f));
        CHECK_THAT(dLight.SpotOuterAngle, Catch::Matchers::WithinAbs(glm::radians(37.0f), 0.0001f));
        CHECK_THAT(dLight.SourceRadius, Catch::Matchers::WithinAbs(0.15f, 0.0001f));
        CHECK(dLight.UseColorTemperature);
        CHECK_THAT(dLight.Temperature, Catch::Matchers::WithinAbs(4200.0f, 0.0001f));
        CHECK(dLight.VisibilityLayers.Value == 0x00ff00ffu);
        CHECK_FALSE(dLight.AffectSpecular);
        CHECK(dLight.Volumetric);
        CHECK(dLight.Shadows.Mode == LightShadowMode::Soft);
        CHECK_THAT(dLight.Shadows.Bias, Catch::Matchers::WithinAbs(0.003f, 0.0001f));
        CHECK_THAT(dLight.Shadows.NormalBias, Catch::Matchers::WithinAbs(0.04f, 0.0001f));
        CHECK_THAT(dLight.Shadows.NearPlane, Catch::Matchers::WithinAbs(0.2f, 0.0001f));
        CHECK_THAT(dLight.Shadows.Importance, Catch::Matchers::WithinAbs(2.5f, 0.0001f));
        CHECK(dLight.Shadows.Resolution == 2048);
        CHECK_FALSE(dLight.Shadows.CacheStaticCasters);

        REQUIRE(dGrandChild.HasComponent<TextComponent>());
        const auto& dText = dGrandChild.GetComponent<TextComponent>();
        CHECK(dText.Text == "Aligned text");
        CHECK_THAT(dText.Size, Catch::Matchers::WithinAbs(42.0f, 0.0001f));
        CHECK(dText.AutoSize);
        CHECK_THAT(dText.AutoSizeMin, Catch::Matchers::WithinAbs(12.0f, 0.0001f));
        CHECK_THAT(dText.AutoSizeMax, Catch::Matchers::WithinAbs(60.0f, 0.0001f));
        CHECK(dText.LayoutSize == glm::vec2(8.0f, 3.0f));
        CHECK(dText.WrapMode == TextWrapMode::WordThenCharacter);
        CHECK(dText.Overflow == TextOverflow::Ellipsis);
        CHECK(dText.ClipToBounds);
        CHECK(dText.MaxLines == 4);
        CHECK(dText.HorizontalAlignment == TextHorizontalAlignment::Justified);
        CHECK(dText.VerticalAlignment == TextVerticalAlignment::Middle);
        CHECK(dText.FontStyle.IsSet(TextFontStyleBits::Underline));
        CHECK(dText.FontStyle.IsSet(TextFontStyleBits::Strikethrough));
        CHECK_THAT(dText.CharacterSpacing, Catch::Matchers::WithinAbs(0.05f, 0.0001f));
        CHECK_THAT(dText.WordSpacing, Catch::Matchers::WithinAbs(0.1f, 0.0001f));
        CHECK_THAT(dText.LineSpacing, Catch::Matchers::WithinAbs(0.2f, 0.0001f));
        CHECK_THAT(dText.ParagraphSpacing, Catch::Matchers::WithinAbs(0.35f, 0.0001f));
        CHECK(dText.UseCustomDecorationColor);
        CHECK(dText.DecorationColor == glm::vec4(0.2f, 0.4f, 0.6f, 0.8f));
        CHECK_THAT(dText.DecorationThickness, Catch::Matchers::WithinAbs(0.03f, 0.0001f));
        CHECK_THAT(dText.UnderlineOffset, Catch::Matchers::WithinAbs(-0.04f, 0.0001f));
        CHECK_THAT(dText.StrikethroughOffset, Catch::Matchers::WithinAbs(0.02f, 0.0001f));
        CHECK(dText.SortingLayer == -3);
        CHECK(dText.OrderInLayer == 17);

        REQUIRE(dChild1.HasComponent<SpriteRendererComponent>());
        const auto& dSprite = dChild1.GetComponent<SpriteRendererComponent>();
        CHECK(dSprite.Color == glm::vec4(0.25f, 0.5f, 0.75f, 1.0f));
        CHECK(dSprite.SortingLayer == 4);
        CHECK(dSprite.OrderInLayer == -9);

        REQUIRE(dChild2.HasComponent<Rigidbody3DComponent>());
        const auto& dRb3d = dChild2.GetComponent<Rigidbody3DComponent>();
        CHECK(dRb3d.GetBodyType() == PhysicsBodyType3D::Dynamic);
        CHECK_THAT(dRb3d.GetMass(), Catch::Matchers::WithinAbs(3.5f, 0.0001f));
        CHECK_FALSE(dRb3d.GetAutoMass());
        CHECK(dRb3d.GetContinuousCollision());
        REQUIRE(dChild2.HasComponent<BoxCollider3DComponent>());
        const auto& dBox3d = dChild2.GetComponent<BoxCollider3DComponent>();
        CHECK(dBox3d.GetSize() == glm::vec3(2.0f, 3.0f, 4.0f));
        CHECK(dBox3d.GetOffset() == glm::vec3(0.5f, 0.0f, -0.5f));
        CHECK_THAT(dBox3d.GetMaterialData().Restitution, Catch::Matchers::WithinAbs(0.6f, 0.0001f));
        CHECK(dBox3d.GetMaterialData().FrictionCombine == PhysicsCombineMode::Minimum);

        REQUIRE(dChild1.HasComponent<AudioSourceComponent>());
        auto& dAsc = dChild1.GetComponent<AudioSourceComponent>();
        CHECK(dAsc.GetVolume() == 0.5f);
        CHECK(dAsc.GetPitch() == 1.5f);
        CHECK(dAsc.GetLooping() == true);

        REQUIRE(dChild1.HasComponent<Rigidbody2DComponent>());
        const auto& dRb2d = dChild1.GetComponent<Rigidbody2DComponent>();
        CHECK(dRb2d.GetConfiguredCenterOfMass() == glm::vec2(0.25f, -0.5f));
        CHECK_THAT(dRb2d.GetConfiguredInertia(), Catch::Matchers::WithinAbs(2.75f, 0.0001f));
        CHECK(dRb2d.GetLayerMask() == Physics2DLayerCount - 1);
    }
}

TEST_CASE("Runtime physics materials flatten into collider overrides during scene round trips", "[Serialization][Physics]")
{
    SerializationTestFixture fixture;
    Ref<Scene> scene = CreateRef<Scene>(false);
    scene->SetName("PhysicsMaterials");
    Entity entity = scene->CreateEntity("Colliders");

    const AssetHandle<PhysicsMaterial2D> material2DHandle = CreateRuntimePhysicsMaterial2D(*AssetManager::TryGet());
    material2DHandle->SetFriction(0.15f);
    material2DHandle->SetFrictionCombine(PhysicsCombineMode::Maximum);
    entity.AddComponent<BoxCollider2DComponent>().SetMaterial(material2DHandle);

    const AssetHandle<PhysicsMaterial3D> material3DHandle = CreateRuntimePhysicsMaterial3D(*AssetManager::TryGet());
    material3DHandle->SetRestitution(0.85f);
    material3DHandle->SetRestitutionCombine(PhysicsCombineMode::Multiply);
    entity.AddComponent<SphereCollider3DComponent>().SetMaterial(material3DHandle);

    const auto verify = [&](const Ref<Scene>& loadedScene) {
        Entity loadedEntity = loadedScene->FindEntityByName("Colliders");
        REQUIRE(loadedEntity);
        const auto& collider2D = loadedEntity.GetComponent<BoxCollider2DComponent>();
        const auto& collider3D = loadedEntity.GetComponent<SphereCollider3DComponent>();
        CHECK(collider2D.GetMaterialData().Friction == 0.15f);
        CHECK(collider3D.GetMaterialData().Restitution == 0.85f);
        CHECK(collider2D.GetMaterialData().FrictionCombine == PhysicsCombineMode::Maximum);
        CHECK(collider3D.GetMaterialData().RestitutionCombine == PhysicsCombineMode::Multiply);
        CHECK(collider2D.GetMaterialOverride().Fields == PhysicsMaterialOverrideBits::All);
        CHECK(collider3D.GetMaterialOverride().Fields == PhysicsMaterialOverrideBits::All);
        CHECK(collider2D.GetMaterial().GetUUID() != material2DHandle.GetUUID());
        CHECK(collider3D.GetMaterial().GetUUID() != material3DHandle.GetUUID());
    };

    SECTION("YAML")
    {
        const Path path = fs::temp_directory_path() / "crowny-physics-material-scene.yaml";
        SceneSerializer(scene).Serialize(path);
        Ref<Scene> loadedScene = CreateRef<Scene>(false);
        REQUIRE(SceneSerializer(loadedScene).Deserialize(path));
        verify(loadedScene);
        fs::remove(path);
    }

    SECTION("Binary")
    {
        const Path path = fs::temp_directory_path() / "crowny-physics-material-scene.cwb";
        SceneSerializer(scene).SerializeBinary(path);
        Ref<Scene> loadedScene = CreateRef<Scene>(false);
        REQUIRE(SceneSerializer(loadedScene).DeserializeBinary(path));
        verify(loadedScene);
        fs::remove(path);
    }
}

TEST_CASE("Physics material asset references and collider overrides survive scene round trips", "[Serialization][Physics][Assets]")
{
    SerializationTestFixture fixture;
    AssetManager& manager = *AssetManager::TryGet();
    const Path material2DPath = fs::temp_directory_path() / "crowny-collider-material-override.pmat";
    const Path material3DPath = fs::temp_directory_path() / "crowny-collider-material-override.pmat3d";
    const Path yamlPath = fs::temp_directory_path() / "crowny-collider-material-override.cwscene";
    const Path binaryPath = fs::temp_directory_path() / "crowny-collider-material-override.cwb";
    fs::remove(material2DPath);
    fs::remove(material3DPath);
    fs::remove(yamlPath);
    fs::remove(binaryPath);

    Ref<PhysicsMaterial2D> material2D = CreateRef<PhysicsMaterial2D>();
    material2D->SetDensity(2.0f);
    material2D->SetFriction(0.2f);
    REQUIRE(manager.Save(material2D, material2DPath));
    Ref<PhysicsMaterial3D> material3D = CreateRef<PhysicsMaterial3D>();
    material3D->SetDensity(3.0f);
    material3D->SetRestitution(0.3f);
    REQUIRE(manager.Save(material3D, material3DPath));

    const UUID material2DId = UuidGenerator::Generate();
    const UUID material3DId = UuidGenerator::Generate();
    Ref<AssetManifest> manifest = CreateRef<AssetManifest>("PhysicsOverrideTests");
    manifest->RegisterAsset(material2DId, material2DPath);
    manifest->RegisterAsset(material3DId, material3DPath);
    ScopedAssetManifestRegistration manifestRegistration(manager, manifest);

    const AssetHandle<PhysicsMaterial2D> material2DHandle = manager.LoadFromUUID<PhysicsMaterial2D>(material2DId);
    const AssetHandle<PhysicsMaterial3D> material3DHandle = manager.LoadFromUUID<PhysicsMaterial3D>(material3DId);
    REQUIRE(material2DHandle);
    REQUIRE(material3DHandle);
    REQUIRE(manager.IsAssetRegistered(material2DId));
    REQUIRE(manager.IsAssetRegistered(material3DId));

    Ref<Scene> scene = CreateRef<Scene>(false);
    scene->SetName("ColliderMaterialOverrides");
    Entity entity = scene->CreateEntity("Colliders");
    auto& collider2D = entity.AddComponent<BoxCollider2DComponent>();
    collider2D.SetMaterial(material2DHandle);
    PhysicsMaterialOverride override2D;
    override2D.Fields = PhysicsMaterialOverrideBits::Friction;
    override2D.Values.Friction = 0.75f;
    collider2D.SetMaterialOverride(override2D);
    auto& collider3D = entity.AddComponent<SphereCollider3DComponent>();
    collider3D.SetMaterial(material3DHandle);
    PhysicsMaterialOverride override3D;
    override3D.Fields = PhysicsMaterialOverrideBits::Restitution | PhysicsMaterialOverrideBits::RestitutionCombine;
    override3D.Values.Restitution = 0.8f;
    override3D.Values.RestitutionCombine = PhysicsCombineMode::Multiply;
    collider3D.SetMaterialOverride(override3D);

    const auto verify = [&](const Ref<Scene>& loadedScene) {
        const Entity loaded = loadedScene->FindEntityByName("Colliders");
        REQUIRE(loaded);
        const auto& loaded2D = loaded.GetComponent<BoxCollider2DComponent>();
        const auto& loaded3D = loaded.GetComponent<SphereCollider3DComponent>();
        CHECK(loaded2D.GetMaterial().GetUUID() == material2DId);
        CHECK(loaded3D.GetMaterial().GetUUID() == material3DId);
        CHECK(loaded2D.GetMaterialOverride().Fields == PhysicsMaterialOverrideBits::Friction);
        CHECK(loaded3D.GetMaterialOverride().Fields ==
              (PhysicsMaterialOverrideBits::Restitution | PhysicsMaterialOverrideBits::RestitutionCombine));
        CHECK(loaded2D.GetMaterialData().Density == 2.0f);
        CHECK(loaded2D.GetMaterialData().Friction == 0.75f);
        CHECK(loaded3D.GetMaterialData().Density == 3.0f);
        CHECK(loaded3D.GetMaterialData().Restitution == 0.8f);
        CHECK(loaded3D.GetMaterialData().RestitutionCombine == PhysicsCombineMode::Multiply);
    };

    SceneSerializer(scene).Serialize(yamlPath);
    Ref<Scene> yamlScene = CreateRef<Scene>(false);
    REQUIRE(SceneSerializer(yamlScene).Deserialize(yamlPath));
    verify(yamlScene);

    SceneSerializer(scene).SerializeBinary(binaryPath);
    Ref<Scene> binaryScene = CreateRef<Scene>(false);
    REQUIRE(SceneSerializer(binaryScene).DeserializeBinary(binaryPath));
    verify(binaryScene);

    fs::remove(material2DPath);
    fs::remove(material3DPath);
    fs::remove(yamlPath);
    fs::remove(binaryPath);
}

TEST_CASE("Missing physics material assets retain their references and use defaults", "[Serialization][Physics]")
{
    SerializationTestFixture fixture;
    const UUID missing2D = UuidGenerator::Generate();
    const UUID missing3D = UuidGenerator::Generate();

    Ref<Scene> scene = CreateRef<Scene>(false);
    scene->SetName("MissingMaterials");
    Entity entity = scene->CreateEntity("MissingColliders");
    auto& collider2D = entity.AddComponent<BoxCollider2DComponent>();
    collider2D.SetMaterial(static_asset_cast<PhysicsMaterial2D>(AssetManager::TryGet()->GetAssetHandle(missing2D)));
    PhysicsMaterialOverride override2D;
    override2D.Fields = PhysicsMaterialOverrideBits::Friction;
    override2D.Values.Friction = 0.9f;
    collider2D.SetMaterialOverride(override2D);
    auto& collider3D = entity.AddComponent<BoxCollider3DComponent>();
    collider3D.SetMaterial(static_asset_cast<PhysicsMaterial3D>(AssetManager::TryGet()->GetAssetHandle(missing3D)));
    PhysicsMaterialOverride override3D;
    override3D.Fields = PhysicsMaterialOverrideBits::Restitution;
    override3D.Values.Restitution = 0.65f;
    collider3D.SetMaterialOverride(override3D);

    const Path path = fs::temp_directory_path() / "crowny-missing-physics-material-scene.yaml";
    SceneSerializer(scene).Serialize(path);
    Ref<Scene> loadedScene = CreateRef<Scene>(false);
    REQUIRE(SceneSerializer(loadedScene).Deserialize(path));
    Entity loadedEntity = loadedScene->FindEntityByName("MissingColliders");
    REQUIRE(loadedEntity);
    const auto& loaded2D = loadedEntity.GetComponent<BoxCollider2DComponent>();
    const auto& loaded3D = loadedEntity.GetComponent<BoxCollider3DComponent>();
    CHECK_FALSE(loaded2D.GetMaterial().IsLoaded());
    CHECK_FALSE(loaded3D.GetMaterial().IsLoaded());
    CHECK(loaded2D.GetMaterial().GetUUID() == missing2D);
    CHECK(loaded3D.GetMaterial().GetUUID() == missing3D);
    CHECK(loaded2D.GetMaterialData().Density == Physics2D::TryGet()->GetDefaultMaterial()->GetDensity());
    CHECK(loaded3D.GetMaterialData().Density == Physics3D::Get().GetDefaultMaterial()->GetDensity());
    CHECK(loaded2D.GetMaterialData().Friction == 0.9f);
    CHECK(loaded3D.GetMaterialData().Restitution == 0.65f);
    fs::remove(path);
}

TEST_CASE("Failed YAML scene loads discard partial entities", "[Serialization]")
{
    SerializationTestFixture fixture;
    Ref<Scene> scene = CreateRef<Scene>(false);
    scene->CreateEntity("Existing");

    YAML::Emitter emitter;
    emitter << YAML::BeginMap << YAML::Key << "Version" << YAML::Value << SceneSerializer::FORMAT_VERSION << YAML::Key << "Scene"
            << YAML::Value << "Broken" << YAML::Key << "Entities" << YAML::Value << YAML::BeginSeq << YAML::BeginMap << YAML::Key
            << "Entity" << YAML::Value << UuidGenerator::Generate() << YAML::Key << "TagComponent" << YAML::Value << YAML::BeginMap
            << YAML::Key << "Tag" << YAML::Value << "Partial" << YAML::EndMap << YAML::Key << "BoxCollider2DComponent" << YAML::Value
            << YAML::BeginMap << YAML::Key << "Offset" << YAML::Value << "invalid" << YAML::Key << "Size" << YAML::Value << glm::vec2(1.0f)
            << YAML::Key << "IsTrigger" << YAML::Value << false << YAML::EndMap << YAML::EndMap << YAML::EndSeq << YAML::EndMap;

    const Path path = fs::temp_directory_path() / "crowny-broken-scene.yaml";
    const Ref<DataStream> stream = FileSystem::CreateAndOpenFile(path);
    REQUIRE(stream != nullptr);
    stream->Write(emitter.c_str(), emitter.size());
    stream->Close();

    CHECK_FALSE(SceneSerializer(scene).Deserialize(path));
    CHECK(scene->GetRootEntity());
    CHECK_FALSE(scene->FindEntityByName("Partial"));
    CHECK_FALSE(scene->FindEntityByName("Existing"));
    fs::remove(path);
}

TEST_CASE("Light binary serialization", "[Serialization][Lights]")
{
    SerializationTestFixture fixture;
    Ref<Scene> scene = CreateRef<Scene>(false);
    scene->SetName("LightScene");
    Entity entity = scene->CreateEntity("Sun");
    auto& source = entity.AddComponent<LightComponent>();
    source.Type = LightType::Directional;
    source.Intensity = 110000.0f;
    source.Color = { 1.0f, 0.92f, 0.81f };
    source.VisibilityLayers.Value = 0x5u;
    source.Shadows.Mode = LightShadowMode::Soft;
    source.Shadows.Bias = 0.0005f;
    source.Shadows.NormalBias = 0.025f;
    source.Shadows.Resolution = 4096;

    const Path path = "light_scene.cwb";
    SceneSerializer(scene).SerializeBinary(path);

    Ref<Scene> loadedScene = CreateRef<Scene>(false);
    SceneSerializer(loadedScene).DeserializeBinary(path);
    Entity loadedEntity = loadedScene->FindEntityByName("Sun");
    REQUIRE(loadedEntity);
    REQUIRE(loadedEntity.HasComponent<LightComponent>());
    const LightComponent& loaded = loadedEntity.GetComponent<LightComponent>();
    CHECK(loaded.Type == LightType::Directional);
    CHECK_THAT(loaded.Intensity, Catch::Matchers::WithinAbs(110000.0f, 0.001f));
    CHECK(loaded.Color == source.Color);
    CHECK(loaded.VisibilityLayers.Value == 0x5u);
    CHECK(loaded.Shadows.Mode == LightShadowMode::Soft);
    CHECK_THAT(loaded.Shadows.Bias, Catch::Matchers::WithinAbs(0.0005f, 0.000001f));
    CHECK_THAT(loaded.Shadows.NormalBias, Catch::Matchers::WithinAbs(0.025f, 0.000001f));
    CHECK(loaded.Shadows.Resolution == 4096);
}

TEST_CASE("Text layout binary serialization", "[Serialization][Text]")
{
    SerializationTestFixture fixture;
    Ref<Scene> scene = CreateRef<Scene>(false);
    scene->SetName("TextLayoutScene");
    Entity entity = scene->CreateEntity("Text");
    auto& text = entity.AddComponent<TextComponent>();
    const UUID fontId = UuidGenerator::Generate();
    text.Font = static_asset_cast<Font>(AssetManager::TryGet()->CreateAssetHandle(CreateRef<Font>(), fontId));
    text.Text = "First paragraph\nSecond paragraph";
    text.AutoSize = true;
    text.AutoSizeMin = 10.0f;
    text.AutoSizeMax = 54.0f;
    text.LayoutSize = { 7.5f, 4.25f };
    text.WrapMode = TextWrapMode::Character;
    text.Overflow = TextOverflow::Truncate;
    text.ClipToBounds = true;
    text.MaxLines = 3;
    text.TabWidth = 7;
    text.ParagraphSpacing = 0.75f;
    text.ShadowColor = { 0.1f, 0.2f, 0.3f, 0.65f };
    text.ShadowOffset = { 0.3f, -0.45f };
    text.ShadowSoftness = 0.2f;
    text.UseCustomDecorationColor = true;
    text.DecorationColor = { 0.8f, 0.6f, 0.4f, 0.2f };
    text.DecorationThickness = 0.08f;
    text.UnderlineOffset = -0.1f;
    text.StrikethroughOffset = 0.15f;
    text.SortingLayer = 12;
    text.OrderInLayer = -7;

    const Path path = "text_layout_scene.cwb";
    SceneSerializer(scene).SerializeBinary(path);

    Ref<Scene> loadedScene = CreateRef<Scene>(false);
    SceneSerializer(loadedScene).DeserializeBinary(path);
    Entity loadedEntity = loadedScene->FindEntityByName("Text");
    REQUIRE(loadedEntity);
    REQUIRE(loadedEntity.HasComponent<TextComponent>());

    const auto& loaded = loadedEntity.GetComponent<TextComponent>();
    CHECK(loaded.Text == text.Text);
    CHECK(loaded.Font.GetUUID() == fontId);
    CHECK(loaded.AutoSize);
    CHECK_THAT(loaded.AutoSizeMin, Catch::Matchers::WithinAbs(10.0f, 0.0001f));
    CHECK_THAT(loaded.AutoSizeMax, Catch::Matchers::WithinAbs(54.0f, 0.0001f));
    CHECK(loaded.LayoutSize == glm::vec2(7.5f, 4.25f));
    CHECK(loaded.WrapMode == TextWrapMode::Character);
    CHECK(loaded.Overflow == TextOverflow::Truncate);
    CHECK(loaded.ClipToBounds);
    CHECK(loaded.MaxLines == 3);
    CHECK(loaded.TabWidth == 7);
    CHECK_THAT(loaded.ParagraphSpacing, Catch::Matchers::WithinAbs(0.75f, 0.0001f));
    CHECK(loaded.ShadowColor == glm::vec4(0.1f, 0.2f, 0.3f, 0.65f));
    CHECK(loaded.ShadowOffset == glm::vec2(0.3f, -0.45f));
    CHECK_THAT(loaded.ShadowSoftness, Catch::Matchers::WithinAbs(0.2f, 0.0001f));
    CHECK(loaded.UseCustomDecorationColor);
    CHECK(loaded.DecorationColor == glm::vec4(0.8f, 0.6f, 0.4f, 0.2f));
    CHECK_THAT(loaded.DecorationThickness, Catch::Matchers::WithinAbs(0.08f, 0.0001f));
    CHECK_THAT(loaded.UnderlineOffset, Catch::Matchers::WithinAbs(-0.1f, 0.0001f));
    CHECK_THAT(loaded.StrikethroughOffset, Catch::Matchers::WithinAbs(0.15f, 0.0001f));
    CHECK(loaded.SortingLayer == 12);
    CHECK(loaded.OrderInLayer == -7);
}

TEST_CASE("Scene deserialization requires the current format", "[Serialization][Scene]")
{
    SerializationTestFixture fixture;
    const uint32_t obsoleteVersion = SceneSerializer::FORMAT_VERSION - 1;

    SECTION("YAML")
    {
        const Path path = fs::temp_directory_path() / "crowny-obsolete-scene.yaml";
        YAML::Emitter out;
        out << YAML::BeginMap;
        SerializeValueYAML(out, "Version", obsoleteVersion);
        SerializeValueYAML(out, "Scene", "Obsolete");
        out << YAML::EndMap;
        Ref<DataStream> stream = FileSystem::CreateAndOpenFile(path);
        stream->Write(out.c_str(), std::strlen(out.c_str()));
        stream->Close();

        CHECK_FALSE(SceneSerializer(CreateRef<Scene>(false)).Deserialize(path));
        fs::remove(path);
    }

    SECTION("binary")
    {
        const Path path = fs::temp_directory_path() / "crowny-obsolete-scene.cwb";
        Ref<DataStream> stream = FileSystem::CreateAndOpenFile(path);
        BinaryDataStreamOutputArchive archive(stream);
        archive(obsoleteVersion);
        stream->Close();

        CHECK_FALSE(SceneSerializer(CreateRef<Scene>(false)).DeserializeBinary(path));
        fs::remove(path);
    }
}

TEST_CASE("Text layout wrapping modes handle oversized words", "[Text][Layout]")
{
    TextComponent component = MakeUnitTextComponent();
    component.LayoutSize = { 2.0f, 0.0f };
    TextLayoutScratch scratch;
    for (uint32_t i = 0; i < 4; i++)
        AddLayoutToken(scratch, U'a');

    component.WrapMode = TextWrapMode::Word;
    TextLayoutResult word = TextLayout::BuildPrepared(component, UNIT_LAYOUT_FONT, scratch);
    CHECK(word.LineCount == 1);
    CHECK(word.OverflowedHorizontally);

    component.WrapMode = TextWrapMode::Character;
    TextLayoutResult character = TextLayout::BuildPrepared(component, UNIT_LAYOUT_FONT, scratch);
    REQUIRE(character.LineCount == 2);
    CHECK_THAT(character.Lines[0].Width, Catch::Matchers::WithinAbs(2.0f, 0.0001f));
    CHECK_THAT(character.Lines[1].Width, Catch::Matchers::WithinAbs(2.0f, 0.0001f));

    component.WrapMode = TextWrapMode::WordThenCharacter;
    TextLayoutResult fallback = TextLayout::BuildPrepared(component, UNIT_LAYOUT_FONT, scratch);
    CHECK(fallback.LineCount == 2);
}

TEST_CASE("Text layout positions bounded alignment and flush spacing", "[Text][Layout]")
{
    TextComponent component = MakeUnitTextComponent();
    component.LayoutSize = { 10.0f, 10.0f };
    component.Wrapping = false;
    TextLayoutScratch scratch;
    AddLayoutToken(scratch, U'a');
    AddLayoutToken(scratch, U' ', true);
    AddLayoutToken(scratch, U'b');

    component.HorizontalAlignment = TextHorizontalAlignment::Center;
    TextLayoutResult center = TextLayout::BuildPrepared(component, UNIT_LAYOUT_FONT, scratch);
    REQUIRE(center.LineCount == 1);
    CHECK_THAT(center.Lines[0].X, Catch::Matchers::WithinAbs(3.5f, 0.0001f));

    component.HorizontalAlignment = TextHorizontalAlignment::Right;
    TextLayoutResult right = TextLayout::BuildPrepared(component, UNIT_LAYOUT_FONT, scratch);
    CHECK_THAT(right.Lines[0].X, Catch::Matchers::WithinAbs(7.0f, 0.0001f));

    component.HorizontalAlignment = TextHorizontalAlignment::Flush;
    TextLayoutResult flush = TextLayout::BuildPrepared(component, UNIT_LAYOUT_FONT, scratch);
    REQUIRE(flush.GlyphCount == 2);
    CHECK_THAT(flush.Lines[0].Width, Catch::Matchers::WithinAbs(10.0f, 0.0001f));
    CHECK_THAT(flush.Glyphs[0].PenPosition.x, Catch::Matchers::WithinAbs(0.0f, 0.0001f));
    CHECK_THAT(flush.Glyphs[1].PenPosition.x, Catch::Matchers::WithinAbs(9.0f, 0.0001f));

    component.VerticalAlignment = TextVerticalAlignment::Bottom;
    TextLayoutResult bottom = TextLayout::BuildPrepared(component, UNIT_LAYOUT_FONT, scratch);
    CHECK_THAT(bottom.Lines[0].Baseline, Catch::Matchers::WithinAbs(-9.8f, 0.0001f));
}

TEST_CASE("Text layout ellipsizes hidden lines and fits font size", "[Text][Layout]")
{
    TextComponent component = MakeUnitTextComponent();
    component.LayoutSize = { 10.0f, 1.0f };
    component.Overflow = TextOverflow::Ellipsis;
    TextLayoutScratch scratch;
    AddLayoutToken(scratch, U'a');
    AddLayoutToken(scratch, U'\n', false, true);
    AddLayoutToken(scratch, U'b');

    TextLayoutResult ellipsis = TextLayout::BuildPrepared(component, UNIT_LAYOUT_FONT, scratch);
    REQUIRE(ellipsis.LineCount == 1);
    REQUIRE(ellipsis.GlyphCount == 2);
    CHECK(ellipsis.Truncated);
    CHECK(ellipsis.Lines[0].Ellipsized);
    CHECK(ellipsis.Glyphs[1].CodePoint == 0x2026);

    TextLayoutScratch autosizeScratch;
    AddLayoutToken(autosizeScratch, U'a');
    component.LayoutSize = { 2.0f, 2.0f };
    component.Overflow = TextOverflow::Overflow;
    component.AutoSize = true;
    component.AutoSizeMin = 8.0f;
    component.AutoSizeMax = 72.0f;
    TextLayoutResult autosized = TextLayout::BuildPrepared(component, UNIT_LAYOUT_FONT, autosizeScratch);
    CHECK_THAT(autosized.FontSize, Catch::Matchers::WithinAbs(72.0f, 0.05f));
}

TEST_CASE("Scene component codecs have stable complete registrations", "[Serialization][Scene]")
{
    const auto codecs = GetSceneComponentCodecs();
    REQUIRE(codecs.size() == 21);

    for (size_t index = 0; index < codecs.size(); index++)
    {
        const SceneComponentCodec& codec = codecs[index];
        CHECK(static_cast<uint32_t>(codec.Id) == index);
        CHECK(codec.YamlName != nullptr);
        CHECK(codec.HasComponent != nullptr);
        CHECK(codec.ShouldSerialize != nullptr);
        CHECK(codec.WriteYaml != nullptr);
        CHECK(codec.ReadYaml != nullptr);
        CHECK(codec.WriteBinary != nullptr);
        CHECK(codec.ReadBinary != nullptr);
        CHECK(FindSceneComponentCodec(static_cast<uint32_t>(codec.Id)) == &codec);

        for (size_t otherIndex = index + 1; otherIndex < codecs.size(); otherIndex++)
            CHECK(StringView(codec.YamlName) != StringView(codecs[otherIndex].YamlName));
    }

    CHECK(static_cast<uint32_t>(SceneComponentId::Tag) == 0);
    CHECK(static_cast<uint32_t>(SceneComponentId::Light) == 20);
    CHECK(FindSceneComponentCodec(21) == nullptr);
    CHECK(FindSceneComponentCodec(SceneComponentId::Transform)->PrefabPath != nullptr);
    CHECK(FindSceneComponentCodec(SceneComponentId::Transform)->EditorName != nullptr);
}
