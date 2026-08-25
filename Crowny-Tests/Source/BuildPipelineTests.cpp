#include <catch2/catch_test_macros.hpp>

#include "Crowny/Build/BuildPipeline.h"

#include <chrono>
#include <fstream>
#include <iterator>
#include <stdexcept>

namespace Crowny
{
    class BuildPipelineTestAccess
    {
    public:
        static BuildPipeline Create(BuildPipelineOperations operations) { return BuildPipeline(std::move(operations)); }
    };

    namespace
    {
        struct TemporaryDirectory
        {
            TemporaryDirectory()
            {
                Root = fs::temp_directory_path() /
                       ("crowny-pipeline-tests-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
                fs::create_directories(Root);
            }

            ~TemporaryDirectory()
            {
                std::error_code error;
                fs::remove_all(Root, error);
            }

            Path Root;
        };

        void WriteText(const Path& path, StringView text)
        {
            fs::create_directories(path.parent_path());
            std::ofstream stream(path, std::ios::binary | std::ios::trunc);
            stream.write(text.data(), static_cast<std::streamsize>(text.size()));
        }

        String ReadText(const Path& path)
        {
            std::ifstream stream(path, std::ios::binary);
            return String(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
        }

        void WriteRecoveryJournal(const Path& output, StringView phase, const Path& backup)
        {
            WriteText(output.parent_path() / ("." + output.filename().string() + ".publish-journal"),
                      "phase=" + String(phase) + "\nbackup=" + backup.filename().generic_string() + "\n");
        }

        BuildPipelineRequest CreateRequest(const TemporaryDirectory& temporary, const Path& output)
        {
            const UUID scene("aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee");
            const UUID texture("11111111-2222-3333-4444-555555555555");
            const UUID targetId("99999999-8888-7777-6666-555555555555");
            BuildPipelineRequest request;
            request.ProjectRoot = temporary.Root / "Project";
            request.OutputDirectory = output;
            request.Game.ProductName = "Crownfall";
            request.Game.ArtifactName = "Crownfall";
            request.Game.ProductVersion = "1.0.0";
            request.Profile.Id = UUID("22222222-3333-4444-5555-666666666666");
            request.Profile.Name = "Shipping";
            request.Profile.SceneOrder = { scene };
            request.Profile.StartupScene = scene;
            request.Profile.AllowedQuality = { QualityTier::High, QualityTier::Low };
            request.Profile.DefaultQuality = QualityTier::High;
            request.Target.Id = targetId;
            request.Target.Platform = BuildPlatform::WindowsX64;
            request.Target.Configuration = BuildConfiguration::Shipping;
            request.Target.DefaultQuality = QualityTier::High;
            request.Target.Renderers = RendererPolicy::VulkanThenOpenGL;
            request.Target.Compatibility = CompatibilityPolicy::Exact;
            request.Target.Archive = false;
            request.Target.IncludeSymbols = true;
            request.Profile.Targets = { request.Target };
            request.Content.Assets = {
                { texture, "Assets/Textures/Sky.png", "Cooked/sky.asset", {}, "Texture", "texture-hash" },
                { scene, "Assets/Scenes/Main.cwscene", "Cooked/main.asset", { texture }, "Scene", "scene-hash" },
            };
            request.Managed.ProjectRoot = request.ProjectRoot;
            request.Managed.Sources = { request.ProjectRoot / "Scripts/Game.cs" };
            request.Managed.References = { request.ProjectRoot / "References/Crowny-Sharp.dll" };
            request.Toolchain.CompilerAssembly = temporary.Root / "Toolchain/csc.exe";
            request.Toolchain.ReferenceDirectory = temporary.Root / "Toolchain/Framework";
            request.Toolchain.Version = "test-compiler-1";
            request.TemplateRoot = temporary.Root / "Template";
            request.Template.EngineVersion = "1.0.0";
            request.Template.Platform = request.Target.Platform;
            request.Template.Configuration = request.Target.Configuration;
            request.Template.Renderers = { RendererBackend::Vulkan, RendererBackend::OpenGL };
            request.EngineVersion = "1.0.0";
            request.MonoVersion = "6.12";
            fs::create_directories(request.ProjectRoot);
            WriteText(request.Managed.Sources.front(), "class Game {}\n");
            WriteText(request.Managed.References.front(), "managed-reference");
            WriteText(request.Toolchain.CompilerAssembly, "compiler");
            WriteText(request.Toolchain.ReferenceDirectory / "mscorlib.dll", "framework-mscorlib");
            WriteText(request.Toolchain.ReferenceDirectory / "System.DLL", "framework-system");
            WriteText(request.ProjectRoot / "Cooked/sky.asset", "cooked-sky");
            WriteText(request.ProjectRoot / "Cooked/main.asset", "cooked-main");
            return request;
        }

        struct FakeBuildTools
        {
            Vector<String> Calls;
            Vector<ContentPackInput> PackedInputs;
            BuildManifest Manifest;
            bool CancelAfterResolve = false;
            bool CancelDuringCompile = false;
            bool Cancelled = false;
            bool FailPack = false;
            bool ThrowPack = false;
            bool FailPublishMove = false;
            bool ThrowPublishMove = false;
            bool ThrowAfterPublishMove = false;
            bool BackupReportsFailureAfterMove = false;
            bool BackupReportsSuccessWithoutMove = false;
            bool FailQuarantineMove = false;
            size_t MoveCount = 0;

            BuildPipelineOperations Operations()
            {
                BuildPipelineOperations operations;
                operations.Validate = [&](const BuildPipelineRequest&) {
                    Calls.push_back("Validate");
                    return BuildValidation();
                };
                operations.ResolveContent = [&](const ContentDatabase& database, const ContentResolveRequest&) {
                    Calls.push_back("Resolve Content");
                    ContentResolveResult result;
                    for (const ContentAssetRecord& asset : database.Assets)
                        result.Assets.push_back({ asset, { asset.Id }, "Test" });
                    Cancelled = CancelAfterResolve;
                    return result;
                };
                operations.CompileManaged = [&](const ManagedBuildRequest& request, const ManagedToolchain&) {
                    Calls.push_back("Compile Managed");
                    ManagedCompileResult result;
                    result.ProcessStarted = true;
                    if (CancelDuringCompile)
                    {
                        Cancelled = true;
                        result.Cancelled = request.Cancellation && request.Cancellation();
                        return result;
                    }
                    WriteText(request.OutputAssembly, "managed");
                    result.ExitCode = 0;
                    return result;
                };
                operations.PackContent = [&](const Path& path, const ContentPackDescriptor&, const Vector<ContentPackInput>& inputs) {
                    Calls.push_back("Pack");
                    PackedInputs = inputs;
                    if (ThrowPack)
                        throw std::runtime_error("pack callback threw");
                    if (FailPack)
                        return String("pack exploded");
                    WriteText(path, "pack");
                    return String();
                };
                operations.StageTemplate = [&](const BuildTemplateStageRequest& request) {
                    Calls.push_back("Stage Template");
                    BuildValidation validation;
                    fs::create_directories(request.StageDirectory);
                    for (const BuildPipelineArtifact& artifact : request.Artifacts)
                    {
                        const Path destination = request.StageDirectory / artifact.RelativeDestination;
                        fs::create_directories(destination.parent_path());
                        std::error_code error;
                        fs::copy_file(artifact.Source, destination, fs::copy_options::none, error);
                        if (error)
                            validation.Error("fake.copy", error.message(), destination.string());
                    }
                    WriteText(request.StageDirectory / "Player.bin", "player");
                    return validation;
                };
                operations.WriteManifest = [&](const Path& path, const BuildManifest& manifest) {
                    Calls.push_back("Write Manifest");
                    Manifest = manifest;
                    WriteText(path, "manifest");
                    return String();
                };
                operations.MoveDirectory = [&](const Path& source, const Path& destination) {
                    const size_t move = ++MoveCount;
                    if (move == 1)
                        Calls.push_back("Publish");
                    if (BackupReportsSuccessWithoutMove && move == 1)
                        return String();
                    if (FailPublishMove && move == 2)
                        return String("injected publish failure");
                    if (ThrowPublishMove && move == 2)
                        throw std::runtime_error("publish callback threw");
                    if (FailQuarantineMove && move == 3)
                        return String("injected quarantine failure");
                    std::error_code error;
                    fs::rename(source, destination, error);
                    if (!error && BackupReportsFailureAfterMove && move == 1)
                        return String("backup callback reported failure after moving");
                    if (!error && ThrowAfterPublishMove && move == 2)
                        throw std::runtime_error("publish callback threw after moving");
                    return error ? error.message() : String();
                };
                return operations;
            }
        };

        size_t CountTemporaryBuildDirectories(const Path& parent, const Path& output)
        {
            size_t count = 0;
            if (!fs::is_directory(parent))
                return count;
            const String prefix = "." + output.filename().string() + ".build-";
            for (const fs::directory_entry& entry : fs::directory_iterator(parent))
                if (entry.path().filename().string().starts_with(prefix))
                    ++count;
            return count;
        }

        size_t CountSiblingDirectories(const Path& output, StringView label)
        {
            size_t count = 0;
            std::error_code error;
            const String prefix = "." + output.filename().string() + "." + String(label) + "-";
            for (fs::directory_iterator iterator(output.parent_path(), error), end; !error && iterator != end; iterator.increment(error))
                if (iterator->path().filename().string().starts_with(prefix))
                    ++count;
            return count;
        }
    } // namespace

