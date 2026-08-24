#include <catch2/catch_test_macros.hpp>

#include "Crowny/Scripting/ManagedReload.h"

#include <fstream>

using namespace Crowny;

TEST_CASE("Managed reload debounce coalesces source events", "[Scripting][Reload]")
{
    ManagedReloadDebouncer debounce(std::chrono::milliseconds(500));
    const ManagedReloadDebouncer::TimePoint start{};

    debounce.Notify(start);
    debounce.Notify(start + std::chrono::milliseconds(100));
    CHECK_FALSE(debounce.TryBegin(start + std::chrono::milliseconds(599)));
    REQUIRE(debounce.TryBegin(start + std::chrono::milliseconds(600)));

    debounce.Notify(start + std::chrono::milliseconds(650));
    CHECK_FALSE(debounce.TryBegin(start + std::chrono::milliseconds(1200)));
    debounce.Complete();
    REQUIRE(debounce.TryBegin(start + std::chrono::milliseconds(1200)));
    CHECK(debounce.GetGeneration() == 3);
}

TEST_CASE("Mono runtime discovery prefers a complete candidate", "[Scripting][Reload]")
{
    const Path base =
      fs::temp_directory_path() / ("crowny-mono-paths-" + std::to_string(ManagedReloadDebouncer::Clock::now().time_since_epoch().count()));
    const Path incomplete = base / "incomplete";
    const Path complete = base / "complete";
    fs::create_directories(incomplete / "lib");
    fs::create_directories(complete / "lib");
    fs::create_directories(complete / "etc");
    fs::create_directories(complete / "bin");
    std::ofstream(complete / "bin/csc.bat") << "@exit /b 0\n";

    const MonoRuntimePaths paths = ResolveMonoRuntimePaths(Vector<Path>{ incomplete, complete });
    CHECK(paths.Root == fs::weakly_canonical(complete));
    CHECK(paths.HasRuntime());
    CHECK(paths.HasCompiler());
    fs::remove_all(base);
}

TEST_CASE("Managed assembly publishing replaces DLL and symbols", "[Scripting][Reload]")
{
    const Path base =
      fs::temp_directory_path() / ("crowny-assembly-stage-" + std::to_string(ManagedReloadDebouncer::Clock::now().time_since_epoch().count()));
    const Path staged = base / "stage/GameAssembly.dll";
    const Path destination = base / "current/GameAssembly.dll";
    fs::create_directories(staged.parent_path());
    fs::create_directories(destination.parent_path());
    std::ofstream(staged, std::ios::binary) << "new-dll";
    std::ofstream(Path(staged).replace_extension("pdb"), std::ios::binary) << "new-pdb";
    std::ofstream(destination, std::ios::binary) << "old-dll";

    String error;
    REQUIRE(PublishManagedAssembly(staged, destination, &error));
    {
        std::ifstream dll(destination, std::ios::binary);
        std::ifstream pdb(Path(destination).replace_extension("pdb"), std::ios::binary);
        CHECK(String(std::istreambuf_iterator<char>(dll), std::istreambuf_iterator<char>()) == "new-dll");
        CHECK(String(std::istreambuf_iterator<char>(pdb), std::istreambuf_iterator<char>()) == "new-pdb");
    }
    fs::remove_all(base);
}
