#include <catch2/catch_test_macros.hpp>

#include "Crowny/Build/ManagedBuild.h"

#include <chrono>
#include <fstream>
#include <iterator>

using namespace Crowny;

namespace
{
    struct TemporaryDirectory
    {
        TemporaryDirectory()
        {
            Path base = fs::temp_directory_path() / "crowny-managed-build-tests";
            fs::create_directories(base);
            Root = base / std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
            fs::create_directories(Root);
        }

        ~TemporaryDirectory()
        {
            std::error_code error;
            fs::remove_all(Root, error);
        }

        Path Root;
    };

    void WriteFile(const Path& path, StringView contents)
    {
        fs::create_directories(path.parent_path());
        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
        stream.write(contents.data(), static_cast<std::streamsize>(contents.size()));
    }

    String ReadText(const Path& path)
    {
        std::ifstream stream(path, std::ios::binary);
        return String(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
    }

    String CanonicalPathString(const Path& path)
    {
        std::error_code error;
        const Path canonical = fs::weakly_canonical(fs::absolute(path), error);
        return (error ? fs::absolute(path).lexically_normal() : canonical).string();
    }

    String DiagnosticsText(const Vector<ManagedBuildDiagnostic>& diagnostics)
    {
        String text;
        for (const ManagedBuildDiagnostic& diagnostic : diagnostics)
        {
            if (!text.empty())
                text += "\n";
            text += diagnostic.Code + ": " + diagnostic.Message + " [" + diagnostic.Subject.string() + "]";
        }
        return text;
    }

    ManagedToolchain MakeToolchain(const TemporaryDirectory& temporary, const Path& compiler, String version = "test-compiler")
    {
        ManagedToolchain toolchain;
        toolchain.CompilerAssembly = compiler;
        toolchain.ReferenceDirectory = temporary.Root / "Framework";
        toolchain.Version = std::move(version);
        WriteFile(toolchain.ReferenceDirectory / "mscorlib.dll", "framework-mscorlib");
        WriteFile(toolchain.ReferenceDirectory / "System.dll", "framework-system");
        return toolchain;
    }
} // namespace

TEST_CASE("Managed build plans are deterministic and content addressed", "[Build][Managed]")
{
    TemporaryDirectory temporary;
    const Path project = temporary.Root / "Project with spaces";
    const Path sourceA = project / "Assets/Zeta.cs";
    const Path sourceB = project / "Assets/Alpha.cs";
    const Path referenceA = temporary.Root / "References/Zeta.dll";
    const Path referenceB = temporary.Root / "References/Alpha.dll";
    const Path compiler = temporary.Root / "Tools/Roslyn/csc.exe";
    WriteFile(sourceA, "class Zeta {}\n");
    WriteFile(sourceB, "class Alpha {}\n");
    WriteFile(referenceA, "reference-zeta");
    WriteFile(referenceB, "reference-alpha");
    WriteFile(compiler, "compiler-v1");

    ManagedToolchain toolchain = MakeToolchain(temporary, compiler, "roslyn-test-1");

    ManagedBuildRequest first;
    first.ProjectRoot = project;
    first.OutputAssembly = project / "Internal/Assemblies/Game Assembly.dll";
    first.Sources = { sourceA, sourceB };
    first.References = { referenceA, referenceB };
    first.Symbols = { "PLAYTEST", "CROWNY_WINDOWS", "PLAYTEST" };
    first.Configuration = BuildConfiguration::Development;

    ManagedBuildRequest reordered = first;
    reordered.Sources = { sourceB, sourceA };
    reordered.References = { referenceB, referenceA };
    reordered.Symbols = { "CROWNY_WINDOWS", "PLAYTEST" };

    const ManagedBuildPlan firstPlan = CreateManagedBuildPlan(first, toolchain);
    const ManagedBuildPlan reorderedPlan = CreateManagedBuildPlan(reordered, toolchain);
    REQUIRE(firstPlan.IsValid());
    REQUIRE(reorderedPlan.IsValid());
    CHECK(firstPlan.CompilerArguments == reorderedPlan.CompilerArguments);
    CHECK(firstPlan.CacheKey == reorderedPlan.CacheKey);
    CHECK(firstPlan.CacheKey.size() == 64);
    CHECK(firstPlan.CompilerArguments.front() == "/noconfig");
    CHECK(std::find(firstPlan.CompilerArguments.begin(), firstPlan.CompilerArguments.end(), "/deterministic+") != firstPlan.CompilerArguments.end());
    CHECK(std::find(firstPlan.CompilerArguments.begin(), firstPlan.CompilerArguments.end(), "/define:CROWNY_DEVELOPMENT;CROWNY_WINDOWS;PLAYTEST") !=
          firstPlan.CompilerArguments.end());
    CHECK(std::find(firstPlan.CompilerArguments.begin(), firstPlan.CompilerArguments.end(),
                    "/reference:" + CanonicalPathString(toolchain.ReferenceDirectory / "mscorlib.dll")) != firstPlan.CompilerArguments.end());
    CHECK(firstPlan.CompilerArguments[firstPlan.CompilerArguments.size() - 2] == CanonicalPathString(sourceB));
    CHECK(firstPlan.CompilerArguments.back() == CanonicalPathString(sourceA));

    WriteFile(sourceA, "class Zeta { int Changed; }\n");
    const ManagedBuildPlan changedPlan = CreateManagedBuildPlan(first, toolchain);
    REQUIRE(changedPlan.IsValid());
    CHECK(changedPlan.CacheKey != firstPlan.CacheKey);
}

TEST_CASE("Managed build plans reject paths that escape the project", "[Build][Managed]")
{
    TemporaryDirectory temporary;
    const Path project = temporary.Root / "Project";
    const Path compiler = temporary.Root / "Tools/csc.exe";
    const Path outsideSource = temporary.Root / "Outside.cs";
    WriteFile(compiler, "compiler");
    WriteFile(outsideSource, "class Outside {}\n");
    fs::create_directories(project);

    ManagedToolchain toolchain = MakeToolchain(temporary, compiler, "test");
    ManagedBuildRequest request;
    request.ProjectRoot = project;
    request.OutputAssembly = temporary.Root / "escaped.dll";
    request.Sources = { outsideSource };

    const ManagedBuildPlan plan = CreateManagedBuildPlan(request, toolchain);
    REQUIRE_FALSE(plan.IsValid());
    CHECK(plan.CacheKey.empty());
    CHECK(std::any_of(plan.Diagnostics.begin(), plan.Diagnostics.end(),
                      [](const ManagedBuildDiagnostic& diagnostic) { return diagnostic.Code == "MB401"; }));
    CHECK(std::any_of(plan.Diagnostics.begin(), plan.Diagnostics.end(),
                      [](const ManagedBuildDiagnostic& diagnostic) { return diagnostic.Code == "MB406"; }));
}

TEST_CASE("Managed build plans include reference and compiler content", "[Build][Managed]")
{
    TemporaryDirectory temporary;
    const Path project = temporary.Root / "Project";
    const Path source = project / "Game.cs";
    const Path reference = temporary.Root / "References/Engine.dll";
    const Path compiler = temporary.Root / "Tools/csc.exe";
    WriteFile(source, "class Game {}\n");
    WriteFile(reference, "engine-v1");
    WriteFile(compiler, "compiler-v1");

    ManagedToolchain toolchain = MakeToolchain(temporary, compiler);
    ManagedBuildRequest request;
    request.ProjectRoot = project;
    request.OutputAssembly = project / "Game.dll";
    request.Sources = { source };
    request.References = { reference };

    const String initial = CreateManagedBuildPlan(request, toolchain).CacheKey;
    REQUIRE(initial.size() == 64);
    WriteFile(reference, "engine-v2");
    const String referenceChanged = CreateManagedBuildPlan(request, toolchain).CacheKey;
    CHECK(referenceChanged != initial);
    WriteFile(reference, "engine-v1");
    WriteFile(compiler, "compiler-v2");
    CHECK(CreateManagedBuildPlan(request, toolchain).CacheKey != initial);
    WriteFile(compiler, "compiler-v1");
    WriteFile(toolchain.ReferenceDirectory / "System.dll", "framework-system-v2");
    CHECK(CreateManagedBuildPlan(request, toolchain).CacheKey != initial);
}

TEST_CASE("Managed assembly inspection fails closed for non PE inputs", "[Build][Managed]")
{
    TemporaryDirectory temporary;
    const Path assembly = temporary.Root / "NotManaged.dll";
    WriteFile(assembly, "not a portable executable");

    const ManagedAssemblyInspection inspection = InspectManagedAssembly(assembly);
    CHECK_FALSE(inspection.IsPureManaged());
    REQUIRE_FALSE(inspection.Diagnostics.empty());
    CHECK(inspection.Diagnostics.front().Code == "MB201");
}

TEST_CASE("Managed dependency resolution reports missing roots", "[Build][Managed]")
{
    TemporaryDirectory temporary;
    ManagedDependencyRequest request;
    request.Roots = { temporary.Root / "Missing.dll" };

    const ManagedDependencyResult result = ResolveManagedDependencyClosure(request);
    CHECK_FALSE(result.Succeeded());
    CHECK(result.Assemblies.empty());
    REQUIRE_FALSE(result.Diagnostics.empty());
    CHECK(result.Diagnostics.front().Code == "MB200");
}

TEST_CASE("Managed assembly identities use CLI display form", "[Build][Managed]")
{
    ManagedAssemblyIdentity identity;
    identity.Name = "Crowny-Sharp";
    identity.Major = 1;
    identity.Minor = 2;
    identity.Build = 3;
    identity.Revision = 4;
    identity.Culture = "en-US";
    identity.PublicKeyToken = "0011223344556677";
    CHECK(identity.ToString() == "Crowny-Sharp, Version=1.2.3.4, Culture=en-US, PublicKeyToken=0011223344556677");
}

TEST_CASE("Managed compilation preserves the last published assembly when process startup fails", "[Build][Managed]")
{
    TemporaryDirectory temporary;
    const Path project = temporary.Root / "Project";
    const Path source = project / "Game.cs";
    const Path output = project / "Game.dll";
    const Path compiler = temporary.Root / "Tools/csc.exe";
    WriteFile(source, "class Game {}\n");
    WriteFile(output, "last-known-good");
    WriteFile(compiler, "not an executable");

    const ManagedToolchain toolchain = MakeToolchain(temporary, compiler);
    ManagedBuildRequest request;
    request.ProjectRoot = project;
    request.OutputAssembly = output;
    request.Sources = { source };
    request.Timeout = std::chrono::seconds(2);
    request.MaxCapturedOutputBytes = 4096;

    const ManagedCompileResult result = CompileManagedAssembly(request, toolchain);
    CHECK_FALSE(result.Succeeded());
    CHECK(ReadText(output) == "last-known-good");
    CHECK_FALSE(fs::exists(project / "Game.pdb"));
}

TEST_CASE("Managed build compiles and inspects a real assembly when Mono is available", "[Build][Managed][Integration]")
{
    const ManagedToolchain toolchain = LocateManagedToolchain({});
    if (!toolchain.IsValid())
    {
        WARN("Skipping Mono integration because no managed toolchain is available.");
        return;
    }

    TemporaryDirectory temporary;
    const Path project = temporary.Root / "Project";
    const Path source = project / "HelloWorld.cs";
    const Path output = project / "GameAssembly.dll";
    WriteFile(source, "public static class HelloWorld { public static string Message => \"hello\"; }\n");

    ManagedBuildRequest request;
    request.ProjectRoot = project;
    request.OutputAssembly = output;
    request.Sources = { source };
    request.Timeout = std::chrono::seconds(30);
    request.MaxCapturedOutputBytes = 256 * 1024;

    const ManagedCompileResult compilation = CompileManagedAssembly(request, toolchain);
    INFO(compilation.StandardOutput);
    INFO(compilation.StandardError);
    INFO(DiagnosticsText(compilation.Diagnostics));
    REQUIRE(compilation.Succeeded());
    REQUIRE(fs::is_regular_file(output));

    const ManagedAssemblyInspection inspection = InspectManagedAssembly(output);
    INFO(DiagnosticsText(inspection.Diagnostics));
    CHECK(inspection.IsPureManaged());
    CHECK(inspection.Identity.Name == "GameAssembly");
    CHECK_FALSE(inspection.References.empty());

    ManagedDependencyRequest dependencyRequest;
    dependencyRequest.Roots = { output };
    dependencyRequest.FrameworkDirectories = { toolchain.ReferenceDirectory };
    const ManagedDependencyResult dependencies = ResolveManagedDependencyClosure(dependencyRequest);
    INFO(DiagnosticsText(dependencies.Diagnostics));
    CHECK(dependencies.Succeeded());
    CHECK(std::find(dependencies.Assemblies.begin(), dependencies.Assemblies.end(), fs::weakly_canonical(output)) != dependencies.Assemblies.end());
}