    TEST_CASE("Build pipeline executes deterministic stages and decisions", "[Build][Pipeline]")
    {
        TemporaryDirectory temporary;
        BuildPipelineRequest request = CreateRequest(temporary, temporary.Root / "Builds/Crownfall");
        FakeBuildTools tools;
        BuildPipeline pipeline = BuildPipelineTestAccess::Create(tools.Operations());

        const BuildPipelineReport first = pipeline.Run(request);
        for (const BuildPipelineStageReport& stage : first.Stages)
            for (const BuildIssue& issue : stage.Diagnostics.Issues)
                UNSCOPED_INFO(ToString(stage.Stage) << ": " << issue.Code << ": " << issue.Message << " [" << issue.Subject << "]");
        REQUIRE(first.Succeeded());
        CHECK(tools.Calls ==
              Vector<String>{ "Validate", "Resolve Content", "Compile Managed", "Pack", "Stage Template", "Write Manifest", "Publish" });
        REQUIRE(tools.PackedInputs.size() == 2);
        CHECK(tools.PackedInputs[0].LogicalPath == Path("Assets/Scenes/Main.cwscene"));
        CHECK(tools.PackedInputs[1].LogicalPath == Path("Assets/Textures/Sky.png"));
        REQUIRE(tools.Manifest.Scenes.size() == 1);
        CHECK(tools.Manifest.Scenes.front().Id == request.Profile.StartupScene);
        CHECK(tools.Manifest.Paths.ContentPack == Path("Content/main.cwpack"));
        CHECK(tools.Manifest.Paths.ManagedAssembly == Path("Managed/Game.dll"));
        CHECK(fs::is_regular_file(request.OutputDirectory / "BuildManifest.yaml"));

        std::reverse(request.Content.Assets.begin(), request.Content.Assets.end());
        request.OutputDirectory = temporary.Root / "Builds/Crownfall-copy";
        FakeBuildTools secondTools;
        const BuildPipelineReport second = BuildPipelineTestAccess::Create(secondTools.Operations()).Run(request);
        REQUIRE(second.Succeeded());
        CHECK(second.Fingerprint == first.Fingerprint);
        CHECK(secondTools.PackedInputs[0].LogicalPath == tools.PackedInputs[0].LogicalPath);
        CHECK(secondTools.Manifest.AllowedQuality == Vector<QualityTier>{ QualityTier::Low, QualityTier::High });
    }

