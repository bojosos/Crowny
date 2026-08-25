#include <catch2/catch_test_macros.hpp>

#include "Crowny/Build/ContentPack.h"
#include "Crowny/Build/PlayerTemplate.h"

#include <fstream>

#ifdef CW_PLATFORM_WIN32
#include <Windows.h>
#endif

namespace Crowny
{
    namespace
    {
        class TemplateTestDirectory
        {
        public:
            TemplateTestDirectory()
            {
                Root = fs::temp_directory_path() / ("crowny-template-security-" + UuidGenerator::Generate().ToString());
                fs::create_directories(Root);
            }

            ~TemplateTestDirectory()
            {
                std::error_code error;
                fs::remove_all(Root, error);
            }

            Path Root;
        };

        void WriteTemplateFile(const Path& path, StringView contents)
        {
            fs::create_directories(path.parent_path());
            std::ofstream stream(path, std::ios::binary | std::ios::trunc);
            stream.write(contents.data(), static_cast<std::streamsize>(contents.size()));
        }

        String ReadTemplateFile(const Path& path)
        {
            std::ifstream stream(path, std::ios::binary);
            return String(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
        }

        PlayerTemplateManifest MakeTemplateManifest(const Path& root, const Vector<Path>& paths)
        {
            PlayerTemplateManifest manifest;
            manifest.EngineVersion = "0.1.0";
            manifest.Platform = BuildPlatform::WindowsX64;
            manifest.Configuration = BuildConfiguration::Development;
            manifest.Renderers = { RendererBackend::Vulkan, RendererBackend::OpenGL };
            for (const Path& path : paths)
                manifest.Files.push_back({ path, ComputeFileSha256(root / path), false });
            return manifest;
        }

        PlayerTemplateRequest MakeTemplateRequest()
        {
            PlayerTemplateRequest request;
            request.EngineVersion = "0.1.0";
            request.Platform = BuildPlatform::WindowsX64;
            request.Configuration = BuildConfiguration::Development;
            return request;
        }
    } // namespace

    TEST_CASE("Build paths reject parent traversal before normalization", "[Build][Security]")
    {
        CHECK(IsSafeRelativeBuildPath("Data/player.bin"));
        CHECK_FALSE(IsSafeRelativeBuildPath(Path("Data") / ".." / "player.bin"));
        CHECK_FALSE(IsSafeRelativeBuildPath(Path("Data") / "Nested" / ".." / "player.bin"));
        CHECK_FALSE(IsSafeRelativeBuildPath("../player.bin"));
        CHECK_FALSE(IsSafeRelativeBuildPath("Data\\..\\player.bin"));
        CHECK_FALSE(IsSafeRelativeBuildPath("C:\\Crowny\\player.bin"));
        CHECK_FALSE(IsSafeRelativeBuildPath("\\\\server\\templates\\player.bin"));
        CHECK_FALSE(IsSafeRelativeBuildPath(fs::temp_directory_path() / "player.bin"));
    }

    TEST_CASE("Player template validation rejects duplicate portable paths", "[Build][Security]")
    {
        TemplateTestDirectory temporary;
        WriteTemplateFile(temporary.Root / "Data" / "Player.bin", "player");
        PlayerTemplateManifest manifest = MakeTemplateManifest(temporary.Root, { "Data/Player.bin" });
        manifest.Files.push_back(manifest.Files.front());

        const BuildValidation validation = ValidatePlayerTemplate(temporary.Root, manifest, MakeTemplateRequest());
        CHECK(validation.ContainsCode("template.file.path_duplicate"));
    }

    TEST_CASE("Player template validation rejects case-only path collisions", "[Build][Security]")
    {
        TemplateTestDirectory temporary;
        WriteTemplateFile(temporary.Root / "Data" / "Player.bin", "player");
        PlayerTemplateManifest manifest = MakeTemplateManifest(temporary.Root, { "Data/Player.bin" });
        manifest.Files.push_back({ "data/player.bin", manifest.Files.front().Sha256, false });

        const BuildValidation validation = ValidatePlayerTemplate(temporary.Root, manifest, MakeTemplateRequest());
        CHECK(validation.ContainsCode("template.file.path_case_collision"));
    }

