#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>

#include "cwpch.h"
#include "Crowny/Common/VirtualFileSystem.h"

using namespace Crowny;
namespace fs = std::filesystem;

TEST_CASE("VirtualFileSystem", "[Common][VFS]")
{
    VirtualFileSystem vfs;
    
    // Setup temporary physical directories
    fs::path tempBase = fs::temp_directory_path() / "CrownyVFSTest";
    fs::path physPath1 = tempBase / "Phys1";
    fs::path physPath2 = tempBase / "Phys2";
    
    fs::create_directories(physPath1);
    fs::create_directories(physPath2);

    auto WriteFile = [](const fs::path& path, const std::string& content) {
        std::ofstream f(path);
        f << content;
    };

    SECTION("Path Normalization")
    {
        vfs.Mount("/assets", physPath1.string());
        WriteFile(physPath1 / "test.txt", "content");

        String out;
        // Test /./ and /../
        CHECK(vfs.ResolvePhyiscalPath("/assets/./test.txt", out));
        CHECK(out == (physPath1 / "test.txt").generic_string());

        CHECK(vfs.ResolvePhyiscalPath("/assets/sub/../test.txt", out));
        CHECK(out == (physPath1 / "test.txt").generic_string());
    }

    SECTION("Priority Overlays")
    {
        // Mount two physical paths to the same virtual path
        vfs.Mount("/data", physPath1.string());
        vfs.Mount("/data", physPath2.string());

        // Create file only in Phys2 (lower priority)
        WriteFile(physPath2 / "unique2.txt", "content2");
        
        String out;
        CHECK(vfs.ResolvePhyiscalPath("/data/unique2.txt", out));
        CHECK(out == (physPath2 / "unique2.txt").generic_string());

        // Create file in both - should pick Phys1 (higher priority)
        WriteFile(physPath1 / "both.txt", "content1");
        WriteFile(physPath2 / "both.txt", "content2");

        CHECK(vfs.ResolvePhyiscalPath("/data/both.txt", out));
        CHECK(out == (physPath1 / "both.txt").generic_string());
        
        CHECK(vfs.ReadTextFile("/data/both.txt") == "content1");
    }

    SECTION("Writing always uses highest priority mount")
    {
        vfs.Mount("/data", physPath1.string());
        vfs.Mount("/data", physPath2.string());

        vfs.WriteTextFile("/data/newfile.txt", "newcontent");
        
        CHECK(fs::exists(physPath1 / "newfile.txt"));
        CHECK_FALSE(fs::exists(physPath2 / "newfile.txt"));
    }

    SECTION("Deepest match wins")
    {
        vfs.Mount("/assets", physPath1.string());
        vfs.Mount("/assets/scripts", physPath2.string());

        WriteFile(physPath2 / "main.lua", "content");
        fs::create_directories(physPath1 / "textures");
        WriteFile(physPath1 / "textures" / "logo.png", "content");

        String out;
        // Should match /assets/scripts, not /assets
        CHECK(vfs.ResolvePhyiscalPath("/assets/scripts/main.lua", out));
        CHECK(out == (physPath2 / "main.lua").generic_string());

        CHECK(vfs.ResolvePhyiscalPath("/assets/textures/logo.png", out));
        CHECK(out == (physPath1 / "textures/logo.png").generic_string());
    }

    SECTION("Unmount removes mount point")
    {
        vfs.Mount("/assets", physPath1.string());
        WriteFile(physPath1 / "file.txt", "content");

        String out;
        CHECK(vfs.ResolvePhyiscalPath("/assets/file.txt", out));

        vfs.Unmount("/assets");
        CHECK_FALSE(vfs.ResolvePhyiscalPath("/assets/file.txt", out));
    }

    SECTION("Non-existent virtual path returns false")
    {
        String out;
        CHECK_FALSE(vfs.ResolvePhyiscalPath("/nowhere/file.txt", out));
    }

    SECTION("Backslash normalization")
    {
        vfs.Mount("/assets", physPath1.string());
        WriteFile(physPath1 / "file.txt", "content");

        String out;
        CHECK(vfs.ResolvePhyiscalPath("\\assets\\file.txt", out));
        CHECK(out == (physPath1 / "file.txt").generic_string());
    }

    // Cleanup
    fs::remove_all(tempBase);
}