    TEST_CASE("Build fingerprint changes when any consumed payload changes", "[Build][Pipeline]")
    {
        TemporaryDirectory temporary;
        BuildPipelineRequest request = CreateRequest(temporary, temporary.Root / "Builds/Baseline");
        size_t run = 0;
        const auto buildFingerprint = [&]() {
            request.OutputDirectory = temporary.Root / ("Builds/Mutation-" + std::to_string(run++));
            FakeBuildTools tools;
            const BuildPipelineReport report = BuildPipelineTestAccess::Create(tools.Operations()).Run(request);
            REQUIRE(report.Succeeded());
            return report.Fingerprint;
        };

        const String baseline = buildFingerprint();
        const auto changesFingerprint = [&](const Path& path, StringView original, StringView changed) {
            WriteText(path, changed);
            const String mutated = buildFingerprint();
            WriteText(path, original);
            CHECK(mutated != baseline);
        };
        changesFingerprint(request.Managed.Sources.front(), "class Game {}\n", "class Game { int Value; }\n");
        changesFingerprint(request.Managed.References.front(), "managed-reference", "managed-reference-v2");
        changesFingerprint(request.Toolchain.CompilerAssembly, "compiler", "compiler-v2");
        changesFingerprint(request.Toolchain.ReferenceDirectory / "System.DLL", "framework-system", "framework-system-v2");
        changesFingerprint(request.ProjectRoot / "Cooked/main.asset", "cooked-main", "cooked-main-v2");
    }

    TEST_CASE("Build fingerprint frames managed source and reference collections", "[Build][Pipeline]")
    {
        TemporaryDirectory temporary;
        BuildPipelineRequest request = CreateRequest(temporary, temporary.Root / "Builds/Framing-A");
        const Path third = request.ProjectRoot / "References/Third.dll";
        WriteText(third, "third-reference");
        request.Managed.References.push_back(third);
        FakeBuildTools firstTools;
        const BuildPipelineReport first = BuildPipelineTestAccess::Create(firstTools.Operations()).Run(request);
        REQUIRE(first.Succeeded());

        request.OutputDirectory = temporary.Root / "Builds/Framing-B";
        request.Managed.Sources.push_back(request.Managed.References.front());
        request.Managed.References.erase(request.Managed.References.begin());
        FakeBuildTools secondTools;
        const BuildPipelineReport second = BuildPipelineTestAccess::Create(secondTools.Operations()).Run(request);
        REQUIRE(second.Succeeded());
        CHECK(second.Fingerprint != first.Fingerprint);
    }

    TEST_CASE("Build fingerprint includes schemas and managed execution settings", "[Build][Pipeline]")
    {
        TemporaryDirectory temporary;
        BuildPipelineRequest request = CreateRequest(temporary, temporary.Root / "Builds/Fingerprint-Baseline");
        size_t run = 0;
        const auto fingerprint = [&](BuildPipelineRequest value) {
            value.OutputDirectory = temporary.Root / ("Builds/Fingerprint-" + std::to_string(run++));
            FakeBuildTools tools;
            return BuildPipelineTestAccess::Create(tools.Operations()).Run(std::move(value)).Fingerprint;
        };
        const String baseline = fingerprint(request);

        BuildPipelineRequest mutated = request;
        mutated.Game.Schema++;
        CHECK(fingerprint(mutated) != baseline);
        mutated = request;
        mutated.Profile.Schema++;
        CHECK(fingerprint(mutated) != baseline);
        mutated = request;
        mutated.Content.Schema++;
        CHECK(fingerprint(mutated) != baseline);
        mutated = request;
        mutated.Template.Schema++;
        CHECK(fingerprint(mutated) != baseline);
        mutated = request;
        mutated.Managed.Configuration = BuildConfiguration::Shipping;
        CHECK(fingerprint(mutated) != baseline);
        mutated = request;
        mutated.Managed.Timeout += std::chrono::milliseconds(1);
        CHECK(fingerprint(mutated) != baseline);
        mutated = request;
        mutated.Managed.MaxCapturedOutputBytes++;
        CHECK(fingerprint(mutated) != baseline);
        request.Toolchain.Diagnostics.push_back({ "TC001", "toolchain warning", request.Toolchain.CompilerAssembly });
        const String diagnosticBaseline = fingerprint(request);
        mutated = request;
        mutated.Toolchain.Diagnostics.front().Code = "TC002";
        CHECK(fingerprint(mutated) != diagnosticBaseline);
        mutated = request;
        mutated.Toolchain.Diagnostics.front().Message = "different warning";
        CHECK(fingerprint(mutated) != diagnosticBaseline);
        mutated = request;
        mutated.Toolchain.Diagnostics.front().Subject = request.Toolchain.ReferenceDirectory;
        CHECK(fingerprint(mutated) != diagnosticBaseline);
    }

