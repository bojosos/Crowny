#include <catch2/catch_test_macros.hpp>

#include "Crowny/Build/BuilderCli.h"
#include "Crowny/Common/Version.h"

#include <rapidjson/document.h>

#include <chrono>
#include <fstream>
#include <iterator>
#include <sstream>

using namespace Crowny;

namespace
{
    struct BuilderFixture
    {
        BuilderFixture()
        {
            Root = fs::temp_directory_path() /
                   ("crowny-builder-cli-tests-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
            Project = Root / "Project";
            Output = Root / "Output";
            Template = Root / "Template";
            Request = Root / "request.yaml";
            Report = Root / "report.json";
            fs::create_directories(Project);
        }

        ~BuilderFixture()
        {
            std::error_code error;
            fs::remove_all(Root, error);
        }

        void WriteText(const Path& path, StringView contents) const
        {
            if (!path.parent_path().empty())
                fs::create_directories(path.parent_path());
            std::ofstream stream(path, std::ios::binary | std::ios::trunc);
            stream.write(contents.data(), static_cast<std::streamsize>(contents.size()));
        }

        String ReadText(const Path& path) const
        {
            std::ifstream stream(path, std::ios::binary);
            return String(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
        }

        void CreateValidRequest()
        {
            GameSettings game;
            game.ProductName = "Builder Test";
            game.ArtifactName = "Builder-Test";
            game.ProductVersion = "1.0.0";
            REQUIRE(BuildProfileStore::SaveGameSettings(Project / "ProjectSettings/Game.yaml", game).empty());

            BuildProfile profile;
            profile.Id = UUID("10000000-0000-0000-0000-000000000001");
            profile.Name = "Default";
            profile.SceneOrder = { SceneId };
            profile.StartupScene = SceneId;
            profile.DefaultQuality = QualityTier::High;
            BuildTarget target;
            target.Id = TargetId;
            target.Platform = BuildPlatform::WindowsX64;
            target.Configuration = BuildConfiguration::Development;
            target.DefaultQuality = QualityTier::High;
            target.Renderers = RendererPolicy::VulkanThenOpenGL;
            target.Archive = false;
            target.IncludeSymbols = false;
            profile.Targets = { target };
            REQUIRE(BuildProfileStore::SaveProfile(Project / "ProjectSettings/BuildProfiles/default.yaml", profile).empty());

            WriteText(Project / "Internal/Assets/main.asset", "cooked-scene");
            ContentDatabase content;
            content.Assets = {
                { SceneId, "Assets/Scenes/Main.cwscene", "Internal/Assets/main.asset", {}, "Scene", "scene-source-hash" },
            };
            REQUIRE(ContentDatabaseStore::Save(Project / "Internal/Build/ContentDatabase.yaml", content).empty());

            WriteText(Template / "Crowny-Player.exe", "player-template");
            PlayerTemplateManifest templateInput;
            templateInput.EngineVersion = CROWNY_VERSION_STRING;
            templateInput.Platform = BuildPlatform::WindowsX64;
            templateInput.Configuration = BuildConfiguration::Development;
            templateInput.Renderers = { RendererBackend::Vulkan, RendererBackend::OpenGL };
            PlayerTemplateManifest templateManifest;
            REQUIRE(PlayerTemplateStore::CreateManifest(Template, templateInput, { "Crowny-Player.exe" }, templateManifest).empty());
            REQUIRE(PlayerTemplateStore::Save(Template / "template.yaml", templateManifest).empty());

            const String requestText = "Schema: 1\n"
                                       "ProjectRoot: Project\n"
                                       "OutputDirectory: ../Output\n"
                                       "GameSettings: ProjectSettings/Game.yaml\n"
                                       "BuildProfile: ProjectSettings/BuildProfiles/default.yaml\n"
                                       "BuildTarget: " +
                                       TargetId.ToString() +
                                       "\n"
                                       "ContentDatabase: Internal/Build/ContentDatabase.yaml\n"
                                       "TemplateRoot: ../Template\n"
                                       "TemplateManifest: template.yaml\n"
                                       "EngineVersion: " CROWNY_VERSION_STRING "\n"
                                       "MonoVersion: test-mono\n";
            WriteText(Request, requestText);
        }

        const UUID SceneId{ "20000000-0000-0000-0000-000000000002" };
        const UUID TargetId{ "30000000-0000-0000-0000-000000000003" };
        Path Root;
        Path Project;
        Path Output;
        Path Template;
        Path Request;
        Path Report;
    };
} // namespace

TEST_CASE("Crowny Builder describes its command without loading a project", "[Build][BuilderCli]")
{
    std::ostringstream output;
    std::ostringstream error;

    const int exitCode = RunCrownyBuilder({ "--help" }, output, error);

    CHECK(exitCode == static_cast<int>(BuilderExitCode::Success));
    CHECK(output.str().find("Crowny-Builder build --request") != String::npos);
    CHECK(error.str().empty());
}

TEST_CASE("Crowny Builder reports a missing format value without throwing", "[Build][BuilderCli]")
{
    std::ostringstream output;
    std::ostringstream error;

    const int exitCode = RunCrownyBuilder({ "build", "--format" }, output, error);

    CHECK(exitCode == static_cast<int>(BuilderExitCode::InvalidCommandLine));
    CHECK(output.str().empty());
    CHECK(error.str().find("builder.command.value_missing") != String::npos);
}

TEST_CASE("Crowny Builder rejects incomplete request files as structured input errors", "[Build][BuilderCli]")
{
    BuilderFixture fixture;
    fixture.WriteText(fixture.Request, "Schema: 1\n");
    std::ostringstream output;
    std::ostringstream error;

    const int exitCode = RunCrownyBuilder(
      { "build", "--request", fixture.Request.string(), "--report", fixture.Report.string(), "--format", "json" }, output, error);

    CHECK(exitCode == static_cast<int>(BuilderExitCode::InputError));
    CHECK(error.str().empty());
    rapidjson::Document response;
    response.Parse(output.str().c_str());
    REQUIRE_FALSE(response.HasParseError());
    CHECK(response["kind"] == "error");
    CHECK(response["exitCode"] == static_cast<int>(BuilderExitCode::InputError));
    CHECK(response["error"]["code"] == "builder.request.value_missing");
    CHECK(fixture.ReadText(fixture.Report) == output.str());
}

TEST_CASE("Crowny Builder rejects requests for a different engine binary", "[Build][BuilderCli]")
{
    BuilderFixture fixture;
    fixture.CreateValidRequest();
    String request = fixture.ReadText(fixture.Request);
    const size_t version = request.find("EngineVersion: " CROWNY_VERSION_STRING);
    REQUIRE(version != String::npos);
    request.replace(version, String("EngineVersion: " CROWNY_VERSION_STRING).size(), "EngineVersion: incompatible-version");
    fixture.WriteText(fixture.Request, request);
    std::ostringstream output;
    std::ostringstream error;

    const int exitCode = RunCrownyBuilder(
      { "build", "--request", fixture.Request.string(), "--format", "json" }, output, error);

    CHECK(exitCode == static_cast<int>(BuilderExitCode::InputError));
    CHECK(error.str().empty());
    rapidjson::Document response;
    response.Parse(output.str().c_str());
    REQUIRE_FALSE(response.HasParseError());
    CHECK(response["error"]["code"] == "builder.request.engine_version_mismatch");
}

TEST_CASE("Crowny Builder validates managed timeout using the compiler limit", "[Build][BuilderCli]")
{
    BuilderFixture fixture;
    fixture.CreateValidRequest();
    String request = fixture.ReadText(fixture.Request);
    const size_t managed = request.find("TemplateRoot:");
    REQUIRE(managed != String::npos);
    request.insert(managed, "Managed:\n  TimeoutMilliseconds: 1800001\n");
    fixture.WriteText(fixture.Request, request);
    std::ostringstream output;
    std::ostringstream error;

    const int exitCode = RunCrownyBuilder(
      { "build", "--request", fixture.Request.string(), "--format", "json" }, output, error);

    CHECK(exitCode == static_cast<int>(BuilderExitCode::InputError));
    rapidjson::Document response;
    response.Parse(output.str().c_str());
    REQUIRE_FALSE(response.HasParseError());
    CHECK(response["error"]["code"] == "builder.request.timeout_invalid");
}

TEST_CASE("Crowny Builder publishes a player build through BuildPipeline", "[Build][BuilderCli]")
{
    BuilderFixture fixture;
    fixture.CreateValidRequest();
    fixture.WriteText(fixture.Report, "last-report");
    std::ostringstream output;
    std::ostringstream error;

    const int exitCode = RunCrownyBuilder(
      { "build", "--request=" + fixture.Request.string(), "--report", fixture.Report.string(), "--format=json" }, output, error);

    CHECK(exitCode == static_cast<int>(BuilderExitCode::Success));
    CHECK(error.str().empty());
    CHECK(fs::is_regular_file(fixture.Output / "Crowny-Player.exe"));
    CHECK(fs::is_regular_file(fixture.Output / "Content/main.cwpack"));
    CHECK(fs::is_regular_file(fixture.Output / "BuildManifest.yaml"));
    rapidjson::Document response;
    response.Parse(output.str().c_str());
    REQUIRE_FALSE(response.HasParseError());
    CHECK(response["kind"] == "build");
    CHECK(response["succeeded"].GetBool());
    REQUIRE(response["stages"].IsArray());
    CHECK(response["stages"].Size() == 7);
    CHECK(fixture.ReadText(fixture.Report) == output.str());
    size_t staleReports = 0;
    for (const fs::directory_entry& entry : fs::directory_iterator(fixture.Root))
        staleReports += entry.path().filename().string().starts_with(".report.json.") ? 1 : 0;
    CHECK(staleReports == 0);
}

TEST_CASE("Crowny Builder maps cancellation to a stable exit code", "[Build][BuilderCli]")
{
    BuilderFixture fixture;
    fixture.CreateValidRequest();
    std::ostringstream output;
    std::ostringstream error;

    const int exitCode = RunCrownyBuilder({ "build", "--request", fixture.Request.string(), "--format", "json" }, output, error,
                                          [] { return true; });

    CHECK(exitCode == static_cast<int>(BuilderExitCode::Cancelled));
    CHECK(error.str().empty());
    CHECK_FALSE(fs::exists(fixture.Output));
    rapidjson::Document response;
    response.Parse(output.str().c_str());
    REQUIRE_FALSE(response.HasParseError());
    CHECK(response["cancelled"].GetBool());
    CHECK(response["exitCode"] == static_cast<int>(BuilderExitCode::Cancelled));
}
