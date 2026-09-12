#include <catch2/catch_test_macros.hpp>

#include "Build/BuildManager.h"
#include "Crowny/Common/FileSystem.h"
#include "Crowny/Common/Version.h"
#include "Crowny/Common/Yaml.h"
#include "Crowny/Scripting/Managed/Interop/ManagedJson.h"
#include "Crowny/Serialization/SceneSerializer.h"
#include "ManagedTestPaths.h"

using namespace Crowny;

// Explicit integration fixture: creates a distributable game for launch tests.
TEST_CASE("Build a playable standalone sample", "[.][PlayerSmoke]")
{
    const Path templateRoot = Test::ReadEnvironmentVariable("CROWNY_PLAYER_TEMPLATE");
    const Path destination = Test::ReadEnvironmentVariable("CROWNY_PLAYER_SMOKE_ROOT");
    REQUIRE(fs::is_regular_file(templateRoot / "Game.exe"));
    REQUIRE_FALSE(destination.empty());
    const Path project = destination / "Project";
    fs::create_directories(project / "Assets");
    const UUID sceneId("11111111-2222-3333-4444-555555555555");
    const UUID cameraId("22222222-2222-3333-4444-555555555555");
    const UUID spriteId("33333333-2222-3333-4444-555555555555");
    const UUID rootId("44444444-2222-3333-4444-555555555555");
    ScriptState state;
    state.Identity = { GAME_ASSEMBLY, "Sample", "Mover" };
    state.Root = ScriptValue::Object({}, state.Identity);
    YAML::Node scene;
    scene["Version"] = SceneSerializer::FORMAT_VERSION;
    scene["Scene"] = "Standalone sample";
    scene["Environment"] = UUID::EMPTY;
    const auto entity = [](const UUID& id, const String& name) {
        YAML::Node node;
        node["Entity"] = id;
        node["TagComponent"]["Tag"] = name;
        node["TransformComponent"]["Position"] = glm::vec3(0);
        node["TransformComponent"]["Rotation"] = glm::quat(1, 0, 0, 0);
        node["TransformComponent"]["Scale"] = glm::vec3(1);
        return node;
    };
    YAML::Node root = entity(rootId, "World");
    root["RelationshipComponent"]["Children"].push_back(cameraId);
    root["RelationshipComponent"]["Children"].push_back(spriteId);
    scene["Entities"].push_back(root);
    YAML::Node camera = entity(cameraId, "Camera");
    camera["CameraComponent"]["ProjectionType"] = 0;
    camera["CameraComponent"]["OrthographicSize"] = 8.0f;
    camera["CameraComponent"]["BackgroundColor"] = glm::vec3(0.025f, 0.04f, 0.09f);
    scene["Entities"].push_back(camera);
    YAML::Node sprite = entity(spriteId, "Move with arrow keys or WASD");
    sprite["SpriteRendererComponent"]["Color"] = glm::vec4(0.15f, 0.8f, 0.6f, 1.0f);
    sprite["SpriteRendererComponent"]["Texture"] = UUID::EMPTY;
    sprite["ManagedScriptComponent"]["Scripts"][0]["State"] = WriteManagedStateJson(state);
    scene["Entities"].push_back(sprite);
    REQUIRE(FileSystem::WriteTextFileAtomic(project / "Assets/Start.cwscene", YAML::Dump(scene)));
    const String code = R"cs(
using Crowny;
namespace Sample {
    public class Mover : EntityBehaviour {
        private int frames;
        private static bool Pressed(KeyCode key) { return Input.GetKey(key) || Input.GetKeyDown(key); }
        private void Start() { System.IO.File.WriteAllText("script-started.txt", "Standalone script started"); }
        private void Update() {
            ++frames;
            if (frames == 10) System.IO.File.WriteAllText("script-updated.txt", "Standalone Update ran");
            var position = transform.position;
            float distance = 3.0f * Time.deltaTime;
            if (Pressed(KeyCode.Right) || Pressed(KeyCode.D)) position.x += distance;
            if (Pressed(KeyCode.Left) || Pressed(KeyCode.A)) position.x -= distance;
            if (Pressed(KeyCode.Up) || Pressed(KeyCode.W)) position.y += distance;
            if (Pressed(KeyCode.Down) || Pressed(KeyCode.S)) position.y -= distance;
            transform.position = position;
        }
    }
}
)cs";
    REQUIRE(FileSystem::WriteTextFileAtomic(project / "Assets/Mover.cs", code));
    EditorBuildInputs inputs;
    inputs.ProjectRoot = project;
    inputs.Game.ProductName = "Crowny standalone sample";
    inputs.Game.ArtifactName = "StandaloneSample";
    inputs.HasGameSettings = true;
    inputs.Content.Assets = { { sceneId, "Assets/Start.cwscene", "Assets/Start.cwscene", {}, "Scene", {} } };
    inputs.HasContentDatabase = true;
    inputs.Managed.Sources = { "Assets/Mover.cs" };
    inputs.Managed.References = { templateRoot / "Managed/CrownySharp.dll" };
    inputs.Toolchain = LocateManagedToolchain({});
    inputs.TemplateRoot = templateRoot;
    inputs.EngineVersion = CROWNY_VERSION_STRING;
    inputs.MonoVersion = "6.12";
    PlayerTemplateManifest description;
    description.EngineVersion = inputs.EngineVersion;
    description.Configuration = BuildConfiguration::Shipping;
    description.Renderers = { RendererBackend::Vulkan, RendererBackend::OpenGL };
    REQUIRE(PlayerTemplateStore::CreateManifest(templateRoot, description, { "Game.exe" }, inputs.Template).empty());
    inputs.HasTemplate = true;
    BuildManager manager;
    manager.GetActivePlatformInfo()->MainScene = sceneId;
    manager.GetActivePlatformInfo()->OutputDirectory = destination / "Game";
    const auto report = manager.ExecuteActiveBuild(inputs);
    CAPTURE(report.Diagnostics.GetErrors());
    REQUIRE(report.Succeeded());
    CHECK(fs::is_regular_file(destination / "Game/Game.exe"));
    CHECK(fs::is_regular_file(destination / "Game/Managed/Game.dll"));
    CHECK(InspectManagedAssembly(destination / "Game/Managed/Game.dll").Identity.Name == GAME_ASSEMBLY);
    CHECK_FALSE(fs::exists(destination / "Game/Crowny-Editor.exe"));
}