    TEST_CASE("Default build pipeline writes and reads back real Crowny artifacts", "[Build][Pipeline][Integration]")
    {
        TemporaryDirectory temporary;
        BuildPipelineRequest request = CreateRequest(temporary, temporary.Root / "Builds/Crownfall");
        request.Managed.Sources.clear();
        request.Managed.References.clear();
        WriteText(request.TemplateRoot / "Player.bin", "player-template");
        PlayerTemplateManifest manifest;
        REQUIRE(PlayerTemplateStore::CreateManifest(request.TemplateRoot, request.Template, {}, manifest).empty());
        request.Template = manifest;

        const BuildPipelineReport report = BuildPipeline().Run(request);

        REQUIRE(report.Succeeded());
        ContentPackReader pack;
        CHECK(pack.Open(request.OutputDirectory / "Content/main.cwpack").empty());
        BuildManifest loaded;
        CHECK(BuildManifestStore::Load(request.OutputDirectory / "BuildManifest.yaml", loaded).empty());
        CHECK(loaded.ProductName == request.Game.ProductName);
        CHECK(fs::is_regular_file(request.OutputDirectory / "Player.bin"));
    }

    TEST_CASE("Default artifact validation rejects content path identity changes", "[Build][Pipeline]")
    {
        TemporaryDirectory temporary;
        BuildPipelineRequest request = CreateRequest(temporary, temporary.Root / "Builds/Crownfall");
        request.Managed.Sources.clear();
        request.Managed.References.clear();
        BuildPipelineOperations operations = CreateDefaultBuildPipelineOperations();
        operations.PackContent = [](const Path& path, const ContentPackDescriptor& descriptor, const Vector<ContentPackInput>& inputs) {
            Vector<ContentPackInput> changed = inputs;
            changed.front().LogicalPath = "Assets/Tampered.asset";
            return ContentPackWriter::Write(path, descriptor, changed);
        };

        const BuildPipelineReport report = BuildPipelineTestAccess::Create(std::move(operations)).Run(request);

        for (const BuildPipelineStageReport& stage : report.Stages)
            for (const BuildIssue& issue : stage.Diagnostics.Issues)
                UNSCOPED_INFO(ToString(stage.Stage) << ": " << issue.Code << ": " << issue.Message << " [" << issue.Subject << "]");
        CHECK_FALSE(report.Succeeded());
        const BuildPipelineStageReport* pack = report.Find(BuildPipelineStage::PackContent);
        REQUIRE(pack != nullptr);
        CHECK(pack->Diagnostics.ContainsCode("pipeline.pack.entry_path_mismatch"));
        CHECK(pack->Diagnostics.ContainsCode("pipeline.pack.path_identity_mismatch"));
    }

    TEST_CASE("Default artifact validation compares the complete manifest", "[Build][Pipeline]")
    {
        TemporaryDirectory temporary;
        BuildPipelineRequest request = CreateRequest(temporary, temporary.Root / "Builds/Crownfall");
        request.Managed.Sources.clear();
        request.Managed.References.clear();
        WriteText(request.TemplateRoot / "Player.bin", "player-template");
        PlayerTemplateManifest templateManifest;
        REQUIRE(PlayerTemplateStore::CreateManifest(request.TemplateRoot, request.Template, {}, templateManifest).empty());
        request.Template = templateManifest;
        BuildPipelineOperations operations = CreateDefaultBuildPipelineOperations();
        operations.WriteManifest = [](const Path& path, const BuildManifest& manifest) {
            BuildManifest changed = manifest;
            changed.Company += "-tampered";
            changed.Paths.MonoRoot = "TamperedMono";
            if (!changed.Scenes.empty())
                changed.Scenes.front().LogicalPath = "Assets/Scenes/Tampered.cwscene";
            return BuildManifestStore::Save(path, changed);
        };

        const BuildPipelineReport report = BuildPipelineTestAccess::Create(std::move(operations)).Run(request);

        CHECK_FALSE(report.Succeeded());
        const BuildPipelineStageReport* manifest = report.Find(BuildPipelineStage::WriteManifest);
        REQUIRE(manifest != nullptr);
        for (const BuildPipelineStageReport& stage : report.Stages)
            for (const BuildIssue& issue : stage.Diagnostics.Issues)
                UNSCOPED_INFO(ToString(stage.Stage) << ": " << issue.Code << ": " << issue.Message << " [" << issue.Subject << "]");
        CHECK(manifest->Diagnostics.ContainsCode("pipeline.manifest.readback_mismatch"));
    }