    TEST_CASE("Player template validation requires declared hashes", "[Build][Security]")
    {
        TemplateTestDirectory temporary;
        WriteTemplateFile(temporary.Root / "Player.bin", "player");
        PlayerTemplateManifest manifest = MakeTemplateManifest(temporary.Root, { "Player.bin" });
        manifest.Files.front().Sha256.clear();

        const BuildValidation validation = ValidatePlayerTemplate(temporary.Root, manifest, MakeTemplateRequest());
        CHECK(validation.ContainsCode("template.file.hash_missing"));
    }

    TEST_CASE("Player template validation checks declared hash contents", "[Build][Security]")
    {
        TemplateTestDirectory temporary;
        WriteTemplateFile(temporary.Root / "Player.bin", "player");
        PlayerTemplateManifest manifest = MakeTemplateManifest(temporary.Root, { "Player.bin" });
        manifest.Files.front().Sha256 = String(64, '0');

        const BuildValidation validation = ValidatePlayerTemplate(temporary.Root, manifest, MakeTemplateRequest());
        CHECK(validation.ContainsCode("template.file.hash_mismatch"));
    }

#ifdef CW_PLATFORM_WIN32
    TEST_CASE("Player template creation rejects a file it cannot hash", "[Build][Security]")
    {
        TemplateTestDirectory temporary;
        const Path player = temporary.Root / "Player.bin";
        WriteTemplateFile(player, "player");
        const HANDLE lock = CreateFileW(player.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        REQUIRE(lock != INVALID_HANDLE_VALUE);

        PlayerTemplateManifest output;
        const String error = PlayerTemplateStore::CreateManifest(temporary.Root, {}, {}, output);
        CloseHandle(lock);

        CHECK_FALSE(error.empty());
        CHECK(output.Files.empty());
    }
#endif

    TEST_CASE("Player template staging replaces the complete destination", "[Build][Security]")
    {
        TemplateTestDirectory temporary;
        const Path source = temporary.Root / "Source";
        const Path stage = temporary.Root / "Player";
        WriteTemplateFile(source / "Data" / "Current.bin", "current");
        WriteTemplateFile(stage / "Data" / "Stale.bin", "stale");
        const PlayerTemplateManifest manifest = MakeTemplateManifest(source, { "Data/Current.bin" });

        REQUIRE(StagePlayerTemplate(source, manifest, stage).empty());
        CHECK(ReadTemplateFile(stage / "Data" / "Current.bin") == "current");
        CHECK_FALSE(fs::exists(stage / "Data" / "Stale.bin"));
    }

    TEST_CASE("Player template staging does not erase a destination for an empty manifest", "[Build][Security]")
    {
        TemplateTestDirectory temporary;
        const Path stage = temporary.Root / "Player";
        WriteTemplateFile(stage / "Existing.bin", "preserved");

        CHECK_FALSE(StagePlayerTemplate(temporary.Root / "Source", {}, stage).empty());
        CHECK(ReadTemplateFile(stage / "Existing.bin") == "preserved");
    }

    TEST_CASE("Player template staging preserves the destination when a source hash changes", "[Build][Security]")
    {
        TemplateTestDirectory temporary;
        const Path source = temporary.Root / "Source";
        const Path stage = temporary.Root / "Player";
        WriteTemplateFile(source / "Player.bin", "expected");
        const PlayerTemplateManifest manifest = MakeTemplateManifest(source, { "Player.bin" });
        WriteTemplateFile(source / "Player.bin", "changed");
        WriteTemplateFile(stage / "Existing.bin", "preserved");

        CHECK_FALSE(StagePlayerTemplate(source, manifest, stage).empty());
        CHECK(ReadTemplateFile(stage / "Existing.bin") == "preserved");
        CHECK_FALSE(fs::exists(stage / "Player.bin"));
    }

    TEST_CASE("Player template staging rejects overlapping source and destination trees", "[Build][Security]")
    {
        TemplateTestDirectory temporary;
        const Path source = temporary.Root / "Source";
        WriteTemplateFile(source / "Player.bin", "player");
        const PlayerTemplateManifest manifest = MakeTemplateManifest(source, { "Player.bin" });

        CHECK_FALSE(StagePlayerTemplate(source, manifest, source).empty());
        CHECK_FALSE(StagePlayerTemplate(source, manifest, source / "NestedOutput").empty());
        CHECK_FALSE(StagePlayerTemplate(source, manifest, temporary.Root).empty());
        CHECK(ReadTemplateFile(source / "Player.bin") == "player");
    }
} // namespace Crowny