    TEST_CASE("Build pipeline rejects an output that contains protected inputs", "[Build][Pipeline]")
    {
        TemporaryDirectory temporary;
        BuildPipelineRequest request = CreateRequest(temporary, temporary.Root);
        FakeBuildTools tools;
        const BuildPipelineReport report = BuildPipelineTestAccess::Create(tools.Operations()).Run(request);

        CHECK_FALSE(report.Succeeded());
        REQUIRE(report.Find(BuildPipelineStage::Validate) != nullptr);
        CHECK(report.Find(BuildPipelineStage::Validate)->Diagnostics.ContainsCode("pipeline.output.overlap"));
        CHECK(tools.Calls == Vector<String>{ "Validate" });
    }

    TEST_CASE("Build pipeline rejects cooked inputs that traverse a symbolic link", "[Build][Pipeline]")
    {
        TemporaryDirectory temporary;
        BuildPipelineRequest request = CreateRequest(temporary, temporary.Root / "Builds/Crownfall");
        const Path cooked = request.ProjectRoot / "Cooked/main.asset";
        const Path external = temporary.Root / "External/main.asset";
        WriteText(external, "external-cooked-main");
        std::error_code error;
        fs::remove(cooked, error);
        error.clear();
        fs::create_symlink(external, cooked, error);
        if (error)
        {
            SUCCEED("Symbolic links are unavailable in this test environment.");
            return;
        }
        FakeBuildTools tools;
        const BuildPipelineReport report = BuildPipelineTestAccess::Create(tools.Operations()).Run(request);

        CHECK_FALSE(report.Succeeded());
        CHECK(report.Find(BuildPipelineStage::Validate)->Diagnostics.ContainsCode("pipeline.content.path_escape"));
    }

    TEST_CASE("Build pipeline rejects an output symlink alias of protected roots", "[Build][Pipeline]")
    {
        TemporaryDirectory temporary;
        const Path alias = temporary.Root.parent_path() / (temporary.Root.filename().string() + "-alias");
        std::error_code error;
        fs::create_directory_symlink(temporary.Root, alias, error);
        if (error)
        {
            SUCCEED("Directory symbolic links are unavailable in this test environment.");
            return;
        }
        struct AliasCleanup
        {
            ~AliasCleanup()
            {
                std::error_code cleanupError;
                fs::remove(Alias, cleanupError);
            }
            Path Alias;
        } cleanup{ alias };
        BuildPipelineRequest request = CreateRequest(temporary, alias);
        FakeBuildTools tools;
        const BuildPipelineReport report = BuildPipelineTestAccess::Create(tools.Operations()).Run(request);

        CHECK_FALSE(report.Succeeded());
        CHECK(report.Find(BuildPipelineStage::Validate)->Diagnostics.ContainsCode("pipeline.output.overlap"));
        CHECK(report.Find(BuildPipelineStage::Validate)->Diagnostics.ContainsCode("pipeline.output.link"));
    }

    TEST_CASE("Build pipeline cancellation stops at the next checkpoint and cleans staging", "[Build][Pipeline]")
    {
        TemporaryDirectory temporary;
        BuildPipelineRequest request = CreateRequest(temporary, temporary.Root / "Builds/Crownfall");
        FakeBuildTools tools;
        tools.CancelAfterResolve = true;
        const BuildPipelineReport report = BuildPipelineTestAccess::Create(tools.Operations()).Run(request, [&]() { return tools.Cancelled; });

        CHECK(report.Cancelled);
        CHECK_FALSE(report.Succeeded());
        REQUIRE(report.Find(BuildPipelineStage::CompileManaged) != nullptr);
        CHECK(report.Find(BuildPipelineStage::CompileManaged)->Status == BuildPipelineStageStatus::Cancelled);
        CHECK(report.Find(BuildPipelineStage::PackContent)->Status == BuildPipelineStageStatus::Skipped);
        CHECK(tools.Calls == Vector<String>{ "Validate", "Resolve Content" });
        CHECK(CountTemporaryBuildDirectories(request.OutputDirectory.parent_path(), request.OutputDirectory) == 0);
    }

    TEST_CASE("Build pipeline forwards cancellation into managed compilation", "[Build][Pipeline]")
    {
        TemporaryDirectory temporary;
        BuildPipelineRequest request = CreateRequest(temporary, temporary.Root / "Builds/Crownfall");
        FakeBuildTools tools;
        tools.CancelDuringCompile = true;

        const BuildPipelineReport report = BuildPipelineTestAccess::Create(tools.Operations()).Run(request, [&]() { return tools.Cancelled; });

        CHECK(report.Cancelled);
        CHECK_FALSE(report.Succeeded());
        CHECK(report.Find(BuildPipelineStage::CompileManaged)->Status == BuildPipelineStageStatus::Cancelled);
        CHECK(report.Find(BuildPipelineStage::PackContent)->Status == BuildPipelineStageStatus::Skipped);
        CHECK(tools.Calls == Vector<String>{ "Validate", "Resolve Content", "Compile Managed" });
        CHECK(CountTemporaryBuildDirectories(request.OutputDirectory.parent_path(), request.OutputDirectory) == 0);
    }

    TEST_CASE("Build pipeline attributes stage failure and preserves the last good build", "[Build][Pipeline]")
    {
        TemporaryDirectory temporary;
        BuildPipelineRequest request = CreateRequest(temporary, temporary.Root / "Builds/Crownfall");
        WriteText(request.OutputDirectory / "marker.txt", "last-good");
        FakeBuildTools tools;
        tools.FailPack = true;
        const BuildPipelineReport report = BuildPipelineTestAccess::Create(tools.Operations()).Run(request);

        CHECK_FALSE(report.Succeeded());
        const BuildPipelineStageReport* pack = report.Find(BuildPipelineStage::PackContent);
        REQUIRE(pack != nullptr);
        CHECK(pack->Status == BuildPipelineStageStatus::Failed);
        CHECK(pack->Diagnostics.ContainsCode("pipeline.pack.failed"));
        CHECK(report.Find(BuildPipelineStage::StageTemplate)->Status == BuildPipelineStageStatus::Skipped);
        CHECK(ReadText(request.OutputDirectory / "marker.txt") == "last-good");
        CHECK(CountTemporaryBuildDirectories(request.OutputDirectory.parent_path(), request.OutputDirectory) == 0);
    }

    TEST_CASE("Build pipeline never adopts or removes stale staging directories", "[Build][Pipeline]")
    {
        TemporaryDirectory temporary;
        BuildPipelineRequest request = CreateRequest(temporary, temporary.Root / "Builds/Crownfall");
        const Path stale = request.OutputDirectory.parent_path() / ".Crownfall.build-stale";
        WriteText(stale / "owner.txt", "foreign");
        FakeBuildTools tools;
        tools.FailPack = true;
        const BuildPipelineReport report = BuildPipelineTestAccess::Create(tools.Operations()).Run(request);

        CHECK_FALSE(report.Succeeded());
        CHECK(ReadText(stale / "owner.txt") == "foreign");
        CHECK(CountTemporaryBuildDirectories(request.OutputDirectory.parent_path(), request.OutputDirectory) == 1);
    }

    TEST_CASE("Build pipeline rolls back when final directory publication fails", "[Build][Pipeline]")
    {
        TemporaryDirectory temporary;
        BuildPipelineRequest request = CreateRequest(temporary, temporary.Root / "Builds/Crownfall");
        WriteText(request.OutputDirectory / "marker.txt", "last-good");
        FakeBuildTools tools;
        tools.FailPublishMove = true;
        const BuildPipelineReport report = BuildPipelineTestAccess::Create(tools.Operations()).Run(request);

        CHECK_FALSE(report.Succeeded());
        const BuildPipelineStageReport* publish = report.Find(BuildPipelineStage::Publish);
        REQUIRE(publish != nullptr);
        CHECK(publish->Status == BuildPipelineStageStatus::Failed);
        CHECK(publish->Diagnostics.ContainsCode("pipeline.publish.failed"));
        CHECK_FALSE(publish->Diagnostics.ContainsCode("pipeline.publish.rollback_failed"));
        CHECK(ReadText(request.OutputDirectory / "marker.txt") == "last-good");
        CHECK(CountTemporaryBuildDirectories(request.OutputDirectory.parent_path(), request.OutputDirectory) == 0);
        CHECK(CountSiblingDirectories(request.OutputDirectory, "previous") == 0);
    }

    TEST_CASE("Build pipeline contains throwing pack and cancellation callbacks", "[Build][Pipeline]")
    {
        TemporaryDirectory temporary;
        BuildPipelineRequest request = CreateRequest(temporary, temporary.Root / "Builds/Crownfall");
        WriteText(request.OutputDirectory / "marker.txt", "last-good");
        FakeBuildTools tools;
        tools.ThrowPack = true;
        const BuildPipelineReport packReport = BuildPipelineTestAccess::Create(tools.Operations()).Run(request);

        CHECK_FALSE(packReport.Succeeded());
        CHECK(packReport.Find(BuildPipelineStage::PackContent)->Diagnostics.ContainsCode("pipeline.stage.exception"));
        CHECK(ReadText(request.OutputDirectory / "marker.txt") == "last-good");
        CHECK(CountTemporaryBuildDirectories(request.OutputDirectory.parent_path(), request.OutputDirectory) == 0);

        request.OutputDirectory = temporary.Root / "Builds/Cancelled";
        FakeBuildTools cancellationTools;
        const BuildPipelineReport cancellationReport = BuildPipelineTestAccess::Create(cancellationTools.Operations()).Run(request, []() -> bool {
            throw std::runtime_error("cancel callback threw");
        });
        CHECK_FALSE(cancellationReport.Succeeded());
        CHECK(cancellationReport.Find(BuildPipelineStage::Validate)->Diagnostics.ContainsCode("pipeline.cancellation.exception"));
        CHECK(CountTemporaryBuildDirectories(request.OutputDirectory.parent_path(), request.OutputDirectory) == 0);
    }

    TEST_CASE("Build pipeline restores the last good build when publication throws", "[Build][Pipeline]")
    {
        TemporaryDirectory temporary;
        BuildPipelineRequest request = CreateRequest(temporary, temporary.Root / "Builds/Crownfall");
        WriteText(request.OutputDirectory / "marker.txt", "last-good");
        FakeBuildTools tools;
        tools.ThrowAfterPublishMove = true;
        const BuildPipelineReport report = BuildPipelineTestAccess::Create(tools.Operations()).Run(request);

        CHECK_FALSE(report.Succeeded());
        CHECK(report.Find(BuildPipelineStage::Publish)->Diagnostics.ContainsCode("pipeline.publish.failed"));
        CHECK_FALSE(report.Find(BuildPipelineStage::Publish)->Diagnostics.ContainsCode("pipeline.publish.rollback_failed"));
        CHECK(ReadText(request.OutputDirectory / "marker.txt") == "last-good");
        CHECK(CountSiblingDirectories(request.OutputDirectory, "previous") == 0);
        CHECK(CountSiblingDirectories(request.OutputDirectory, "failed") == 0);
    }

    TEST_CASE("Build pipeline verifies backup move postconditions", "[Build][Pipeline]")
    {
        TemporaryDirectory temporary;
        BuildPipelineRequest request = CreateRequest(temporary, temporary.Root / "Builds/Crownfall");
        WriteText(request.OutputDirectory / "marker.txt", "last-good");
        FakeBuildTools tools;
        tools.BackupReportsSuccessWithoutMove = true;
        const BuildPipelineReport report = BuildPipelineTestAccess::Create(tools.Operations()).Run(request);

        CHECK_FALSE(report.Succeeded());
        CHECK(report.Find(BuildPipelineStage::Publish)->Diagnostics.ContainsCode("pipeline.publish.backup_failed"));
        CHECK(ReadText(request.OutputDirectory / "marker.txt") == "last-good");
        CHECK(CountSiblingDirectories(request.OutputDirectory, "previous") == 0);
    }

    TEST_CASE("Build pipeline restores a backup even when its move reports failure after moving", "[Build][Pipeline]")
    {
        TemporaryDirectory temporary;
        BuildPipelineRequest request = CreateRequest(temporary, temporary.Root / "Builds/Crownfall");
        WriteText(request.OutputDirectory / "marker.txt", "last-good");
        FakeBuildTools tools;
        tools.BackupReportsFailureAfterMove = true;
        const BuildPipelineReport report = BuildPipelineTestAccess::Create(tools.Operations()).Run(request);

        CHECK_FALSE(report.Succeeded());
        CHECK(report.Find(BuildPipelineStage::Publish)->Diagnostics.ContainsCode("pipeline.publish.backup_failed"));
        CHECK_FALSE(report.Find(BuildPipelineStage::Publish)->Diagnostics.ContainsCode("pipeline.publish.rollback_failed"));
        CHECK(ReadText(request.OutputDirectory / "marker.txt") == "last-good");
        CHECK(CountSiblingDirectories(request.OutputDirectory, "previous") == 0);
    }

    TEST_CASE("Build pipeline preserves the backup path when a failed output blocks rollback", "[Build][Pipeline]")
    {
        TemporaryDirectory temporary;
        BuildPipelineRequest request = CreateRequest(temporary, temporary.Root / "Builds/Crownfall");
        WriteText(request.OutputDirectory / "marker.txt", "last-good");
        FakeBuildTools tools;
        tools.ThrowAfterPublishMove = true;
        tools.FailQuarantineMove = true;
        const BuildPipelineReport report = BuildPipelineTestAccess::Create(tools.Operations()).Run(request);

        const BuildPipelineStageReport* publish = report.Find(BuildPipelineStage::Publish);
        REQUIRE(publish != nullptr);
        REQUIRE(publish->Diagnostics.ContainsCode("pipeline.publish.rollback_failed"));
        const auto issue = std::find_if(publish->Diagnostics.Issues.begin(), publish->Diagnostics.Issues.end(),
                                        [](const BuildIssue& entry) { return entry.Code == "pipeline.publish.rollback_failed"; });
        REQUIRE(issue != publish->Diagnostics.Issues.end());
        const Path backup = issue->Subject;
        CHECK(fs::is_regular_file(backup / "marker.txt"));
        CHECK(ReadText(backup / "marker.txt") == "last-good");
        CHECK(fs::is_directory(request.OutputDirectory));
        CHECK(CountSiblingDirectories(request.OutputDirectory, "previous") == 1);
        CHECK(fs::is_regular_file(request.OutputDirectory.parent_path() / ".Crownfall.publish-journal"));
    }

    TEST_CASE("Build pipeline reconciles an interrupted persistent backup before publishing", "[Build][Pipeline]")
    {
        TemporaryDirectory temporary;
        BuildPipelineRequest request = CreateRequest(temporary, temporary.Root / "Builds/Crownfall");
        const Path recovery = request.OutputDirectory.parent_path() / ".Crownfall.previous-recovery";
        WriteText(recovery / "marker.txt", "interrupted-last-good");
        WriteRecoveryJournal(request.OutputDirectory, "backed_up", recovery);
        FakeBuildTools tools;
        tools.FailPublishMove = true;
        const BuildPipelineReport report = BuildPipelineTestAccess::Create(tools.Operations()).Run(request);

        CHECK_FALSE(report.Succeeded());
        CHECK(report.Find(BuildPipelineStage::Publish)->Diagnostics.ContainsCode("pipeline.publish.recovered"));
        CHECK(ReadText(request.OutputDirectory / "marker.txt") == "interrupted-last-good");
        CHECK_FALSE(fs::exists(recovery));
        CHECK(CountSiblingDirectories(request.OutputDirectory, "previous") == 0);
    }

    TEST_CASE("Build pipeline reconciles every valid publication journal phase", "[Build][Pipeline]")
    {
        SECTION("prepared restores the exact backup when the output is absent")
        {
            TemporaryDirectory temporary;
            BuildPipelineRequest request = CreateRequest(temporary, temporary.Root / "Builds/Crownfall");
            const Path backup = request.OutputDirectory.parent_path() / ".Crownfall.previous-prepared";
            WriteText(backup / "marker.txt", "prepared-last-good");
            WriteRecoveryJournal(request.OutputDirectory, "prepared", backup);
            FakeBuildTools tools;
            tools.FailPublishMove = true;

            const BuildPipelineReport report = BuildPipelineTestAccess::Create(tools.Operations()).Run(request);

            CHECK_FALSE(report.Succeeded());
            CHECK(report.Find(BuildPipelineStage::Publish)->Diagnostics.ContainsCode("pipeline.publish.recovered"));
            CHECK(ReadText(request.OutputDirectory / "marker.txt") == "prepared-last-good");
        }

        SECTION("backed_up quarantines an unvalidated candidate and restores the exact backup")
        {
            TemporaryDirectory temporary;
            BuildPipelineRequest request = CreateRequest(temporary, temporary.Root / "Builds/Crownfall");
            const Path backup = request.OutputDirectory.parent_path() / ".Crownfall.previous-backed-up";
            WriteText(backup / "marker.txt", "backed-up-last-good");
            WriteText(request.OutputDirectory / "marker.txt", "unvalidated-candidate");
            WriteRecoveryJournal(request.OutputDirectory, "backed_up", backup);
            FakeBuildTools tools;
            tools.FailPublishMove = true;

            const BuildPipelineReport report = BuildPipelineTestAccess::Create(tools.Operations()).Run(request);

            CHECK_FALSE(report.Succeeded());
            CHECK(report.Find(BuildPipelineStage::Publish)->Diagnostics.ContainsCode("pipeline.publish.recovered"));
            CHECK(ReadText(request.OutputDirectory / "marker.txt") == "backed-up-last-good");
            CHECK(CountSiblingDirectories(request.OutputDirectory, "recovery-candidate") == 0);
        }

        SECTION("published keeps the validated output and cleans its exact backup")
        {
            TemporaryDirectory temporary;
            BuildPipelineRequest request = CreateRequest(temporary, temporary.Root / "Builds/Crownfall");
            const Path backup = request.OutputDirectory.parent_path() / ".Crownfall.previous-published";
            WriteText(backup / "marker.txt", "older-build");
            WriteText(request.OutputDirectory / "marker.txt", "validated-build");
            WriteRecoveryJournal(request.OutputDirectory, "published", backup);
            FakeBuildTools tools;
            tools.FailPublishMove = true;

            const BuildPipelineReport report = BuildPipelineTestAccess::Create(tools.Operations()).Run(request);

            CHECK_FALSE(report.Succeeded());
            CHECK(ReadText(request.OutputDirectory / "marker.txt") == "validated-build");
            CHECK_FALSE(fs::exists(backup));
        }
    }

    TEST_CASE("Build pipeline never guesses recovery without a valid journal", "[Build][Pipeline]")
    {
        SECTION("missing journal preserves the unmatched backup")
        {
            TemporaryDirectory temporary;
            BuildPipelineRequest request = CreateRequest(temporary, temporary.Root / "Builds/Crownfall");
            const Path backup = request.OutputDirectory.parent_path() / ".Crownfall.previous-orphan";
            WriteText(backup / "marker.txt", "orphaned-last-good");
            FakeBuildTools tools;

            const BuildPipelineReport report = BuildPipelineTestAccess::Create(tools.Operations()).Run(request);

            CHECK_FALSE(report.Succeeded());
            CHECK(report.Find(BuildPipelineStage::Publish)->Diagnostics.ContainsCode("pipeline.publish.recovery_journal_missing"));
            CHECK(ReadText(backup / "marker.txt") == "orphaned-last-good");
            CHECK_FALSE(fs::exists(request.OutputDirectory));
        }

        SECTION("invalid journal identity preserves every possible backup")
        {
            TemporaryDirectory temporary;
            BuildPipelineRequest request = CreateRequest(temporary, temporary.Root / "Builds/Crownfall");
            const Path backup = request.OutputDirectory.parent_path() / ".Crownfall.previous-preserved";
            WriteText(backup / "marker.txt", "preserved-last-good");
            WriteText(request.OutputDirectory.parent_path() / ".Crownfall.publish-journal", "phase=backed_up\nbackup=../untrusted\n");
            FakeBuildTools tools;

            const BuildPipelineReport report = BuildPipelineTestAccess::Create(tools.Operations()).Run(request);

            CHECK_FALSE(report.Succeeded());
            CHECK(report.Find(BuildPipelineStage::Publish)->Diagnostics.ContainsCode("pipeline.publish.recovery_journal_invalid"));
            CHECK(ReadText(backup / "marker.txt") == "preserved-last-good");
            CHECK_FALSE(fs::exists(request.OutputDirectory));
        }
    }
} // namespace Crowny
